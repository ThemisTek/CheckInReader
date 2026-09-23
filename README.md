# CheckInReader

Firmware for the dancing school's unattended check-in reader: an M5Stack CoreS3 with the NFC
Universal Unit (ST25R3916), mounted at a class entrance. A student taps their card, the device
POSTs the tag to the school's check-in API, and shows the result on screen.

Full protocol contract (what `POST /check-in/scan` expects and returns) lives in the API repo:
`Project/docs/features/check-in-reader.md` (ADR-024); how a reader is set up lives in
`Project/docs/features/device-pairing.md` (ADR-036). This device is a **thin client** — it only
sends the scanned UID and renders the `Message` the API sends back. All check-in logic (which
session, which student, warnings) lives server-side.

## Hardware

- M5Stack CoreS3 (or CoreS3 SE)
- NFC Universal Unit (ST25R3916), connected over the Grove/I2C port

## Building and flashing

1. Copy `include/config.h.example` to `include/config.h` and set `API_BASE_URL`. `config.h` is
   gitignored; never commit real credentials.
2. Install [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
3. Build: `pio run`
4. Flash (device connected over USB): `pio run -t upload`
5. Serial monitor: `pio device monitor`

## Setting a reader up (no PC needed)

1. **Power it on.** A reader with no WiFi shows a QR code and starts a hotspot named `DanceReader-XXXX`.
2. **Scan the QR with a phone.** It joins the hotspot; a setup page opens. Pick the school's WiFi,
   enter its password, tap Connect. The reader's screen says whether it worked. (If the phone's page
   stops loading after you tap Connect, that is normal — the hotspot moves to the router's channel.
   Trust the reader's screen.)
3. **Pair it to the school.** The reader shows a 6-character code. In the web app open
   *Settings → API keys → Add reader*, type the code, check the details match, name the reader,
   choose its role (and room) and tap Pair. The reader collects its own key.

## Changing WiFi or re-pairing

Hold the idle "Ready" screen for **5 seconds**. A menu offers *Change WiFi* (a new hotspot session)
and *Re-pair to school* (a new code; the old key stays in the web app until you revoke it). Both ask
for a second tap to confirm, because the reader stops checking students in while you do it. The same
long-press works at power-on if the saved WiFi is no longer reachable.

## Readers flashed before pairing existed

A `config.h` that still holds WiFi networks, an API key and a company ID keeps working unchanged —
those values are the fallback whenever nothing has been saved on the device. The first time someone
uses the setup menu, the saved values take over.

## Known limitations

- The device does not verify the API server's TLS certificate (`HTTPClient` with no CA set). Pairing
  hands the reader a long-lived key over that connection, so pinning the server's root CA is the
  next hardening step.
- If the reply carrying the key is lost in transit, the reader shows an expired code; pair it again
  and revoke the unused key in the web app.
