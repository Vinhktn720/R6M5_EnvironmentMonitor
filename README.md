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

