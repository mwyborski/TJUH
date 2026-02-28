/*
 * TJUH PWM Output Example
 *
 * Demonstrates TJUH by mapping gamepad inputs to physical outputs on the
 * Raspberry Pi Pico (RP2040) or Pico 2 (RP2350):
 *
 *   - 4 analog axes  → 4 PWM outputs (measurable as DC voltage with a multimeter)
 *   - 8 buttons      → 8 digital GPIO outputs (active high, 3.3V when pressed)
 *   - All inputs     → UART serial console (GP0=TX, GP1=RX, 115200 baud)
 *
 * Note: The USB port is occupied by TinyUSB in host mode for gamepad input.
 * Serial output is available only via UART on GP0/GP1. Use a USB-to-serial
 * adapter (e.g. FTDI, CP2102) or a Raspberry Pi's UART pins to read it.
 *
 * PWM outputs produce a duty cycle proportional to the axis value:
 *   Axis = 0   → 0% duty   → ~0V
 *   Axis = 128 → 50% duty  → ~1.65V  (stick centered)
 *   Axis = 255 → 100% duty → ~3.3V
 *
 * A standard multimeter on DC voltage mode averages the PWM and reads
 * a proportional voltage — no external filtering needed for validation.
 *
 * Compile with -DTJUH_EXAMPLE_ENABLE_PIN_OUTPUT=0 (or change the define
 * below) to disable all physical pin outputs and use serial logging only.
 *
 * Pin assignment (active side of the Pico, all even GPIOs for PWM to
 * avoid slice conflicts):
 *
 *   Function        GPIO    Physical Pin    PWM Slice
 *   ──────────────  ──────  ────────────    ─────────
 *   X axis (LX)     GP2     Pin 4          Slice 1A
 *   Y axis (LY)     GP4     Pin 6          Slice 2A
 *   Z axis (RX)     GP6     Pin 9          Slice 3A
 *   RZ axis (RY)    GP8     Pin 11         Slice 4A
 *
 *   Cross / A       GP10    Pin 14
 *   Circle / B      GP11    Pin 15
 *   Square / X      GP12    Pin 16
 *   Triangle / Y    GP13    Pin 17
 *   L1              GP14    Pin 19
 *   R1              GP15    Pin 20
 *   Start           GP16    Pin 21
 *   Select          GP17    Pin 22
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "tjuh.h"

/*
 * Set to 0 to disable all physical pin outputs (PWM + GPIO).
 * The example will then only log to UART serial.
 * Can also be set via CMake: -DTJUH_EXAMPLE_ENABLE_PIN_OUTPUT=0
 */
#ifndef TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
#define TJUH_EXAMPLE_ENABLE_PIN_OUTPUT 1
#endif

#if TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
#include "hardware/pwm.h"

/* ---------------------------------------------------------------------- */
/*  Pin definitions                                                       */
/* ---------------------------------------------------------------------- */

#define PIN_AXIS_X     2
#define PIN_AXIS_Y     4
#define PIN_AXIS_Z     6
#define PIN_AXIS_RZ    8

#define PIN_CROSS      10
#define PIN_CIRCLE     11
#define PIN_SQUARE     12
#define PIN_TRIANGLE   13
#define PIN_L1         14
#define PIN_R1         15
#define PIN_START      16
#define PIN_SELECT     17

static const uint8_t AXIS_PINS[] = {
    PIN_AXIS_X, PIN_AXIS_Y, PIN_AXIS_Z, PIN_AXIS_RZ
};

static const uint8_t BUTTON_PINS[] = {
    PIN_CROSS, PIN_CIRCLE, PIN_SQUARE, PIN_TRIANGLE,
    PIN_L1, PIN_R1, PIN_START, PIN_SELECT
};

#define AXIS_PIN_COUNT   (sizeof(AXIS_PINS) / sizeof(AXIS_PINS[0]))
#define BUTTON_PIN_COUNT (sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]))

/* ---------------------------------------------------------------------- */
/*  PWM and GPIO initialization                                           */
/* ---------------------------------------------------------------------- */

static void init_pwm_outputs(void)
{
    for (size_t i = 0; i < AXIS_PIN_COUNT; i++) {
        gpio_set_function(AXIS_PINS[i], GPIO_FUNC_PWM);

        uint slice = pwm_gpio_to_slice_num(AXIS_PINS[i]);
        pwm_config cfg = pwm_get_default_config();

        /* 8-bit resolution (0–255) matching the gamepad axis range */
        pwm_config_set_wrap(&cfg, 255);
        pwm_config_set_clkdiv(&cfg, 1.0f);

        pwm_init(slice, &cfg, true);
        pwm_set_gpio_level(AXIS_PINS[i], 128);
    }
}

static void init_button_outputs(void)
{
    for (size_t i = 0; i < BUTTON_PIN_COUNT; i++) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_OUT);
        gpio_put(BUTTON_PINS[i], false);
    }
}

/* ---------------------------------------------------------------------- */
/*  Output update                                                         */
/* ---------------------------------------------------------------------- */

