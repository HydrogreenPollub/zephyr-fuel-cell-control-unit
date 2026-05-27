#ifndef FCCU_SETTINGS_H
#define FCCU_SETTINGS_H

#include <stddef.h>
#include <zephyr/settings/settings.h>

/**
 * @brief Zephyr Settings callback — restores one persisted key into the
 *        corresponding global variable on settings_load().
 *
 * Registered automatically via SETTINGS_STATIC_HANDLER_DEFINE; never call
 * directly.
 *
 * @param key    Setting key relative to the "fccu/" subtree.
 * @param len    Length of the stored value in bytes.
 * @param read_cb Callback to read the stored bytes into a buffer.
 * @param cb_arg Opaque argument forwarded to read_cb.
 * @return 0 on success, negative errno on read error.
 */
int fccu_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg);

/**
 * @brief Initialise the Settings subsystem and load all persisted values.
 *
 * Must be called once from fccu_init() after valves, fan, and purge globals
 * have their compile-time defaults set, so that flash values overwrite them.
 */
void fccu_settings_init();

/**
 * @brief Persist current purge and fan settings to NVS flash.
 *
 * Saves: purge mode, interval, duration, threshold; fan manual flag,
 * target temperature, and manual duty cycle. Call from the `settings save`
 * shell command.
 */
void fccu_settings_save();

#endif /* FCCU_SETTINGS_H */
