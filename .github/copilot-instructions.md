# Copilot / Agent instructions for busboard-esp32

Summary
- Small project with three cooperating components:
  - Backend (Python Flask) — provides /buses and /health (see `backend/main.py`).
  - Mobile (Flutter) — BLE provisioning app that sends Wi‑Fi SSID/password to the device (`mobile/lib/main.dart`).
  - Firmware (ESP32 Arduino sketch) — BLE receives credentials, stores them with `Preferences`, fetches `/buses` and renders to TFT (`firmware/bus_board/bus_board.ino`).

Key integration points (do not change without updating both sides)
- BLE UUIDs: defined in `bus_board.ino` (SERVICE_UUID, SSID_CHAR_UUID, PASS_CHAR_UUID) and used verbatim by the Flutter app. If you change them, update both files.
- HTTP API: firmware expects `GET /buses` to return an array of objects with keys `route`, `destination`, and `due_in_minutes` (see `get_next_buses()` in `backend/main.py` and `fetchAndDisplayBuses()` in `bus_board.ino`).
- Secrets: firmware uses `bus_board/secrets.h` (copy from `secrets.example.h`); **do not commit** `secrets.h` with real credentials.

Backend notes (exact references / behavior)
- File: `backend/main.py` — caching via `_CACHE` with `CACHE_SECONDS = 20`. Tests and behavior assume the cache and de-duplication by `lineName` (first arrival per route).
- Response shape: returns a list of up to `TFL_RESULTS_LIMIT` (env var, default 5) maps of `{route, destination, due_in_minutes}`. Destination is truncated at first comma by `_truncate_destination()` (unit-tested in `backend/tests/test_destination.py`).
- Environment variables used at runtime: `TFL_APP_ID`, `TFL_APP_KEY`, `TFL_STOP_ID` (required), optional `TFL_RESULTS_LIMIT`, optional `PORT` (defaults to 8000).
- Local dev commands:
  - from `backend/`: `pip install -r requirements.txt` then `python main.py`
  - tests: `cd backend && pytest -q` (tests add repo root to `sys.path` to import `main`).
  - Docker: `docker build -t busboard-esp32 .` and `docker run -e TFL_APP_ID=... -e TFL_APP_KEY=... -e TFL_STOP_ID=... -p 8000:8000 busboard-esp32`
  - docker-compose: `cd backend && docker-compose up` (it reads `.env` and exposes host port 8008 -> container 8000 by default in `docker-compose.yml`).

Mobile notes
- File: `mobile/lib/main.dart` — uses `flutter_blue_plus` for BLE scanning/communication and `permission_handler` to request Bluetooth/location permissions. The app scans for devices named `Bus-Board-*` and writes credentials to the firmware BLE characteristics.
- Dev commands:
  - `cd mobile && flutter pub get`
  - `cd mobile && flutter run -d <device>`
- Platform caveats: Android 12+ requires runtime Bluetooth permissions; iOS requires adding Bluetooth usage strings to Info.plist if testing on-device (app currently requests permissions via `permission_handler`).

Firmware notes
- File: `firmware/bus_board/bus_board.ino`
  - BLE provisioning: when no stored creds, firmware exposes BLE service & characteristics and waits for SSID/PASS writes, storing them with `Preferences` and then attempts to connect.
  - Pressing the hardware RESET button (GPIO defined by `RESET_BTN`) during boot triggers `factoryReset()` which clears stored credentials and restarts.
  - HTTP fetch behavior: uses `SERVER_URL` from `secrets.h` to GET bus data; `fetchHTTP()` retries up to 3 times and has timeouts.
  - LED behavior: `TARGET_ROUTE` (also in `secrets.h`) controls LED color logic when that route is present.
- Required Arduino libraries (from includes): `Arduino_GFX_Library`, `Preferences`, `BLEDevice`, `WiFi`, `HTTPClient`, `ArduinoJson`, `Adafruit_NeoPixel` — ensure build environment (Arduino IDE or PlatformIO) has these installed.
- Build/flash pattern: this is an Arduino `.ino` sketch for ESP32. Typical flow: copy `secrets.example.h` → `secrets.h`, fill `WIFI_SSID`, `WIFI_PASSWORD`, `SERVER_URL` (e.g., `http://<host>:8000/buses`) and `TARGET_ROUTE`, then compile/flash to an ESP32.

Testing and change guidance (practical, repository‑specific)
- If you change the `/buses` response shape or field names, update `bus_board.ino` parsing and add/adjust unit tests in `backend/tests/`.
- When modifying BLE behavior or UUIDs, update both `bus_board.ino` and `mobile/lib/main.dart` and add a short comment in the changed file referencing the counterpart file.
- For backend requests to TfL, handle `requests.RequestException` consistently; current behavior logs and returns cached data — preserve that pattern or add explicit tests.
- Keep secrets out of Git (see `backend/README.md` instructions and `.gitignore` recommendations in that file).

What agents should do first
1. Read `backend/main.py`, `firmware/bus_board/bus_board.ino`, and `mobile/lib/main.dart` to understand cross-component expectations.
2. When adding features, update the small, focused unit test in `backend/tests/` and run `cd backend && pytest`.
3. If you need to change runtime wiring (env vars, ports, server URLs), update `README.md` + `docker-compose.yml` and add explicit integration test notes so developers know how to test on-device.

Notes for maintainers / CI
- There is currently no CI workflow in `.github/workflows/`. If adding CI, run `cd backend && pytest` and consider adding a lint/build step for the Flutter app (`flutter analyze`) and a recommended esp32 build matrix (or at least a PlatformIO build check) where possible.

If anything here is unclear or you want the instructions to be stricter (for example: exact lint rules, preferred commit messages, or a CI proposal), tell me what to add or change and I will iterate.