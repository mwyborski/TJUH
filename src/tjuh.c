/*
 * TJUH — Tiny Joystick USB Host
 * USB host stack management and TinyUSB callbacks using the bare endpoint API.
 */

#include "tjuh.h"
#include "tjuh_parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "host/usbh.h"
#include "host/usbh_classdriver.h"

/* ---------------------------------------------------------------------- */
/*  Constants                                                             */
/* ---------------------------------------------------------------------- */

#define LANGUAGE_ID   0x0409
#define BUF_POOL_SIZE 4

/* ---------------------------------------------------------------------- */
/*  Internal state                                                        */
/* ---------------------------------------------------------------------- */

typedef struct {
    tusb_desc_device_t desc_device;
    tjuh_hint_t        hint;
    size_t             max_hid_buf_size;
} tjuh_device_state_t;

static const tjuh_device_state_t s_dev_init = {0};

/* Index 0 is unused — device addresses are 1-based */
static tjuh_device_state_t s_devices[TJUH_MAX_DEVICES + 1];
static uint8_t s_assigned_mask;

static uint8_t s_buf_pool[BUF_POOL_SIZE][64];
static uint8_t s_buf_owner[BUF_POOL_SIZE];

static tjuh_config_t s_config;
static const tjuh_gamepad_report_t s_zero_report = {0};

/* Xbox One initialization sequence */
static const uint8_t s_xboxone_start_input[] = {0x05, 0x20, 0x03, 0x01, 0x00};

/* Switch Pro initialization: handshake + force USB-only mode */
static const uint8_t s_switch_handshake[] = {0x80, 0x02};
static const uint8_t s_switch_force_usb[] = {0x80, 0x04};

/* Known VID/PID for hint detection */
#define TJUH_VID_NINTENDO     0x057E
#define TJUH_PID_SWITCH_PRO   0x2009
#define TJUH_PID_JOYCON_L     0x2006
#define TJUH_PID_JOYCON_R     0x2007

/* ---------------------------------------------------------------------- */
/*  Buffer pool                                                           */
/* ---------------------------------------------------------------------- */

static uint8_t *buf_pool_alloc(uint8_t dev_addr)
{
    for (size_t i = 0; i < BUF_POOL_SIZE; i++) {
        if (s_buf_owner[i] == 0) {
            s_buf_owner[i] = dev_addr;
            return s_buf_pool[i];
        }
    }
    return NULL;
}

static void buf_pool_free(uint8_t dev_addr)
{
    for (size_t i = 0; i < BUF_POOL_SIZE; i++) {
        if (s_buf_owner[i] == dev_addr)
            s_buf_owner[i] = 0;
    }
}

/* ---------------------------------------------------------------------- */
/*  Forward declarations                                                  */
/* ---------------------------------------------------------------------- */

static void on_device_descriptor(tuh_xfer_t *xfer);
static void on_report_received(tuh_xfer_t *xfer);
static void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg);
static bool open_hid_interface(uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
static uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf, uint8_t itf_count, uint16_t max_len);

/* ---------------------------------------------------------------------- */
/*  Xbox One output                                                       */
/* ---------------------------------------------------------------------- */

static uint8_t s_epout_buf[64];

static bool send_xinput_report(uint8_t dev_addr, uint8_t ep_out, const uint8_t *data, uint16_t len)
{
    if (len > sizeof(s_epout_buf))
        return false;

    memcpy(s_epout_buf, data, len);

    tuh_xfer_t xfer = {
        .daddr       = dev_addr,
        .ep_addr     = ep_out,
        .buflen      = len,
        .buffer      = s_epout_buf,
        .complete_cb = NULL,
        .user_data   = 0,
    };

    return tuh_edpt_xfer(&xfer);
}

/* ---------------------------------------------------------------------- */
/*  UTF-16 to UTF-8 helpers (for debug printing)                          */
/* ---------------------------------------------------------------------- */

static void convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len,
                                    uint8_t *utf8, size_t utf8_len)
{
    (void)utf8_len;
    for (size_t i = 0; i < utf16_len; i++) {
        uint16_t ch = utf16[i];
        if (ch < 0x80) {
            *utf8++ = (uint8_t)ch;
        } else if (ch < 0x800) {
            *utf8++ = (uint8_t)(0xC0 | (ch >> 6));
            *utf8++ = (uint8_t)(0x80 | (ch & 0x3F));
        } else {
            *utf8++ = (uint8_t)(0xE0 | (ch >> 12));
            *utf8++ = (uint8_t)(0x80 | ((ch >> 6) & 0x3F));
            *utf8++ = (uint8_t)(0x80 | (ch & 0x3F));
        }
    }
}

