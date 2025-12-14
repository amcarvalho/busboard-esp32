# busboard-esp32

Small Flask service and ESP32 sketch for showing upcoming TfL arrivals at a StopPoint.

Project contents
- main.py — Flask service that queries TfL arrivals and exposes:
  - GET /buses — returns next N unique routes (default 5, configurable)
  - GET /health — simple health check
- bus_board/
  - bus_board.ino — ESP32 Arduino sketch (uses secrets.h for WiFi/API config)
  - secrets.example.h — example of required defines
  - secrets.h — (likely contains real secrets; should not be committed)
- Dockerfile — container image build (if present)
- .github/workflows/ — CI workflow(s) (if present)

Key behavior
- Default number of results returned by /buses is 5. Override with environment variable TFL_RESULTS_LIMIT.
- Simple in-memory cache in main.py: CACHE_SECONDS = 20 to avoid frequent API calls.
- TfL credentials are read from environment variables:
  - TFL_APP_ID, TFL_APP_KEY, TFL_STOP_ID

Configuration

Environment variables (runtime)
- TFL_APP_ID — TfL app id (if required)
- TFL_APP_KEY — TfL app key (if required)
- TFL_STOP_ID — TfL StopPoint id (required)
- TFL_RESULTS_LIMIT — optional integer; default 5
- PORT — optional, defaults to 8000 (main.py default)

ESP32 secrets (bus_board)
- Copy `bus_board/secrets.example.h` → `bus_board/secrets.h` and fill WiFi / API constants.
- Do NOT commit `bus_board/secrets.h` with real credentials.

Recommended .gitignore additions
```text
# filepath: /Users/amcarvalho/Documents/busboard-esp32/.gitignore
.env
bus_board/secrets.h
```

Run locally (Python)
```bash
# Install deps
pip install Flask requests

export TFL_APP_ID=...
export TFL_APP_KEY=...
export TFL_STOP_ID=490008660N
# optional
export TFL_RESULTS_LIMIT=5

python main.py
# then:
curl http://localhost:8000/buses
```

Docker (recommended: pass secrets at runtime)
```bash
# build
docker build -t busboard-esp32 .

# run with runtime env (do not COPY .env into image)
docker run -e TFL_APP_ID="$TFL_APP_ID" -e TFL_APP_KEY="$TFL_APP_KEY" \
  -e TFL_STOP_ID="$TFL_STOP_ID" -e TFL_RESULTS_LIMIT=5 -p 8000:8000 busboard-esp32
```

CI / GitHub Actions guidance
- Do not commit secret files. Use GitHub repository secrets and pass them into the runner or as BuildKit secrets.
- Example: build & push to ghcr.io on push to main/master (use workflow secrets). Avoid copying any local `.env` or `secrets.h` in the Dockerfile.

If you need, I can:
- Add/modify .gitignore
- Update Dockerfile to remove any COPY of secret files and optionally use BuildKit --secret
- Create a GitHub Actions workflow that uses repository secrets to pass the runtime env or build-time secrets safely.