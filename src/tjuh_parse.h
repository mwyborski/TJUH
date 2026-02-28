/*
 * TJUH — Tiny Joystick USB Host
 * Internal gamepad report parsing interface.
 */

#ifndef TJUH_PARSE_H
#define TJUH_PARSE_H

#include "tjuh.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TJUH_HINT_NONE       = 0,
    TJUH_HINT_XBOX_ONE   = 1,
    TJUH_HINT_SWITCH_PRO = 2,
} tjuh_hint_t;

/* Device registry */
bool tjuh_parse_init_device(uint8_t dev_addr, uint16_t vid, uint16_t pid);
bool tjuh_parse_free_device(uint8_t dev_addr);
bool tjuh_parse_get_vid_pid(uint8_t dev_addr, uint16_t *vid, uint16_t *pid);

/**
 * Parse a raw USB report into a unified gamepad report.
 *
 * @param dev_addr    TinyUSB device address
 * @param data        Raw report bytes
 * @param actual_len  Bytes received
 * @param max_ep_size Maximum endpoint packet size
 * @param report_out  Destination for parsed data
 * @param hint        Controller type hint from enumeration
 *
 * @return true if the report was successfully parsed.
 */
bool tjuh_parse_report(uint8_t dev_addr,
                       const uint8_t *data,
                       uint16_t actual_len,
                       uint16_t max_ep_size,
                       tjuh_gamepad_report_t *report_out,
                       tjuh_hint_t hint);

#ifdef __cplusplus
}
#endif

#endif /* TJUH_PARSE_H */
