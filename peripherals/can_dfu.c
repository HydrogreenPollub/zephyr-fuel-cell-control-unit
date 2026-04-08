#include "can_dfu.h"

#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/storage/stream_flash.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_dfu, LOG_LEVEL_INF);

#define DFU_THREAD_STACK_SIZE 16384
#define DFU_THREAD_PRIORITY   5

/* Each DATA frame carries a 2-byte sequence number + 6 bytes of payload */
#define DFU_DATA_PAYLOAD_LEN 6U

#define DFU_IDLE_TIMEOUT_S 5

K_THREAD_STACK_DEFINE(dfu_thread_stack, DFU_THREAD_STACK_SIZE);
static struct k_thread dfu_thread_data;

K_MSGQ_DEFINE(dfu_frame_msgq, sizeof(struct can_frame), 64, 4);

typedef enum {
    DFU_STATE_IDLE,
    DFU_STATE_RECEIVING,
} dfu_state_t;

static const struct can_dfu_cfg *g_cfg;
static dfu_state_t dfu_state = DFU_STATE_IDLE;
static volatile bool dfu_active = false;
static struct flash_img_context dfu_ctx;
static uint32_t dfu_image_size;
static uint32_t dfu_bytes_written;
static uint16_t dfu_next_seq;
static uint32_t dfu_crc_accum;

bool can_dfu_is_active(void) { return dfu_active; }

static void dfu_send_status(uint8_t status)
{
    struct can_frame rsp = {
        .id    = g_cfg->rsp_id,
        .dlc   = 1,
        .flags = CAN_FRAME_IDE,
    };
    rsp.data[0] = status;
    /* Send directly from the DFU thread — blocks at most ~1 ms at 500 kbps */
    can_send(g_cfg->can_dev, &rsp, K_MSEC(100), NULL, NULL);
}

static void dfu_abort(void)
{
    dfu_state = DFU_STATE_IDLE;
    dfu_active = false;
    dfu_image_size = 0;
    dfu_bytes_written = 0;
    dfu_next_seq = 0;
    dfu_crc_accum = 0;
    if (dfu_ctx.flash_area != NULL) {
        flash_area_close(dfu_ctx.flash_area);
        dfu_ctx.flash_area = NULL;
    }
    if (g_cfg->on_end) {
        g_cfg->on_end();
    }
}

static void dfu_handle_request(const struct can_frame *frame)
{
    if (frame->dlc < 5) {
        return;
    }

    uint32_t image_size = sys_get_le32(&frame->data[1]);
    LOG_INF("DFU request received: image_size=%u B", image_size);

    int ret = flash_area_open(FIXED_PARTITION_ID(slot1_partition),
                              (const struct flash_area **)&dfu_ctx.flash_area);
    if (ret != 0) {
        LOG_ERR("flash_area_open failed: %d", ret);
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        return;
    }

    /* Pre-erase entire slot1 while the host is blocked waiting for REQUEST ACK.
     * On STM32G4 single-bank flash, each page erase freezes the AHB bus and
     * disables interrupts for ~25-40 ms; doing it here avoids stalls during
     * the data transfer that would overflow the 3-entry FDCAN hardware FIFO. */
    LOG_INF("DFU: erasing slot1 (%u B) — host blocked on REQUEST ACK",
            (unsigned)dfu_ctx.flash_area->fa_size);
    ret = flash_area_erase(dfu_ctx.flash_area, 0, dfu_ctx.flash_area->fa_size);
    if (ret != 0) {
        LOG_ERR("flash_area_erase failed: %d", ret);
        flash_area_close(dfu_ctx.flash_area);
        dfu_ctx.flash_area = NULL;
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        return;
    }
    LOG_INF("DFU: slot1 erased");

    const struct device *flash_dev = flash_area_get_device(dfu_ctx.flash_area);
    ret = stream_flash_init(&dfu_ctx.stream, flash_dev,
                            dfu_ctx.buf, sizeof(dfu_ctx.buf),
                            dfu_ctx.flash_area->fa_off,
                            dfu_ctx.flash_area->fa_size, NULL);
    if (ret != 0) {
        LOG_ERR("stream_flash_init failed: %d", ret);
        flash_area_close(dfu_ctx.flash_area);
        dfu_ctx.flash_area = NULL;
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        return;
    }

    dfu_image_size = image_size;
    dfu_bytes_written = 0;
    dfu_next_seq = 0;
    dfu_crc_accum = 0;
    dfu_active = true;
    dfu_state = DFU_STATE_RECEIVING;

    if (g_cfg->on_start) {
        g_cfg->on_start();
    }

    dfu_send_status(CAN_DFU_STATUS_OK);
    LOG_INF("DFU ready, awaiting %u data frames",
            (image_size + DFU_DATA_PAYLOAD_LEN - 1) / DFU_DATA_PAYLOAD_LEN);
}

