# TJUH — Tiny Joystick USB Host

USB Host library for game controllers on the Raspberry Pi Pico (RP2040). Uses the TinyUSB bare endpoint API to support a wide range of wired USB gamepads including controllers that don't follow the standard HID class protocol (e.g. Xbox 360).

The library identifies controllers by their report format rather than by VID/PID, so most clones and third-party controllers work out of the box.

Based on the [TinyUSB host examples](https://github.com/hathach/tinyusb/tree/master/examples/host).

## Supported Controllers

| Controller               | Status |
| ------------------------ | ------ |
| PS5 DualSense            | ✓      |
| PS5 DualSense Edge       | ✓      |
| PS4 DualShock 4 (v1/v2)  | ✓      |
| Xbox 360 Wired           | ✓      |
| Switch Pro Controller    | ✓      |
| 8BitDo SN30 Pro          | ✓      |
| Competition Pro USB      | ✓      |
| Generic USB 1.1 Gamepad  | ✓      |
| Generic DInput Gamepads  | ✓*     |
| Xbox One                 | ✗      |
| PS3 Controller           | ✗      |

\* Generic DInput gamepads with DS4-compatible report layout are auto-detected via heuristics.

Xbox One support is detected but not yet functional. For Xbox One controllers, see [tusb_xinput](https://github.com/Ryzee119/tusb_xinput).

PS3 DualShock 3 requires a USB control transfer to activate report streaming, which is not yet implemented.

## Features

- Callback-based API: receive parsed gamepad reports, connect/disconnect notifications
- Unified report format across all controller types (axes, D-pad, buttons)
- USB hub support (multiple controllers via a hub)
- USB 1.1 on the Pico's native Micro-USB port (no PIO-USB required)

## Integration

Add as a git submodule or copy into your project:

```bash
git submodule add https://github.com/mwyborski/tjuh.git lib/tjuh
```

In your `CMakeLists.txt`:

```cmake
add_subdirectory(lib/tjuh)

target_link_libraries(my_app
    tjuh
    tinyusb_host
    tinyusb_board
)
```

TJUH ships a reference `tusb_config.h` in `cfg/` that is included by default. To provide your own, set `-DTJUH_USE_REFERENCE_CONFIG=OFF` and make sure your config defines at minimum:

```c
#define CFG_TUH_ENABLED        1
#define CFG_TUH_API_EDPT_XFER  1
#define CFG_TUH_HUB            1
#define CFG_TUH_ENDPOINT_MAX   8
#define CFG_TUH_ENUMERATION_BUFSIZE 384
```

## Usage

```c
#include "tjuh.h"

static void on_report(uint8_t dev_addr, const tjuh_gamepad_report_t *rpt)
{
    printf("X=%u Y=%u Cross=%u\n", rpt->x, rpt->y, rpt->cross);
}

int main(void)
{
    board_init();

    tjuh_config_t config = {
        .on_report     = on_report,
        .on_connect    = NULL,
        .on_disconnect = NULL,
    };
    tjuh_init(&config);

    while (1) {
        tuh_task();
    }
}
```

## Report Format

All controllers are mapped to `tjuh_gamepad_report_t`:

| Field      | Type     | Description                                     |
| ---------- | -------- | ----------------------------------------------- |
| `x, y`     | uint8_t  | Left stick (0–255, 128 = center)                |
| `z, rz`    | uint8_t  | Right stick (0–255, 128 = center)               |
| `dpad`     | 4 bits   | Hat direction (0=N … 7=NW, 8=released)          |
| `cross`    | 1 bit    | South / A button                                |
| `circle`   | 1 bit    | East / B button                                 |
| `square`   | 1 bit    | West / X button                                 |
| `triangle` | 1 bit    | North / Y button                                |
| `l1, r1`   | 1 bit    | Shoulder buttons                                |
| `l2, r2`   | 1 bit    | Triggers (digital)                              |
| `select`   | 1 bit    | Select / Back / Share                           |
| `start`    | 1 bit    | Start / Options                                 |
| `l3, r3`   | 1 bit    | Stick clicks                                    |
| `system`   | 1 bit    | PS / Xbox / Home button                         |
| `extra`    | 1 bit    | Touchpad click (DualShock 4/DualSense)          |

## Remarks

If you need an OTG cable, you can make one yourself:
https://www.instructables.com/Make-a-USB-OTG-host-cable-The-easy-way/

## License

MIT License — see [LICENSE](LICENSE).
