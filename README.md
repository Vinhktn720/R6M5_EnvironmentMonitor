# GUI Read Data — Environmental Monitor

This repository contains a simple, realistic example of an environmental sensor dashboard that combines a small front-end (HTML/CSS/JS) served by a Python FastAPI backend. It's designed to show real-time sensor readings from an MCU (Renesas CK-RA6M5 used as an example) delivered over UART/serial and easily presented in a web UI.

Key features:
- A responsive dashboard (temperature, pressure, altitude, IAQ, TVOC, eCO₂, Ethanol). 
- Chart.js time-series widget with pause/resume, windowed view, clear, and CSV export.
- FastAPI backend with WebSocket streaming and HTTP polling fallback.
- Optional serial integration (pyserial) with an in-process serial reading thread and a minimal serial configuration UI.

---

## Quick Start (recommended)

Follow these steps to run the project locally using a Python virtual environment (.venv):

1) Create a virtual environment and activate it (Windows Git Bash example):

```bash
# create the venv
python -m venv .venv

# activate (Git Bash)
source .venv/Scripts/activate

# OR (PowerShell)
.\.venv\Scripts\Activate.ps1
```

2) Install Python requirements:

```bash
python -m pip install --upgrade pip setuptools wheel
python -m pip install -r requirements.txt
```

If you see a ModuleNotFoundError about `pydantic_core`, install a compatible wheel for your Python ABI:

```bash
pip uninstall -y pydantic-core
pip install --only-binary :all: pydantic-core
```

3) Run the app using the virtual environment's Python to ensure ABI compatibility:

```bash
.venv/Scripts/python -m uvicorn backend:app --reload --host 127.0.0.1 --port 8000
```

4) Open the UI in your browser:

```
http://127.0.0.1:8000/
```

---

## Project Layout (files to focus on)
- `backend.py` — FastAPI server. Responsible for:
  - Serving `index.html` and static files in `/static/`.
  - A simulated `sensor_updater()` that periodically sets `latest_data` for testing.
  - A simple WebSocket route (`/ws`) streaming `latest_data`.
  - HTTP fallback route: `GET /sensor-data`.
  - Serial config endpoints: `GET /serial-config` and `POST /serial-config`.
  - A test-only endpoint: `POST /__test/set-latest` to override `latest_data` from the command line for testing.

- `index.html` — Basic HTML shell containing the dashboard markup.
- `static/app.js` — Frontend logic: WebSocket client, HTTP polling fallback, charting control, settings modal wiring, and UI updates.
- `static/styles.css` — Styling for the dashboard, modal, chart layout, and status indicators.
- `hardware/` — MCU example code and sensor libraries for embedded firmware that push serial frames.

---

## Endpoints & Usage
- `GET /` — Hosts the dashboard. 
- `GET /sensor-data` — Returns the latest sample in JSON (HTTP fallback)
- `GET /serial-config` — Return current serial reader configuration (port, baud, timeout, enabled)
- `POST /serial-config` — Set serial config and start the serial reader thread; expects JSON body with `port`, `baud`, and `timeout` (e.g., `{ "port": "COM3", "baud": 115200, "timeout": 2 }`)
- `POST /__test/set-latest` — Test-only endpoint to set `latest_data` forcibly (useful for simulating `warming_up`). Example payload:

```json
{
  "timestamp": "2025-11-30 08:10:12",
  "pressure": 101325,
  "temperature": 23.5,
  "altitude": 10,
  "iaq": 48,
  "tvoc": 150,
  "eco2": 450,
  "ethanol": 2.3,
  "state": "streaming"
}
```

- `ws://127.0.0.1:8000/ws` — WebSocket endpoint with JSON samples pushed once per second.

---

## Frontend Features
- Automatic WebSocket connection with exponential backoff and HTTP polling fallback.
- Real-time dashboard showing: temperature, pressure, altitude, IAQ (color-coded), TVOC, eCO₂, ethanol.
- Time Series Chart (Chart.js) with controls: metric selection, pause/resume, clear, export CSV, window size.
- Settings modal: configure `COM port`, `Baud`, and `Timeout` for the serial reader.
- Warming-up & negative value logic:
  - If the backend sends `state: "warming_up"` or negative values for IAQ/TVOC/etoh, the UI keeps the last valid sample (see `state.lastValid`) and marks the values as stale visually. The status indicator shows "Warming Up" during this state.