static void dfu_handle_data(const struct can_frame *frame)
{
    if (dfu_state != DFU_STATE_RECEIVING) {
        return;
    }
    if (frame->dlc < 2) {
        return;
    }

    uint16_t seq = sys_get_le16(&frame->data[0]);

    if (seq == 0) {
        LOG_INF("DFU: first data frame received");
    }

    if (seq != dfu_next_seq) {
        LOG_ERR("DFU sequence error: expected %u, got %u", dfu_next_seq, seq);
        dfu_send_status(CAN_DFU_STATUS_SEQ_FAIL);
        dfu_abort();
        return;
    }

    uint32_t remaining = dfu_image_size - dfu_bytes_written;
    size_t data_len = MIN((size_t)(frame->dlc - 2), (size_t)remaining);
    const uint8_t *data = &frame->data[2];

    dfu_crc_accum = crc32_ieee_update(dfu_crc_accum, data, data_len);

    int ret = flash_img_buffered_write(&dfu_ctx, data, data_len, false);
    if (ret != 0) {
        LOG_ERR("flash write failed at seq=%u: %d", seq, ret);
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        dfu_abort();
        return;
    }

    dfu_bytes_written += data_len;
    dfu_next_seq++;

    /* Batch ACK every CAN_DFU_BATCH_SIZE frames so the host's gs_usb TX echo
     * pool does not exhaust and block python-can's send(). */
    if (dfu_next_seq % CAN_DFU_BATCH_SIZE == 0) {
        dfu_send_status(CAN_DFU_STATUS_OK);
        LOG_INF("DFU batch ACK %u (seq=%u written=%u B)",
                dfu_next_seq / CAN_DFU_BATCH_SIZE, dfu_next_seq, dfu_bytes_written);
    } else if (dfu_next_seq % 100 == 0) {
        LOG_INF("DFU progress: seq=%u written=%u B", dfu_next_seq, dfu_bytes_written);
    }
}

static void dfu_handle_commit(const struct can_frame *frame)
{
    LOG_INF("COMMIT received: state=%d written=%u", dfu_state, dfu_bytes_written);
    if (dfu_state != DFU_STATE_RECEIVING) {
        LOG_WRN("COMMIT ignored: not in receiving state");
        return;
    }
    if (frame->dlc < 5) {
        return;
    }

    uint32_t expected_crc = sys_get_le32(&frame->data[1]);
    LOG_INF("DFU commit: written=%u B, crc=0x%08X", dfu_bytes_written, dfu_crc_accum);

    int ret = flash_img_buffered_write(&dfu_ctx, NULL, 0, true);
    if (ret != 0) {
        LOG_ERR("flash flush failed: %d", ret);
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        dfu_abort();
        return;
    }

    if (dfu_crc_accum != expected_crc) {
        LOG_ERR("CRC mismatch: computed=0x%08X expected=0x%08X",
                dfu_crc_accum, expected_crc);
        dfu_send_status(CAN_DFU_STATUS_CRC_FAIL);
        dfu_abort();
        return;
    }

    ret = boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
    if (ret != 0) {
        LOG_ERR("boot_request_upgrade failed: %d", ret);
        dfu_send_status(CAN_DFU_STATUS_WRITE_FAIL);
        dfu_abort();
        return;
    }

    dfu_active = false;
    if (g_cfg->on_end) {
        g_cfg->on_end();
    }
    dfu_send_status(CAN_DFU_STATUS_OK);
    LOG_INF("DFU complete — rebooting in 1s");

    k_sleep(K_SECONDS(1));
    sys_reboot(SYS_REBOOT_COLD);
}

static void dfu_thread(void *p1, void *p2, void *p3)
{
    struct can_frame frame;
    LOG_INF("DFU thread started");

    while (1) {
        k_timeout_t wait = (dfu_state == DFU_STATE_RECEIVING)
                           ? K_SECONDS(DFU_IDLE_TIMEOUT_S)
                           : K_FOREVER;

        int ret = k_msgq_get(&dfu_frame_msgq, &frame, wait);
        if (ret == -EAGAIN) {
            LOG_WRN("DFU timeout: no frame for %d s (written=%u B, next_seq=%u) — aborting",
                    DFU_IDLE_TIMEOUT_S, dfu_bytes_written, dfu_next_seq);
            dfu_abort();
            continue;
        }

        if (frame.id == g_cfg->cmd_id) {
            if (frame.data[0] == CAN_DFU_CMD_REQUEST) {
                dfu_handle_request(&frame);
            } else if (frame.data[0] == CAN_DFU_CMD_COMMIT) {
                dfu_handle_commit(&frame);
            }
        } else if (frame.id == g_cfg->data_id) {
            dfu_handle_data(&frame);
        }
    }
}

void can_dfu_on_frame(const struct can_frame *frame)
{
    if (k_msgq_put(&dfu_frame_msgq, frame, K_NO_WAIT) != 0) {
        LOG_WRN("DFU frame queue full, dropping frame");
    }
}

void can_dfu_init(const struct can_dfu_cfg *cfg)
{
    g_cfg = cfg;

    k_tid_t tid = k_thread_create(
        &dfu_thread_data, dfu_thread_stack,
        K_THREAD_STACK_SIZEOF(dfu_thread_stack),
        dfu_thread, NULL, NULL, NULL,
        DFU_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "can_dfu");

    LOG_INF("DFU module initialized");
}
