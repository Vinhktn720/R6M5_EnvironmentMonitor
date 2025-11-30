import serial
import struct
import time
import datetime

class SerialReader:
    def __init__(self, port, baud, timeout=2.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        
        # Protocol Definitions
        self.START_BYTE = 0xAA
        self.END_BYTE = 0x55
        self.FULL_FRAME_SIZE = 37  # 1 Start + 35 Payload + 1 End
        self.STRUCT_FMT = '<HBBBBBfffffff'
        
        # State tracking (for Warming Up logic)
        self.last_valid_iaq = None
        self.last_valid_tvoc = None
        self.last_valid_eco2 = None
        self.last_valid_etoh = None
        self.has_valid_history = False 

    def parse_packet(self, payload):
        """Parses the payload and returns a dictionary. Returns None if failed."""
        try:
            unpacked = struct.unpack(self.STRUCT_FMT, payload)
            
            year, month, day, hour, minute, second = unpacked[0:6]
            pres, temp, alt = unpacked[6:9]
            iaq, tvoc, eco2, etoh = unpacked[9:13]

            # Format timestamp
            timestamp = f"{year}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}"

            state = "streaming"
            
            # Check if ZMOD4410 is ready (iaq >= 0)
            if iaq < 0:
                if self.has_valid_history:
                    iaq = self.last_valid_iaq if self.last_valid_iaq is not None else 0
                    tvoc = self.last_valid_tvoc if self.last_valid_tvoc is not None else 0
                    eco2 = self.last_valid_eco2 if self.last_valid_eco2 is not None else 0
                    etoh = self.last_valid_etoh if self.last_valid_etoh is not None else 0
                    state = "streaming"
                else:
                    state = "warming_up"
            else:
                self.has_valid_history = True
                self.last_valid_iaq = iaq
                self.last_valid_tvoc = tvoc
                self.last_valid_eco2 = eco2
                self.last_valid_etoh = etoh
                state = "streaming"

            return {
                "timestamp": timestamp,
                "pressure": float(pres),
                "temperature": float(temp),
                "altitude": float(alt),
                "iaq": float(iaq),
                "tvoc": float(tvoc),
                "eco2": float(eco2),
                "ethanol": float(etoh),
                "state": state
            }
        except struct.error:
            print("[ERR] Struct unpack failed")
            return None

    def run(self, data_queue, stop_event):
        """Main loop that auto-reconnects on failure."""
        print(f"--- Serial Reader Service Started on {self.port} ---")
        
        # OUTER LOOP: Handles Reconnection
        while not stop_event.is_set():
            try:
                # 1. Try to open the port
                ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
                time.sleep(2) # Wait for DTR/RTS stability
                ser.reset_input_buffer()
                print(f"--- Connected to {self.port} ---")
                
                buffer = bytearray()
                
                # INNER LOOP: Handles Reading
                while not stop_event.is_set():
                    # Read available bytes
                    if ser.in_waiting > 0:
                        buffer.extend(ser.read(ser.in_waiting))
                    
                    # Parse Frames
                    while len(buffer) >= self.FULL_FRAME_SIZE:
                        if buffer[0] != self.START_BYTE:
                            buffer.pop(0)
                            continue
                        
                        if buffer[self.FULL_FRAME_SIZE - 1] == self.END_BYTE:
                            payload = buffer[1:self.FULL_FRAME_SIZE - 1]
                            parsed_data = self.parse_packet(payload)
                            
                            if parsed_data:
                                # SEND ACK
                                try:
                                    ser.write(b'A')
                                except Exception:
                                    # If write fails, the connection is likely dead -> Raise error to trigger reconnect
                                    raise serial.SerialException("Write failed")
                                
                                data_queue.put(parsed_data)
                            
                            del buffer[:self.FULL_FRAME_SIZE]
                        else:
                            buffer.pop(0)
                    
                    time.sleep(0.005) # Fast polling when connected

            except (serial.SerialException, OSError) as e:
                # 2. Handle Disconnection
                print(f"Connection Lost: {e}. Retrying in 2s...")
                
                # Notify UI of disconnection
                data_queue.put({
                    "state": "disconnected", 
                    "timestamp": datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                })
                
                # 3. Slow down the rate (Wait 2 seconds before retrying)
                # Check stop_event every 0.5s so we can still exit quickly if needed
                for _ in range(4): 
                    if stop_event.is_set(): break
                    time.sleep(0.5)

            except Exception as e:
                print(f"Unexpected Error: {e}")
                time.sleep(1)
            
            finally:
                # Clean up resource before retrying
                if 'ser' in locals() and ser.is_open:
                    ser.close()

        print("--- Serial Reader Service Stopped ---")