static void update_outputs(const tjuh_gamepad_report_t *rpt)
{
    pwm_set_gpio_level(PIN_AXIS_X,  rpt->x);
    pwm_set_gpio_level(PIN_AXIS_Y,  rpt->y);
    pwm_set_gpio_level(PIN_AXIS_Z,  rpt->z);
    pwm_set_gpio_level(PIN_AXIS_RZ, rpt->rz);

    gpio_put(PIN_CROSS,    rpt->cross);
    gpio_put(PIN_CIRCLE,   rpt->circle);
    gpio_put(PIN_SQUARE,   rpt->square);
    gpio_put(PIN_TRIANGLE, rpt->triangle);
    gpio_put(PIN_L1,       rpt->l1);
    gpio_put(PIN_R1,       rpt->r1);
    gpio_put(PIN_START,    rpt->start);
    gpio_put(PIN_SELECT,   rpt->select);
}

static void reset_outputs(void)
{
    for (size_t i = 0; i < AXIS_PIN_COUNT; i++)
        pwm_set_gpio_level(AXIS_PINS[i], 128);

    for (size_t i = 0; i < BUTTON_PIN_COUNT; i++)
        gpio_put(BUTTON_PINS[i], false);
}

#endif /* TJUH_EXAMPLE_ENABLE_PIN_OUTPUT */

/* ---------------------------------------------------------------------- */
/*  D-Pad direction strings                                               */
/* ---------------------------------------------------------------------- */

static const char *DPAD_STR[] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none"
};

/* ---------------------------------------------------------------------- */
/*  Serial logging                                                        */
/* ---------------------------------------------------------------------- */

static void log_report(const tjuh_gamepad_report_t *rpt)
{
    printf("X:%3u Y:%3u Z:%3u RZ:%3u | DPad:%-4s | ",
           rpt->x, rpt->y, rpt->z, rpt->rz,
           DPAD_STR[rpt->dpad < 9 ? rpt->dpad : 8]);

    if (rpt->cross)    printf("Cross ");
    if (rpt->circle)   printf("Circle ");
    if (rpt->square)   printf("Square ");
    if (rpt->triangle) printf("Tri ");
    if (rpt->l1)       printf("L1 ");
    if (rpt->r1)       printf("R1 ");
    if (rpt->l2)       printf("L2 ");
    if (rpt->r2)       printf("R2 ");
    if (rpt->start)    printf("Start ");
    if (rpt->select)   printf("Select ");
    if (rpt->l3)       printf("L3 ");
    if (rpt->r3)       printf("R3 ");
    if (rpt->system)   printf("Sys ");
    if (rpt->extra)    printf("Extra ");

    printf("\r\n");
}

/* ---------------------------------------------------------------------- */
/*  TJUH callbacks                                                        */
/* ---------------------------------------------------------------------- */

static void on_report(uint8_t dev_addr, const tjuh_gamepad_report_t *rpt)
{
    (void)dev_addr;

#if TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
    update_outputs(rpt);
#endif

    log_report(rpt);
}

static void on_connect(uint8_t dev_addr, uint16_t vid, uint16_t pid)
{
    printf("[TJUH Example] Connected: dev=%u VID=%04x PID=%04x\r\n",
           dev_addr, vid, pid);

#if TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
    reset_outputs();
#endif
}

static void on_disconnect(uint8_t dev_addr)
{
    printf("[TJUH Example] Disconnected: dev=%u\r\n", dev_addr);

#if TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
    reset_outputs();
#endif
}

/* ---------------------------------------------------------------------- */
/*  Main                                                                  */
/* ---------------------------------------------------------------------- */

int main(void)
{
    board_init();
    stdio_init_all();

    printf("\r\n");
    printf("TJUH Example\r\n");
    printf("============\r\n");

#if TJUH_EXAMPLE_ENABLE_PIN_OUTPUT
    printf("Mode: PWM + GPIO + Serial\r\n");
    printf("Axes    -> PWM:  GP%d(X) GP%d(Y) GP%d(Z) GP%d(RZ)\r\n",
           PIN_AXIS_X, PIN_AXIS_Y, PIN_AXIS_Z, PIN_AXIS_RZ);
    printf("Buttons -> GPIO: GP%d(Cross) GP%d(Circle) GP%d(Square) GP%d(Tri)\r\n",
           PIN_CROSS, PIN_CIRCLE, PIN_SQUARE, PIN_TRIANGLE);
    printf("                 GP%d(L1) GP%d(R1) GP%d(Start) GP%d(Select)\r\n",
           PIN_L1, PIN_R1, PIN_START, PIN_SELECT);

    init_pwm_outputs();
    init_button_outputs();
#else
    printf("Mode: Serial logging only (pin output disabled)\r\n");
#endif

    printf("Connect a USB gamepad to begin.\r\n\r\n");

    tjuh_config_t config = {
        .on_report     = on_report,
        .on_connect    = on_connect,
        .on_disconnect = on_disconnect,
    };
    tjuh_init(&config);

    while (1) {
        tuh_task();
    }

    return 0;
}
