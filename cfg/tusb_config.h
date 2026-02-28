/*
 * TJUH — Reference TinyUSB configuration
 *
 * This configuration enables the bare endpoint transfer API required by TJUH.
 * Applications can include this directory in their include path, or provide
 * their own tusb_config.h with equivalent settings.
 *
 * Based on TinyUSB examples by Ha Thach (tinyusb.org), MIT License.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Board                                                             */
/* ------------------------------------------------------------------ */

#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT      0
#endif

#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

/* ------------------------------------------------------------------ */
/*  Common                                                            */
/* ------------------------------------------------------------------ */

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#define CFG_TUH_ENABLED       1
#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(4)))
#endif

/* ------------------------------------------------------------------ */
/*  Host stack                                                        */
/* ------------------------------------------------------------------ */

#define CFG_TUH_ENUMERATION_BUFSIZE 384

#define CFG_TUH_HUB           1
#define CFG_TUH_DEVICE_MAX    (CFG_TUH_HUB ? 4 : 1)
#define CFG_TUH_ENDPOINT_MAX  8

/* Required: bare endpoint transfer API */
#define CFG_TUH_API_EDPT_XFER 1

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
