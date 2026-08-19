# CheckInReader

Firmware for the dancing school's unattended check-in reader: an M5Stack CoreS3 with the NFC
Universal Unit (ST25R3916), mounted at a class entrance. A student taps their card, the device
POSTs the tag to the school's check-in API, and shows the result on screen.

Full protocol contract (what `POST /check-in/scan` expects and returns) lives in the API repo:
`Project/docs/features/check-in-reader.md` (ADR-024). This device is a **thin client** — it only
sends the scanned UID and renders the `Message` the API sends back. All check-in logic (which
session, which student, warnings) lives server-side.

## Hardware

- M5Stack CoreS3 (or CoreS3 SE)
- NFC Universal Unit (ST25R3916), connected over the Grove/I2C port

## Setup

1. Copy `include/config.h.example` to `include/config.h` and fill in your WiFi credentials, the
   API's base URL, and a device API key (minted from the manager's API Keys page in the web app,
   configured with `Mode = Classroom` or `Auto` — see ADR-024). `config.h` is gitignored; never
   commit real credentials.
2. Install [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
3. Build: `pio run`
4. Flash (device connected over USB): `pio run -t upload`
5. Serial monitor: `pio device monitor`

## Status

Scaffold only — `connectWifi()`, `tryReadCard()`, and `scanCard()` are stubs that compile but do
not yet talk to WiFi, the NFC unit, or the API. See the `TODO` comments in `src/`.
