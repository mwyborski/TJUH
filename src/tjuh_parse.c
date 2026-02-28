/*
 * TJUH — Tiny Joystick USB Host
 * Controller-specific report parsing.
 *
 * Dispatch priority:
 *   1. Hint-based (Xbox One, Switch Pro — set during enumeration)
 *   2. VID/PID-based (Sony, Nintendo)
 *   3. Endpoint size heuristic (Xbox 360, generic HID)
 */

#include "tjuh_parse.h"
#include <string.h>
#include <stdio.h>

#ifndef MIN
#define MIN(a, b) ((b) > (a) ? (a) : (b))
#endif

/* ---------------------------------------------------------------------- */
/*  Known vendor/product IDs                                              */
/* ---------------------------------------------------------------------- */

#define VID_SONY              0x054C
#define PID_DS4_V1            0x05C4   /* CUH-ZCT1 */
#define PID_DS4_V2            0x09CC   /* CUH-ZCT2 */
#define PID_DUALSENSE         0x0CE6
#define PID_DUALSENSE_EDGE    0x0DF2

#define VID_NINTENDO          0x057E
#define PID_SWITCH_PRO        0x2009
#define PID_JOYCON_L          0x2006
#define PID_JOYCON_R          0x2007

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

static bool get_vid_pid(uint8_t dev_addr, uint16_t *vid, uint16_t *pid)
{
    if (dev_addr == 0 || dev_addr > TJUH_MAX_DEVICES) {
        *vid = 0;
        *pid = 0;
        return false;
    }
    *vid = s_devices[dev_addr - 1].vid;
    *pid = s_devices[dev_addr - 1].pid;
    return (*vid != 0);
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
/*  Nintendo Switch Pro Controller — full report (0x30)                   */
/*                                                                        */
/*  Sent after USB init handshake (80 02, 80 04). Contains 12-bit packed  */
/*  stick axes and discrete direction buttons instead of a hat switch.    */
/*  Reference: dekuNukem/Nintendo_Switch_Reverse_Engineering              */
/* ---------------------------------------------------------------------- */

static void parse_switch_pro_full(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    if (len < 12)
        return;

    /* data[0] = 0x30 (report ID), data[1] = timer, data[2] = battery */

    uint8_t btn_r = data[3]; /* Y=0x01 X=0x02 B=0x04 A=0x08 R=0x40 ZR=0x80 */
    uint8_t btn_m = data[4]; /* -=0x01 +=0x02 RS=0x04 LS=0x08 Home=0x10 Cap=0x20 */
    uint8_t btn_l = data[5]; /* Dn=0x01 Up=0x02 Rt=0x04 Lt=0x08 L=0x40 ZL=0x80 */

    /* Map by physical face button position */
    rpt->cross    = (btn_r & 0x04) != 0; /* B  (south) */
    rpt->circle   = (btn_r & 0x08) != 0; /* A  (east)  */
    rpt->square   = (btn_r & 0x01) != 0; /* Y  (west)  */
    rpt->triangle = (btn_r & 0x02) != 0; /* X  (north) */

    rpt->r1  = (btn_r & 0x40) != 0;
    rpt->r2  = (btn_r & 0x80) != 0;
    rpt->l1  = (btn_l & 0x40) != 0;
    rpt->l2  = (btn_l & 0x80) != 0;

    rpt->select = (btn_m & 0x01) != 0;
    rpt->start  = (btn_m & 0x02) != 0;
    rpt->r3     = (btn_m & 0x04) != 0;
    rpt->l3     = (btn_m & 0x08) != 0;
    rpt->system = (btn_m & 0x10) != 0;
    rpt->extra  = (btn_m & 0x20) != 0;

    /* Synthesize hat direction from discrete buttons */
    bool up    = (btn_l & 0x02) != 0;
    bool down  = (btn_l & 0x01) != 0;
    bool left  = (btn_l & 0x08) != 0;
    bool right = (btn_l & 0x04) != 0;

    if      (up && right)    rpt->dpad = 1;
    else if (down && right)  rpt->dpad = 3;
    else if (down && left)   rpt->dpad = 5;
    else if (up && left)     rpt->dpad = 7;
    else if (up)             rpt->dpad = 0;
    else if (right)          rpt->dpad = 2;
    else if (down)           rpt->dpad = 4;
    else if (left)           rpt->dpad = 6;
    else                     rpt->dpad = 8;

    /* Left stick: 12-bit packed in bytes 6–8 */
    uint16_t lx = (uint16_t)(data[6] | ((data[7] & 0x0F) << 8));
    uint16_t ly = (uint16_t)((data[7] >> 4) | (data[8] << 4));

    /* Right stick: 12-bit packed in bytes 9–11 */
    uint16_t rx = (uint16_t)(data[9] | ((data[10] & 0x0F) << 8));
    uint16_t ry = (uint16_t)((data[10] >> 4) | (data[11] << 4));

    /* 12-bit (0–4095, ~2048 center) → 8-bit (0–255, 128 center) */
    rpt->x  = (uint8_t)(lx >> 4);
    rpt->y  = (uint8_t)(0xFF - (ly >> 4)); /* invert: up = 0 */
    rpt->z  = (uint8_t)(rx >> 4);
    rpt->rz = (uint8_t)(0xFF - (ry >> 4));
}

/* ---------------------------------------------------------------------- */
/*  Nintendo Switch Pro Controller — simple report (0x3F)                 */
/*                                                                        */
/*  Sent before the USB init handshake, or by Switch-compatible           */
/*  third-party controllers that don't implement the full protocol.       */
/*  Uses standard hat encoding and 8-bit axes.                            */
/* ---------------------------------------------------------------------- */

static void parse_switch_pro_simple(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    if (len < 8)
        return;

    /* data[0] = 0x3F (report ID) */

    uint8_t btn1 = data[1]; /* Y=0x01 B=0x02 A=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80 */
    uint8_t btn2 = data[2]; /* -=0x01 +=0x02 LS=0x04 RS=0x08 Home=0x10 Cap=0x20 */

    rpt->square   = (btn1 & 0x01) != 0; /* Y (west)  */
    rpt->cross    = (btn1 & 0x02) != 0; /* B (south) */
    rpt->circle   = (btn1 & 0x04) != 0; /* A (east)  */
    rpt->triangle = (btn1 & 0x08) != 0; /* X (north) */
    rpt->l1       = (btn1 & 0x10) != 0;
    rpt->r1       = (btn1 & 0x20) != 0;
    rpt->l2       = (btn1 & 0x40) != 0;
    rpt->r2       = (btn1 & 0x80) != 0;

    rpt->select = (btn2 & 0x01) != 0;
    rpt->start  = (btn2 & 0x02) != 0;
    rpt->l3     = (btn2 & 0x04) != 0;
    rpt->r3     = (btn2 & 0x08) != 0;
    rpt->system = (btn2 & 0x10) != 0;
    rpt->extra  = (btn2 & 0x20) != 0;

    rpt->dpad = (data[3] > 8) ? 8 : data[3];

    rpt->x  = data[4];
    rpt->y  = data[5];
    rpt->z  = data[6];
    rpt->rz = data[7];
}

/* ---------------------------------------------------------------------- */
/*  Nintendo Switch — dispatch by report ID                               */
/* ---------------------------------------------------------------------- */

static bool parse_switch(const uint8_t *data, uint16_t len, tjuh_gamepad_report_t *rpt)
{
    if (len < 8)
        return false;

    switch (data[0]) {
        case 0x30:
            parse_switch_pro_full(data, len, rpt);
            return true;
        case 0x3F:
            parse_switch_pro_simple(data, len, rpt);
            return true;
        default:
            return false;
    }
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
/*  Sony controller dispatch                                              */
/* ---------------------------------------------------------------------- */

static bool parse_sony(uint16_t pid, const uint8_t *data, uint16_t len,
                       tjuh_gamepad_report_t *rpt)
{
    if (len < 10 || data[0] != 0x01)
        return false;

    switch (pid) {
        case PID_DUALSENSE:
        case PID_DUALSENSE_EDGE:
            parse_sony_dualsense(data, len, rpt);
            return true;

        case PID_DS4_V1:
        case PID_DS4_V2:
        default:
            /* DS4 layout is the default for unknown Sony PIDs (covers clones) */
            parse_sony_ds4(data, len, rpt);
            return true;
    }
}

/* ---------------------------------------------------------------------- */
/*  Size-based fallback for unknown VID/PID                               */
/*                                                                        */
/*  Preserves the original detection paths for generic gamepads and       */
/*  Xbox 360, but no longer blindly sends ep_size=64 to the DS4 parser.  */
/* ---------------------------------------------------------------------- */

static bool parse_by_endpoint_size(const uint8_t *data, uint16_t actual_len,
                                   uint16_t max_ep_size, tjuh_gamepad_report_t *rpt)
{
    switch (max_ep_size) {
        case 8:
            if (actual_len == 8) {
                parse_generic_8byte(data, actual_len, rpt);
                return true;
            }
            if (actual_len == 3) {
                parse_generic_3byte(data, actual_len, rpt);
                return true;
            }
            break;

        case 32:
            if (actual_len == 20) {
                parse_xbox360(data, actual_len, rpt);
                return true;
            }
            break;

        default:
            break;
    }

    /*
     * Catch-all for unknown controllers with ep_size > 8:
     * Many generic DInput gamepads and controller adapters send reports
     * that start with a report ID followed by 4 axis bytes and a hat/button
     * byte in DS4-compatible layout. Accept these only if they look plausible.
     */
    if (actual_len >= 8 && max_ep_size >= 8) {
        uint8_t report_id = data[0];

        /* Report ID is typically 0x01–0x04 for gamepads */
        if (report_id >= 0x01 && report_id <= 0x04) {
            const uint8_t *axes = data + 1;

            /*
             * Sanity check: at least one axis should be near center (~128).
             * This filters out non-gamepad HID reports (keyboards, mice, etc.)
             * that happen to start with a small report ID byte.
             */
            bool any_centered = false;
            for (int i = 0; i < 4 && (i + 1) < actual_len; i++) {
                if (axes[i] >= 96 && axes[i] <= 160) {
                    any_centered = true;
                    break;
                }
            }

            if (any_centered && actual_len >= 9) {
                /*
                 * Assume DS4-compatible layout: report_id + axes(4) + buttons(4).
                 * This covers many third-party DInput pads, Logitech F310 (D mode),
                 * 8BitDo controllers in DInput mode, and similar devices.
                 */
                parse_sony_ds4(data, actual_len, rpt);
                return true;
            }
        }
    }

    return false;
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
    if (actual_len == 0)
        return false;

    /* --- Stage 1: Hint-based routing (set during enumeration) --- */

    if (hint == TJUH_HINT_XBOX_ONE)
        return false;

    if (hint == TJUH_HINT_SWITCH_PRO)
        return parse_switch(data, actual_len, report_out);

    /* --- Stage 2: VID/PID-based routing --- */

    uint16_t vid = 0;
    uint16_t pid = 0;
    bool have_id = get_vid_pid(dev_addr, &vid, &pid);

    if (have_id) {
        switch (vid) {
            case VID_SONY:
                return parse_sony(pid, data, actual_len, report_out);

            case VID_NINTENDO:
                return parse_switch(data, actual_len, report_out);

            default:
                break;
        }
    }

    /* --- Stage 3: Endpoint-size heuristic (generic / Xbox 360) --- */

    return parse_by_endpoint_size(data, actual_len, max_ep_size, report_out);
}
