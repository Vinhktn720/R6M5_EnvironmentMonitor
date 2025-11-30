from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
import asyncio
import threading
import queue
import json
import uvicorn
import datetime
from pathlib import Path
from typing import List

# Import your new separate logic file
from serial_reader import SerialReader

app = FastAPI()
BASE_DIR = Path(__file__).resolve().parent

# Serve files from /static directory
app.mount("/static", StaticFiles(directory=str(BASE_DIR / "static")), name="static")

# Allow CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# GLOBAL STATE
latest_data = None
latest_lock = threading.Lock()

# Serial Thread Management
serial_queue = queue.Queue()
serial_thread = None
serial_stop_event = threading.Event()

serial_config = {
    'port': None,
    'baud': 9600,
    'timeout': 2.0,
    'enabled': False,
}

# --- Connection Manager for WebSockets ---
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        # Send latest data immediately on connect so UI isn't blank
        with latest_lock:
            if latest_data:
                await websocket.send_text(json.dumps(latest_data))

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        # Convert to JSON once
        json_msg = json.dumps(message)
        # Send to all connected clients
        for connection in self.active_connections:
            try:
                await connection.send_text(json_msg)
            except Exception:
                pass

manager = ConnectionManager()


# --- Background Task to Process Queue ---
# CHANGED: Now processes EVERY packet to support buffering flush
async def queue_processor():
    global latest_data
    while True:
        try:
            # Check if there is data in the queue
            if not serial_queue.empty():
                # Get the next item (non-blocking)
                item = serial_queue.get()
                
                # 1. Update Global Latest (for HTTP polling fallback)
                with latest_lock:
                    latest_data = item
                
                # 2. LOGGING (As requested)
                ts = item.get('timestamp', datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
                state = item.get('state', 'unknown')
                
                if state == 'disconnected':
                    print(f"[{ts}] Status: DISCONNECTED - Retrying...")
                else:
                    tvoc = item.get('tvoc', 0.0)
                    iaq = item.get('iaq', 0.0)
                    temp = item.get('temperature', 0.0)
                    pres = item.get('pressure', 0.0)
                    alt = item.get('altitude', 0.0)

                    print(f"[{ts}] dev=ck-ra6m5-01 sensor=zmod4410 tvoc={tvoc:.0f}ppb iaq={iaq:.0f}")
                    print(f"[{ts}] dev=ck-ra6m5-01 sensor=icp-10101 temp={temp:.2f}C pres={pres:.0f}Pa alt={alt:.2f}m")

                # 3. BROADCAST IMMEDIATELY (Fix for buffering issue)
                # We send this specific packet to the UI immediately.
                await manager.broadcast(item)
                
                # Small sleep to yield control, allowing high throughput but preventing freezing
                await asyncio.sleep(0.001)
            else:
                # If queue is empty, sleep longer to save CPU
                await asyncio.sleep(0.05)

        except Exception as e:
            print("Queue processing error:", e)
            await asyncio.sleep(0.1)

@app.on_event("startup")
async def startup_event():
    # Start the queue processor
    asyncio.create_task(queue_processor())

@app.get("/sensor-data")
async def get_http_data():
    """HTTP Polling endpoint (Fallback)"""
    with latest_lock:
        if latest_data is None:
            return {"state": "waiting_for_connection"}
        return latest_data

@app.get('/serial-config')
async def get_serial_config():
    return serial_config

@app.post('/serial-config')
async def post_serial_config(req: Request):
    global serial_thread, serial_stop_event
    
    body = await req.json()
    port = body.get('port')
    baud = int(body.get('baud', 9600))
    timeout = float(body.get('timeout', 2.0))
    
    # 1. Stop existing thread if it's NOT the same port
    # Actually, if we are re-configuring, we should always stop the old one.
    if serial_thread and serial_thread.is_alive():
        print("Stopping existing serial thread...")
        serial_stop_event.set()
        serial_thread.join(timeout=3)
        serial_stop_event.clear()

    # 2. Update Config
    serial_config['port'] = port
    serial_config['baud'] = baud
    serial_config['timeout'] = timeout
    serial_config['enabled'] = True if port else False

    # 3. Start New Thread
    if port:
        reader = SerialReader(port, baud, timeout)
        serial_stop_event.clear()
        
        serial_thread = threading.Thread(
            target=reader.run, 
            args=(serial_queue, serial_stop_event), 
            daemon=True
        )
        serial_thread.start()
        return {'ok': True, 'message': f'Connected to {port}'}
    
    return {'ok': False, 'message': 'No port provided'}

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    current_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{current_time}] WebSocket client connected")
    
    try:
        while True:
            # We just keep the connection open here.
            # Data sending is now handled entirely by queue_processor -> manager.broadcast
            # We can wait for client messages (ping/pong)
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)
        disc_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{disc_time}] WebSocket client disconnected")

@app.get("/")
async def index():
    index_path = BASE_DIR / "index.html"
    if index_path.exists():
        return HTMLResponse(index_path.read_text(encoding="utf-8"))
    return HTMLResponse("<h1>index.html not found</h1>", status_code=404)

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)