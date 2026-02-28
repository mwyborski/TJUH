/*
 * TJUH — Tiny Joystick USB Host
 *
 * USB Host library for game controllers on RP2040 using TinyUSB bare API.
 *
 * Supported controllers:
 *   - Sony DualShock 4 (CUH-ZCT2x)  VID=0x054c PID=0x09cc / 0x05c4
 *   - Sony DualSense (PS5)           VID=0x054c PID=0x0ce6
 *   - Xbox 360 Wired
 *   - Generic HID gamepads (8-byte and 3-byte reports)
 *
 * MIT License — see LICENSE
 */

#ifndef TJUH_H
#define TJUH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TJUH_MAX_DEVICES
#define TJUH_MAX_DEVICES 2
#endif

/* -------------------------------------------------------------------------- */
/*  Gamepad report — unified across all supported controllers                 */
/* -------------------------------------------------------------------------- */

typedef struct __attribute__((packed))
{
    union {
        struct {
            uint8_t x;
            uint8_t y;
            uint8_t z;
            uint8_t rz;
        };
        uint32_t axes_bytes;
    };

    union {
        struct {
            uint8_t dpad     : 4;  /* hat: 0=N 1=NE 2=E 3=SE 4=S 5=SW 6=W 7=NW 8=released */
            uint8_t square   : 1;  /* west  */
            uint8_t cross    : 1;  /* south */
            uint8_t circle   : 1;  /* east  */
            uint8_t triangle : 1;  /* north */
        };
        uint8_t dpad_buttons_byte;
    };

    union {
        struct {
            uint8_t l1     : 1;
            uint8_t r1     : 1;
            uint8_t l2     : 1;
            uint8_t r2     : 1;
            uint8_t select : 1;
            uint8_t start  : 1;
            uint8_t l3     : 1;
            uint8_t r3     : 1;
        };
        uint8_t trigger_buttons_byte;
    };

    union {
        struct {
            uint8_t system        : 1;
            uint8_t extra         : 1;
            uint8_t reserved_bits : 6;
        };
        uint8_t extra_buttons_byte;
    };

    uint8_t reserved_byte;
} tjuh_gamepad_report_t;

/* -------------------------------------------------------------------------- */
/*  Callback types                                                            */
/* -------------------------------------------------------------------------- */

/**
 * Called when a valid gamepad report has been received and parsed.
 *
 * @param dev_addr  TinyUSB device address (1-based)
 * @param report    Parsed gamepad state
 */
typedef void (*tjuh_report_cb_t)(uint8_t dev_addr, const tjuh_gamepad_report_t *report);

/**
 * Called when a gamepad device is connected.
 *
 * @param dev_addr  TinyUSB device address
 * @param vid       USB Vendor ID
 * @param pid       USB Product ID
 */
typedef void (*tjuh_connect_cb_t)(uint8_t dev_addr, uint16_t vid, uint16_t pid);

/**
 * Called when a gamepad device is disconnected.
 *
 * @param dev_addr  TinyUSB device address
 */
typedef void (*tjuh_disconnect_cb_t)(uint8_t dev_addr);

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

typedef struct {
    tjuh_report_cb_t     on_report;
    tjuh_connect_cb_t    on_connect;
    tjuh_disconnect_cb_t on_disconnect;
} tjuh_config_t;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

/**
 * Initialize the USB host stack and register callbacks.
 * Call once before the main loop.
 */
void tjuh_init(const tjuh_config_t *config);

/**
 * Query VID/PID for a connected device.
 *
 * @return true if the device is connected and info is available.
 */
bool tjuh_get_device_info(uint8_t dev_addr, uint16_t *vid, uint16_t *pid);

/* -------------------------------------------------------------------------- */
/*  Debug utilities                                                           */
/* -------------------------------------------------------------------------- */

void tjuh_print_report(const tjuh_gamepad_report_t *report);
void tjuh_print_raw(const uint8_t *buf, uint16_t len, uint16_t max_ep_size);

#ifdef __cplusplus
}
#endif

#endif /* TJUH_H */