static int count_utf8_bytes(const uint16_t *buf, size_t len)
{
    int total = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] < 0x80)       total += 1;
        else if (buf[i] < 0x800) total += 2;
        else                      total += 3;
    }
    return total;
}

static void print_utf16(uint16_t *buf, size_t buf_len)
{
    size_t utf16_len = ((buf[0] & 0xFF) - 2) / sizeof(uint16_t);
    size_t utf8_len  = (size_t)count_utf8_bytes(buf + 1, utf16_len);
    convert_utf16le_to_utf8(buf + 1, utf16_len, (uint8_t *)buf, sizeof(uint16_t) * buf_len);
    ((uint8_t *)buf)[utf8_len] = '\0';
    printf("%s", (char *)buf);
}

/* ---------------------------------------------------------------------- */
/*  Public API                                                            */
/* ---------------------------------------------------------------------- */

void tjuh_init(const tjuh_config_t *config)
{
    if (config)
        s_config = *config;

    memset(s_devices, 0, sizeof(s_devices));
    memset(s_buf_owner, 0, sizeof(s_buf_owner));
    s_assigned_mask = 0;

    tuh_init(BOARD_TUH_RHPORT);
}

bool tjuh_get_device_info(uint8_t dev_addr, uint16_t *vid, uint16_t *pid)
{
    return tjuh_parse_get_vid_pid(dev_addr, vid, pid);
}

/* ---------------------------------------------------------------------- */
/*  Debug utilities                                                       */
/* ---------------------------------------------------------------------- */

static const char *s_dpad_str[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "none"};

void tjuh_print_report(const tjuh_gamepad_report_t *rpt)
{
    printf("(x, y, z, rz) = (%u, %u, %u, %u) ", rpt->x, rpt->y, rpt->z, rpt->rz);
    printf("DPad = %s ", s_dpad_str[rpt->dpad]);

    if (rpt->square)   printf("Square ");
    if (rpt->cross)    printf("Cross ");
    if (rpt->circle)   printf("Circle ");
    if (rpt->triangle) printf("Triangle ");
    if (rpt->l1)       printf("L1 ");
    if (rpt->r1)       printf("R1 ");
    if (rpt->l2)       printf("L2 ");
    if (rpt->r2)       printf("R2 ");
    if (rpt->select)   printf("Select ");
    if (rpt->start)    printf("Start ");
    if (rpt->l3)       printf("L3 ");
    if (rpt->r3)       printf("R3 ");
    if (rpt->system)   printf("System ");
    if (rpt->extra)    printf("Extra ");
    printf("\r\n");
}

