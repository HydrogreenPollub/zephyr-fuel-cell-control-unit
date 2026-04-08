#ifndef PERIPHERALS_CAN_DFU_H
#define PERIPHERALS_CAN_DFU_H

#include <zephyr/drivers/can.h>
#include <stdbool.h>

/* Status bytes (byte 0 of a response frame) */
#define CAN_DFU_STATUS_OK         0x00U
#define CAN_DFU_STATUS_CRC_FAIL   0x01U
#define CAN_DFU_STATUS_WRITE_FAIL 0x02U
#define CAN_DFU_STATUS_SEQ_FAIL   0x03U

/* Command bytes (byte 0 of a command frame) */
#define CAN_DFU_CMD_REQUEST 0x01U
#define CAN_DFU_CMD_COMMIT  0xFFU

/* Device sends STATUS_OK every this many DATA frames so the host's gs_usb
 * TX-echo pool (64 slots on candlelight) does not exhaust. */
#define CAN_DFU_BATCH_SIZE 32U

struct can_dfu_cfg {
    uint32_t            cmd_id;   /* host → device: REQUEST or COMMIT  */
    uint32_t            data_id;  /* host → device: firmware chunks    */
    uint32_t            rsp_id;   /* device → host: status responses   */
    const struct device *can_dev; /* CAN bus to use for responses      */
    void (*on_start)(void);       /* called when a DFU session begins  */
    void (*on_end)(void);         /* called when a DFU session ends    */
};

void can_dfu_init(const struct can_dfu_cfg *cfg);
void can_dfu_on_frame(const struct can_frame *frame);
bool can_dfu_is_active(void);

#endif /* PERIPHERALS_CAN_DFU_H */
