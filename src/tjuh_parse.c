/*
 * TJUH — Tiny Joystick USB Host
 * Controller-specific report parsing.
 */

#include "tjuh_parse.h"
#include <string.h>
#include <stdio.h>

#ifndef MIN
#define MIN(a, b) ((b) > (a) ? (a) : (b))
#endif

/* ---------------------------------------------------------------------- */
/*  Device registry                                                       */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t  dev_addr;
    uint16_t vid;
    uint16_t pid;
} tjuh_device_entry_t;

static tjuh_device_entry_t s_devices[TJUH_MAX_DEVICES];

bool tjuh_parse_init_device(uint8_t dev_addr, uint16_t vid, uint16_t pid)
{
    if (dev_addr == 0 || dev_addr > TJUH_MAX_DEVICES)
        return false;

    s_devices[dev_addr - 1].dev_addr = dev_addr;
    s_devices[dev_addr - 1].vid = vid;
    s_devices[dev_addr - 1].pid = pid;
    return true;
}

bool tjuh_parse_free_device(uint8_t dev_addr)
{
    if (dev_addr == 0 || dev_addr > TJUH_MAX_DEVICES)
        return false;

    memset(&s_devices[dev_addr - 1], 0, sizeof(s_devices[0]));
    return true;
}

bool tjuh_parse_get_vid_pid(uint8_t dev_addr, uint16_t *vid, uint16_t *pid)
{
    if (dev_addr == 0 || dev_addr > TJUH_MAX_DEVICES)
        return false;

    if (s_devices[dev_addr - 1].vid == 0)
        return false;

    *vid = s_devices[dev_addr - 1].vid;
    *pid = s_devices[dev_addr - 1].pid;
    return true;
}

/* ---------------------------------------------------------------------- */
/*  Axis conversion helpers                                               */
/* ---------------------------------------------------------------------- */

static inline uint8_t int16_to_uint8(int val)
{
    val += 0x8000;
    return (uint8_t)(val >> 8);
}

static inline uint8_t int16_to_uint8_inv(int val)
{
    val += 0x8000;
    return (uint8_t)(0xFF - (val >> 8));
}

/* ---------------------------------------------------------------------- */
/*  Xbox 360 parsing                                                      */
/* ---------------------------------------------------------------------- */

static void parse_xbox360_dpad_buttons(uint8_t byte, tjuh_gamepad_report_t *rpt)
{
    uint8_t dpad_bits = byte & 0x0F;

    switch (dpad_bits) {
        case 0x00: rpt->dpad = 8; break; /* released */
        case 0x01: rpt->dpad = 0; break; /* N        */
        case 0x09: rpt->dpad = 1; break; /* NE       */
        case 0x08: rpt->dpad = 2; break; /* E        */
        case 0x0A: rpt->dpad = 3; break; /* SE       */
        case 0x02: rpt->dpad = 4; break; /* S        */
        case 0x06: rpt->dpad = 5; break; /* SW       */
        case 0x04: rpt->dpad = 6; break; /* W        */
        case 0x05: rpt->dpad = 7; break; /* NW       */
        default:   rpt->dpad = 8; break;
    }

    rpt->start  = (byte & 0x10) != 0;
    rpt->select = (byte & 0x20) != 0;
    rpt->l3     = (byte & 0x40) != 0;
    rpt->r3     = (byte & 0x80) != 0;
}

static void parse_xbox360_buttons(uint8_t byte, tjuh_gamepad_report_t *rpt)
{
    rpt->l1       = (byte & 0x01) != 0;
    rpt->r1       = (byte & 0x02) != 0;
    rpt->system   = (byte & 0x04) != 0;
    rpt->cross    = (byte & 0x10) != 0;
    rpt->circle   = (byte & 0x20) != 0;
    rpt->square   = (byte & 0x40) != 0;
    rpt->triangle = (byte & 0x80) != 0;
}

static void parse_xbox360(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    (void)len;

    parse_xbox360_dpad_buttons(data[2], rpt);
    parse_xbox360_buttons(data[3], rpt);

    rpt->l2 = data[4] > 128;
    rpt->r2 = data[5] > 128;

    int16_t x_val;
    int16_t y_val;
    int16_t z_val;
    int16_t rz_val;
    memcpy(&x_val,  data + 6,  sizeof(int16_t));
    memcpy(&y_val,  data + 8,  sizeof(int16_t));
    memcpy(&z_val,  data + 10, sizeof(int16_t));
    memcpy(&rz_val, data + 12, sizeof(int16_t));

    rpt->x  = int16_to_uint8(x_val);
    rpt->y  = int16_to_uint8_inv(y_val);
    rpt->z  = int16_to_uint8(z_val);
    rpt->rz = int16_to_uint8_inv(rz_val);
}