---

## Serial integration notes (pyserial)

- The backend includes a `serial_reader_thread_func()` that opens a serial port and parses frames using the MCU payload layout's struct (see `parse_payload_to_dict`). When `POST /serial-config` sets `port`, the backend spawns the thread, populating `app.state.serial_queue` which the WebSocket loop consumes and sets `latest_data`.

- If your hardware frame format differs, update `parse_payload_to_dict(payload)` in `backend.py` accordingly. For reference the current struct is: `'<HBBBBBfffffff'` with fields (year, month, day, hour, minute, second, pressure, temp, alt, iaq, tvoc, eco2, ethanol).

---

## Development & Testing

Run WebSocket + HTTP checks:

```bash
# quick HTTP check
curl http://127.0.0.1:8000/sensor-data

# quick WebSocket check (Python script)
python - <<'PY'
import asyncio, websockets
async def main():
    async with websockets.connect('ws://127.0.0.1:8000/ws') as ws:
        for i in range(3):
            print(await ws.recv())
            await asyncio.sleep(1)
asyncio.run(main())
PY
```

Simulating a warming_up state:

```bash
curl -X POST -H 'Content-Type: application/json' -d '{"iaq": -1, "tvoc": -1, "eco2": -1, "ethanol": -1, "state":"warming_up"}' http://127.0.0.1:8000/__test/set-latest
```

The UI will show the last valid known readings and mark them as stale while displaying the "Warming Up" status.

---

## Adding images (State machine & GUI screenshots)

Add visual documentation (state machine diagrams or UI screenshots) to the `static/images/` directory (create it if missing). Example suggestions:

- `static/images/state-machine.png` — Your state-machine diagram for the MCU or backend.
- `static/images/gui-screenshot.png` — A screenshot of the dashboard UI with chart and controls.

You can reference images in this README like:

```markdown
![State Machine](static/images/state-machine.png)
![GUI Screenshot](static/images/gui-screenshot.png)
```

Tip: Keep images reasonably sized (e.g., 1200px wide) and use captions to explain each diagram.

---

## Hardware / MCU firmware

- The folder `hardware/` contains MCU code (Renesas CK-RA6M5 example code and sensor drivers). This folder contains multiple C files and sensor driver files and illustrates basic MCU wiring and a sample packet format to send to the host over UART.

Developer note: Work in `hardware/` to integrate state-machine diagrams and reference the serial payload format in `backend.py` so developers of the MCU firmware and the server agree on the expected struct.

---

## Troubleshooting

- If you can't start the backend, ensure you activated the virtual environment and that Uvicorn is installed in the same Python interpreter.
- If the WebSocket disconnects frequently, check for multiple running server processes or a firewall that terminates ws connections.
- When testing serial integration, run the app with a real COM port and ensure your user account has permissions to access the serial device.

---

## License & Contribution
Add your licensing information here (MIT, Apache 2.0, etc.). Contributions are welcome — please send PRs for bugfixes or improvements and create issues for feature requests.

---

If you want, I can add a sample `Dockerfile` or a `docker-compose.yml` that runs the backend and serves the UI in production mode, or add a simple UI screenshot to `static/images/` and include it in this README. Tell me which option you'd like next.
# GUI Read Data — Environmental Monitor

This project is a small, realistic mock of an embedded sensor dashboard (Renesas CK-RA6M5 as an example). It includes a tidy single-page front-end (HTML/CSS/JS) and a simple Python FastAPI backend that serves the site and provides live sensor data via WebSocket and HTTP polling.

This repo is useful as a starting point for building an environmental monitor that reads UART/serial data from a microcontroller and displays it in a modern web UI.

---

## High-level concepts & data flow