void tjuh_print_raw(const uint8_t *buf, uint16_t len, uint16_t max_ep_size)
{
    printf("[size: %d max: %d]:", len, max_ep_size);
    for (uint16_t i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\r\n");
}

/* ---------------------------------------------------------------------- */
/*  TinyUSB mount/unmount callbacks                                       */
/* ---------------------------------------------------------------------- */

void tuh_mount_cb(uint8_t dev_addr)
{
    printf("[TJUH] Device attached, address = %u\r\n", dev_addr);

    if (dev_addr > TJUH_MAX_DEVICES) {
        printf("[TJUH] Device address %u exceeds max (%d)\r\n", dev_addr, TJUH_MAX_DEVICES);
        return;
    }

    s_devices[dev_addr] = s_dev_init;
    s_assigned_mask |= (uint8_t)(0x01 << dev_addr);

    tuh_descriptor_get_device(dev_addr, &s_devices[dev_addr].desc_device, 18,
                              on_device_descriptor, 0);
}

void tuh_umount_cb(uint8_t dev_addr)
{
    printf("[TJUH] Device removed, address = %u\r\n", dev_addr);

    tjuh_parse_free_device(dev_addr);
    buf_pool_free(dev_addr);

    if (dev_addr <= TJUH_MAX_DEVICES) {
        s_devices[dev_addr].hint = TJUH_HINT_NONE;
        s_devices[dev_addr].max_hid_buf_size = 64;
        s_assigned_mask &= (uint8_t)~(0x01 << dev_addr);
    }

    if (s_config.on_disconnect)
        s_config.on_disconnect(dev_addr);
}

/* ---------------------------------------------------------------------- */
/*  Device descriptor callback                                            */
/* ---------------------------------------------------------------------- */

static void on_device_descriptor(tuh_xfer_t *xfer)
{
    if (xfer->result != XFER_RESULT_SUCCESS) {
        printf("[TJUH] Failed to get device descriptor\r\n");
        return;
    }

    uint8_t daddr = xfer->daddr;
    tusb_desc_device_t *desc = &s_devices[daddr].desc_device;

    printf("[TJUH] Device %u: ID %04x:%04x\r\n", daddr, desc->idVendor, desc->idProduct);

    /* Print string descriptors */
    uint16_t temp_buf[128];

    printf("  iManufacturer  %u  ", desc->iManufacturer);
    if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(
            daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
        print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
    printf("\r\n");

    printf("  iProduct       %u  ", desc->iProduct);
    if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(
            daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
        print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
    printf("\r\n");

    if (tjuh_parse_init_device(daddr, desc->idVendor, desc->idProduct)) {
        /* Detect controllers that need special handling during enumeration */
        if (desc->idVendor == TJUH_VID_NINTENDO &&
            (desc->idProduct == TJUH_PID_SWITCH_PRO ||
             desc->idProduct == TJUH_PID_JOYCON_L ||
             desc->idProduct == TJUH_PID_JOYCON_R))
        {
            printf("[TJUH] Nintendo Switch controller detected\r\n");
            s_devices[daddr].hint = TJUH_HINT_SWITCH_PRO;
        }

        if (s_config.on_connect)
            s_config.on_connect(daddr, desc->idVendor, desc->idProduct);

        if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(
                daddr, 0, temp_buf, sizeof(temp_buf)))
            parse_config_descriptor(daddr, (tusb_desc_configuration_t *)temp_buf);
    }
}

/* ---------------------------------------------------------------------- */
/*  Configuration descriptor parsing                                      */
/* ---------------------------------------------------------------------- */

static void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg)
{
    uint8_t const *desc_end = ((uint8_t const *)desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
    uint8_t const *p_desc   = tu_desc_next(desc_cfg);

    uint8_t interface_count = 0;

    while (p_desc < desc_end) {
        uint8_t assoc_itf_count = 1;

        if (TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc)) {
            tusb_desc_interface_assoc_t const *desc_iad =
                (tusb_desc_interface_assoc_t const *)p_desc;
            assoc_itf_count = desc_iad->bInterfaceCount;
            p_desc = tu_desc_next(p_desc);
        }

        if (TUSB_DESC_INTERFACE != tu_desc_type(p_desc))
            return;

        tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t const *)p_desc;
        uint16_t drv_len = count_interface_total_len(desc_itf, assoc_itf_count,
                                                      (uint16_t)(desc_end - p_desc));
        if (drv_len < sizeof(tusb_desc_interface_t))
            return;

        /* Only listen to the first IN endpoint */
        if (interface_count == 0) {
            if (open_hid_interface(dev_addr, desc_itf, drv_len))
                interface_count++;
        }

        p_desc += drv_len;
    }
}

static uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf,
                                          uint8_t itf_count, uint16_t max_len)
{
    uint8_t const *p_desc = (uint8_t const *)desc_itf;
    uint16_t len = 0;

    while (itf_count--) {
        len += tu_desc_len(desc_itf);
        p_desc = tu_desc_next(p_desc);

        while (len < max_len) {
            if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION)
                return len;
            if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
                ((tusb_desc_interface_t const *)p_desc)->bAlternateSetting == 0)
                break;

            len += tu_desc_len(p_desc);
            p_desc = tu_desc_next(p_desc);
        }
    }

    return len;
}

/* ---------------------------------------------------------------------- */
/*  HID interface opening                                                 */
/* ---------------------------------------------------------------------- */