/* ---------------------------------------------------------------------- */
/*  Sony DualSense (PS5) parsing                                          */
/* ---------------------------------------------------------------------- */

static void parse_sony_dualsense(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    (void)len;

    /* Skip report ID byte (0x01) */
    data++;

    memcpy(rpt, data, sizeof(rpt->axes_bytes));

    /* Skip 2 bytes analog triggers + 1 byte timestamp = 7 bytes offset from axes start */
    data += 7;

    rpt->dpad_buttons_byte    = *(data++);
    rpt->trigger_buttons_byte = *(data++);
    rpt->extra_buttons_byte   = *data;
}

/* ---------------------------------------------------------------------- */
/*  Sony DualShock 4 parsing                                              */
/* ---------------------------------------------------------------------- */

static void parse_sony_ds4(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    (void)len;

    /* Skip report ID byte (0x01) */
    data++;

    memcpy(rpt, data, sizeof(*rpt));
}

/* ---------------------------------------------------------------------- */
/*  Generic 8-byte gamepad                                                */
/* ---------------------------------------------------------------------- */

static void parse_generic_8byte(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    (void)len;

    rpt->rz = data[0];
    rpt->z  = data[1];
    rpt->x  = data[2];
    rpt->y  = data[3];

    /* data[4] is typically 0xFF (unused) */

    uint8_t bits = (data[5] >> 4) & 0x0F;
    rpt->triangle = (bits & 0x01) != 0;
    rpt->circle   = (bits & 0x02) != 0;
    rpt->cross    = (bits & 0x04) != 0;
    rpt->square   = (bits & 0x08) != 0;
    rpt->dpad     = (uint8_t)MIN(data[5] & 0x0F, 0x08);

    bits = (data[6] >> 4) & 0x0F;
    rpt->l3     = (bits & 0x01) != 0;
    rpt->r3     = (bits & 0x02) != 0;
    rpt->select = (bits & 0x04) != 0;
    rpt->start  = (bits & 0x08) != 0;

    bits = data[6] & 0x0F;
    rpt->l1 = (bits & 0x01) != 0;
    rpt->r1 = (bits & 0x02) != 0;
    rpt->l2 = (bits & 0x04) != 0;
    rpt->r2 = (bits & 0x08) != 0;
}

/* ---------------------------------------------------------------------- */
/*  Generic 3-byte gamepad (minimal: X, Y, buttons)                       */
/* ---------------------------------------------------------------------- */

static void parse_generic_3byte(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    (void)len;

    rpt->x = data[0];
    rpt->y = data[1];

    rpt->dpad_buttons_byte = (uint8_t)((data[2] << 4) | 0x08);
}

/* ---------------------------------------------------------------------- */
/*  Main dispatch                                                         */
/* ---------------------------------------------------------------------- */

bool tjuh_parse_report(uint8_t dev_addr,
                       const uint8_t *data,
                       uint16_t actual_len,
                       uint16_t max_ep_size,
                       tjuh_gamepad_report_t *report_out,
                       tjuh_hint_t hint)
{
    if (hint == TJUH_HINT_XBOX_ONE) {
        /* Xbox One support is incomplete — extend here when ready */
        return false;
    }

    /* hint == TJUH_HINT_NONE: detect by endpoint/report size */
    switch (max_ep_size) {
        case 8:
            if (actual_len == 8) {
                parse_generic_8byte(data, actual_len, report_out);
                return true;
            }
            if (actual_len == 3) {
                parse_generic_3byte(data, actual_len, report_out);
                return true;
            }
            break;

        case 32:
            if (actual_len == 20) {
                parse_xbox360(data, actual_len, report_out);
                return true;
            }
            break;

        case 64:
            if (data[0] == 0x01) {
                uint16_t pid = 0;
                if (dev_addr > 0 && dev_addr <= TJUH_MAX_DEVICES)
                    pid = s_devices[dev_addr - 1].pid;

                if (pid == 0x0ce6)
                    parse_sony_dualsense(data, actual_len, report_out);
                else
                    parse_sony_ds4(data, actual_len, report_out);

                return true;
            }
            break;

        default:
            break;
    }

    return false;
}