1) Sensor/MCU (Renesas / external device)
	- The MCU produces serial data (or any other transport) with a JSON or a simple delimited payload.
	- Replace the simulated generator with an actual serial reader (e.g. pyserial), then pass parsed JSON to the backend's updater.

2) Python backend (`backend.py`):
	- Reads sensor data (currently simulated in `get_sensor_data()` and `sensor_updater()`).
	- Stores the latest sample in `latest_data` and continuously updates it (every second) in a background `sensor_updater` coroutine.
	- Serves the web UI and static files from `static/`.
	- Exposes API and streaming endpoints:
	  - HTTP: `GET /sensor-data` — returns the latest JSON sample (fallback for HTTP polling)
	  - WebSocket: `ws://<host>:<port>/ws` — pushes JSON messages to connected clients once per second
	- If you replace the simulated data source with a serial reader, `sensor_updater()` should write parsed payloads to `latest_data`.

3) Frontend (Browser) — `index.html` / `static/app.js`:
	- Loads CSS & JavaScript from `/static`.
	- Attempts a WebSocket connection to `CONFIG.WEBSOCKET_URL` (default `ws://localhost:8000/ws`).
	- On WebSocket connect — receives JSON messages every second and updates the dashboard.
	- If WebSocket fails or reconnect attempts exceed `maxRetries`, it falls back to HTTP polling `CONFIG.API_URL` for `/sensor-data`.
	- Tracks min/max temps, applies IAQ color coding, and updates visual indicators.

---

## File summary & responsibilities

- `index.html`: The small HTML shell (no inline CSS or JS) that references assets in `/static`.
- `static/styles.css`: Extracted CSS, layout and component styling for the dashboard.
- `static/app.js`: Main UI logic (WebSocket / HTTP polling, DOM updates, reconnection/backoff logic).
- `backend.py`: FastAPI server with the simulated sensor generator, a background updater, `GET /sensor-data` (HTTP fallback), `GET /` (serves `index.html`), `WS /ws` (live stream), and static file mount (`/static`).
- `requirements.txt`: Python dependencies (FastAPI, Uvicorn, etc.)

---

## Quick setup & installation

Use a virtual environment (recommended). Example (Windows / Git Bash):

```bash
# create venv
python -m venv .venv

# activate (Git Bash)
source .venv/Scripts/activate

# or PowerShell
.\.venv\Scripts\Activate.ps1
```

Install requirements:

```bash
python -m pip install --upgrade pip setuptools wheel
pip install -r requirements.txt
```

Important: If you see errors about `pydantic_core` (ModuleNotFoundError), install the pre-built wheel for your Python ABI (for example Python 3.12 on Windows):

```bash
pip uninstall -y pydantic-core
pip install --only-binary :all: pydantic-core
```

---

## Running the app

Start the server using the virtual environment Python to ensure the correct interpreter is used (recommended):

```bash
.venv/Scripts/python -m uvicorn backend:app --reload --host 0.0.0.0 --port 8000
```

- Browse to: `http://localhost:8000/`
- WebSocket endpoint: `ws://localhost:8000/ws`
- HTTP polling fallback endpoint: `http://localhost:8000/sensor-data`

---

## Front-end configuration

In `static/app.js`, `CONFIG` controls:

- `WEBSOCKET_URL`: WebSocket endpoint (default `ws://localhost:8000/ws`)
- `API_URL`: HTTP polling endpoint (default `http://localhost:8000/sensor-data`)
- `UPDATE_INTERVAL`: Poll interval for the HTTP fallback
- `USE_WEBSOCKET`: If true, the UI attempts WebSocket connections (preferred)

Change these values if you serve the backend on another host/IP.

---

## Time Series Chart (historical)

The project now includes a Time Series Chart that records recent metric samples (every time the dashboard receives a new JSON sample via WebSocket or HTTP polling) and stores them in an in-memory client-side buffer.