static bool open_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf,
                               uint16_t max_len)
{
    bool ep_in_found = false;

    uint16_t const expected_len =
        (uint16_t)(sizeof(tusb_desc_interface_t) +
                   sizeof(tusb_hid_descriptor_hid_t) +
                   desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));

    /* Detect Xbox One controllers by their characteristic descriptor mismatch.
     * Only set if no hint was assigned during VID/PID detection. */
    if (s_devices[daddr].hint == TJUH_HINT_NONE &&
        max_len == 23 && expected_len == 32 && max_len < expected_len) {
        printf("[TJUH] Xbox One controller detected (descriptor mismatch)\r\n");
        s_devices[daddr].hint = TJUH_HINT_XBOX_ONE;
    }

    uint8_t const *p_desc = (uint8_t const *)desc_itf;

    /* Skip interface descriptor */
    p_desc = tu_desc_next(p_desc);

    /* Skip HID descriptor */
    p_desc = tu_desc_next(p_desc);

    tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;

    for (int i = 0; i < desc_itf->bNumEndpoints; i++) {
        if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType) {
            if (s_devices[daddr].hint != TJUH_HINT_XBOX_ONE) {
                printf("[TJUH] Unexpected descriptor type 0x%02x\r\n", desc_ep->bDescriptorType);
                return false;
            }
        }

        if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN && !ep_in_found) {
            if (!tuh_edpt_open(daddr, desc_ep)) {
                printf("[TJUH] Failed to open IN endpoint 0x%02x\r\n", desc_ep->bEndpointAddress);
                return false;
            }

            uint8_t *buf = buf_pool_alloc(daddr);
            if (!buf)
                return false;

            s_devices[daddr].max_hid_buf_size = desc_ep->wMaxPacketSize;

            tuh_xfer_t xfer = {
                .daddr       = daddr,
                .ep_addr     = desc_ep->bEndpointAddress,
                .buflen      = s_devices[daddr].max_hid_buf_size,
                .buffer      = buf,
                .complete_cb = on_report_received,
                .user_data   = (uintptr_t)buf,
            };

            tuh_edpt_xfer(&xfer);
            printf("[TJUH] Listening on [dev %u: ep 0x%02x]\r\n", daddr, desc_ep->bEndpointAddress);
            ep_in_found = true;

        } else if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_OUT) {
            /* Xbox One requires start-input command on the OUT endpoint */
            if (s_devices[daddr].hint == TJUH_HINT_XBOX_ONE) {
                if (!tuh_edpt_open(daddr, desc_ep)) {
                    printf("[TJUH] Failed to open OUT endpoint 0x%02x\r\n",
                           desc_ep->bEndpointAddress);
                } else {
                    while (usbh_edpt_busy(daddr, desc_ep->bEndpointAddress))
                        tuh_task();

                    send_xinput_report(daddr, desc_ep->bEndpointAddress,
                                       s_xboxone_start_input, sizeof(s_xboxone_start_input));
                }
            }

            /* Switch Pro: handshake + force USB-only mode (prevents BT timeout) */
            if (s_devices[daddr].hint == TJUH_HINT_SWITCH_PRO) {
                if (!tuh_edpt_open(daddr, desc_ep)) {
                    printf("[TJUH] Failed to open OUT endpoint 0x%02x\r\n",
                           desc_ep->bEndpointAddress);
                } else {
                    while (usbh_edpt_busy(daddr, desc_ep->bEndpointAddress))
                        tuh_task();
                    send_xinput_report(daddr, desc_ep->bEndpointAddress,
                                       s_switch_handshake, sizeof(s_switch_handshake));

                    while (usbh_edpt_busy(daddr, desc_ep->bEndpointAddress))
                        tuh_task();
                    send_xinput_report(daddr, desc_ep->bEndpointAddress,
                                       s_switch_force_usb, sizeof(s_switch_force_usb));

                    printf("[TJUH] Switch Pro USB mode activated\r\n");
                }
            }
        }

        p_desc = tu_desc_next(p_desc);
        desc_ep = (tusb_desc_endpoint_t const *)p_desc;
    }

    return ep_in_found;
}

/* ---------------------------------------------------------------------- */
/*  Report reception callback                                             */
/* ---------------------------------------------------------------------- */

static void on_report_received(tuh_xfer_t *xfer)
{
    uint8_t *buf = (uint8_t *)xfer->user_data;

    if (xfer->result == XFER_RESULT_SUCCESS) {
        tjuh_gamepad_report_t report = s_zero_report;

        if (tjuh_parse_report(xfer->daddr, buf,
                              (uint16_t)xfer->actual_len,
                              (uint16_t)s_devices[xfer->daddr].max_hid_buf_size,
                              &report,
                              s_devices[xfer->daddr].hint))
        {
            if (s_config.on_report)
                s_config.on_report(xfer->daddr, &report);
        }
    }

    /* Re-submit the transfer */
    if (s_devices[xfer->daddr].max_hid_buf_size == 32 && xfer->actual_len == 20)
        xfer->buflen = xfer->actual_len;
    else
        xfer->buflen = s_devices[xfer->daddr].max_hid_buf_size;

    xfer->buffer = buf;
    tuh_edpt_xfer(xfer);
}