Key details:
- The chart is displayed in the main UI under the “Time Series Chart” placeholder and is initialized by `static/app.js` and `Chart.js` (loaded from CDN).
- The chart records samples in `state.dataBuffer` (client-side) and holds up to 3 hours of samples by default (capped to avoid memory growth).
- The following metrics are selectable in the chart UI: Temperature (°C), Pressure (Pa), Altitude (m), and IAQ.
- Controls: Pause/Resume (temporarily stop updating the chart), Clear (clear the chart and buffer), Export CSV (download the buffer as CSV), Window selector (1m/5m/10m/All).

Developer notes:
- Chart.js is loaded from a CDN and configured to use a time-axis on the x-axis.
- Chart updates are performed client-side so the UI remains responsive; if you need server-side historical storage, implement a persistence store (e.g., SQLite) and a REST endpoint for retrieving historical data, and populate the chart from that endpoint on load.

- Performance limits: the front-end keeps a limited in-memory buffer and only renders a capped number of points for the chart to avoid browser lag. By default:
	- The client buffer stores up to `state.maxSamples` items (configured in `static/app.js`), default 3600 (seconds).
	- The chart renders only the last `MAX_DISPLAY_POINTS` points (configured in `static/app.js`, default 600). This avoids excessive DOM / canvas redraws and prevents the page height from growing endlessly.


---

## Replacing simulated data with real MCU serial data

Steps to integrate real data:

1) Replace `get_sensor_data()` with a parser that reads from your serial/USB interface (e.g., using `pyserial`).
	- Keep the output as a dictionary with keys: `timestamp`, `pressure`, `temperature`, `altitude`, `iaq`, `tvoc`, `eco2`, `ethanol`, `state`.

2) Option A — Run a serial reading background task inside the FastAPI server:
	- Use a background coroutine to read the serial interface and update `latest_data`.
	- Example: `async def sensor_updater()` becomes an async wrapper around a serial read or you use a separate thread and write into `latest_data`.

3) Option B — Use a separate service that publishes to a local socket, HTTP endpoint, or message queue (e.g. Redis) that `backend.py` then reads to update `latest_data`.

Important: Avoid blocking I/O on the event loop — either use `asyncio` or offload to a thread for blocking Serial I/O.

---

## Data flow explanation

1) Sensor plugin -> parsed dict -> backend writes to `latest_data` (shared global)
2) `sensor_updater()` updates `latest_data` on a schedule, or the serial reader writes driven by events.
3) WebSocket route (`/ws`) continuously sends `latest_data` to connected clients every second.
4) The UI (`static/app.js`) receives messages via WebSocket and updates the DOM.
5) If no WebSocket is available, the UI polls `GET /sensor-data` at `UPDATE_INTERVAL` and updates the DOM.

This architecture keeps the dashboard responsive even if WebSocket fails temporarily.

---

## Debugging & testing

- Check the HTTP endpoint (from a shell):
  - ```bash
  curl http://localhost:8000/sensor-data
  ```

- Test WebSocket (simple Python client):
  - ```bash
  python - <<'PY'
  import asyncio, websockets
  async def main():
		uri = 'ws://127.0.0.1:8000/ws'
		async with websockets.connect(uri) as ws:
			 for _ in range(2):
				  print(await ws.recv())
  asyncio.run(main())
  PY
  ```

- Use browser DevTools (Network tab) to inspect the WS and HTTP requests and ensure messages are arriving.

---

## Production & security considerations

- Do not use `allow_origins=["*"]` CORS in production — restrict to safe origins.
- Use HTTPS for production, use a reverse proxy (Nginx) or ASGI server with TLS.
- If exposing the WebSocket publicly, add authentication and session verification.
- Consider adding message signing or authorization to ensure only trusted clients consume the data.

---

## Next improvements (developer suggestions)

1) Replace the simulated generator with a real serial reader (e.g., `pyserial`).
2) Add persisting readings in a lightweight database (SQLite) and a REST endpoint for historical data.
3) Add Chart.js (or similar) into `index.html` and `static/app.js` to display historical charts and trends.
4) Add unit tests and CI to verify that endpoints return valid JSON and that the WebSocket broadcasts messages.
5) Add Dockerfile for easy deployment and reproduction if needed.

---

