#!/usr/bin/env python3
"""
ESP32 DOOM Player - Two modes:
  1. Stream mode: Display DOOM stream from ESP32 WiFi + send keys via serial
  2. Keys-only mode: Only send keyboard input via serial (for use with LCD display)
"""

import sys
import base64
import struct
import threading
import time
import argparse

try:
    import pygame
    import serial
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install pygame pyserial")
    sys.exit(1)

# Only import requests if needed (for stream mode)
requests = None

# Stream configuration
STREAM_WIDTH = 320
STREAM_HEIGHT = 200
SCALE = 2  # Display scale factor

# Key mapping: pygame key -> DOOM key code (sent over serial)
# DOOM key codes from doomdef.h
KEYD_RIGHTARROW = 0xae
KEYD_LEFTARROW = 0xac
KEYD_UPARROW = 0xad
KEYD_DOWNARROW = 0xaf
KEYD_ESCAPE = 27
KEYD_ENTER = 13
KEYD_SPACE = 32
KEYD_RCTRL = 0x9d
KEYD_RSHIFT = 0x36
KEYD_RALT = 0x38
KEYD_TAB = 9

KEY_MAP = {
    pygame.K_UP: KEYD_UPARROW,
    pygame.K_DOWN: KEYD_DOWNARROW,
    pygame.K_LEFT: KEYD_LEFTARROW,
    pygame.K_RIGHT: KEYD_RIGHTARROW,
    pygame.K_RETURN: KEYD_ENTER,
    pygame.K_SPACE: KEYD_SPACE,
    pygame.K_LCTRL: KEYD_RCTRL,
    pygame.K_RCTRL: KEYD_RCTRL,
    pygame.K_LSHIFT: KEYD_RSHIFT,
    pygame.K_RSHIFT: KEYD_RSHIFT,
    pygame.K_LALT: KEYD_RALT,
    pygame.K_RALT: KEYD_RALT,
    pygame.K_ESCAPE: KEYD_ESCAPE,
    pygame.K_TAB: KEYD_TAB,
    pygame.K_1: ord('1'),
    pygame.K_2: ord('2'),
    pygame.K_3: ord('3'),
    pygame.K_4: ord('4'),
    pygame.K_5: ord('5'),
    pygame.K_6: ord('6'),
    pygame.K_7: ord('7'),
    pygame.K_w: KEYD_UPARROW,
    pygame.K_s: KEYD_DOWNARROW,
    pygame.K_a: KEYD_LEFTARROW,
    pygame.K_d: KEYD_RIGHTARROW,
}


class DoomPlayer:
    def __init__(self, serial_port, baud_rate=115200, esp_ip=None, keys_only=False):
        self.esp_ip = esp_ip
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.keys_only = keys_only
        
        self.palette = [(i, i, i) for i in range(256)]  # Default grayscale
        self.frame_buffer = bytearray(STREAM_WIDTH * STREAM_HEIGHT)
        self.frame_lock = threading.Lock()
        self.running = True
        self.fps = 0
        self.frame_count = 0
        self.last_fps_time = time.time()
        
        # Serial connection
        self.serial = None
        self.serial_lock = threading.Lock()
        
        # Initialize pygame
        pygame.init()
        
        if keys_only:
            # Small window just for capturing keys
            self.screen = pygame.display.set_mode((400, 200))
            pygame.display.set_caption("ESP32 DOOM - Keys Only Mode (LCD)")
        else:
            self.screen = pygame.display.set_mode(
                (STREAM_WIDTH * SCALE, STREAM_HEIGHT * SCALE)
            )
            pygame.display.set_caption("ESP32 DOOM - Stream Mode")
        
        self.clock = pygame.time.Clock()
        self.surface = pygame.Surface((STREAM_WIDTH, STREAM_HEIGHT))
        
        # Font for status
        self.font = pygame.font.Font(None, 24)
        self.font_large = pygame.font.Font(None, 36)
    
    def connect_serial(self):
        """Connect to ESP32 serial port"""
        try:
            self.serial = serial.Serial(
                self.serial_port, 
                self.baud_rate,
                timeout=0.1
            )
            print(f"Serial connected: {self.serial_port} @ {self.baud_rate}")
            return True
        except Exception as e:
            print(f"Serial connection failed: {e}")
            return False
    
    def send_key(self, doom_key, pressed):
        """Send key event over serial"""
        if self.serial and self.serial.is_open:
            # Protocol: 'K' <keycode> <state>
            # state: 1 = pressed, 0 = released
            cmd = bytes([ord('K'), doom_key & 0xFF, 1 if pressed else 0])
            with self.serial_lock:
                try:
                    self.serial.write(cmd)
                    print(f"Key sent: {doom_key} {'DOWN' if pressed else 'UP'}")
                except Exception as e:
                    print(f"Serial write error: {e}")
    
    def rle_decode(self, data):
        """Decode RLE compressed frame"""
        result = bytearray()
        i = 0
        while i < len(data) - 1:
            count = data[i]
            value = data[i + 1]
            result.extend([value] * count)
            i += 2
        return result
    
    def process_sse_data(self, b64_data):
        """Process base64-encoded SSE data"""
        try:
            raw = base64.b64decode(b64_data)
            if len(raw) < 1:
                return
            
            msg_type = chr(raw[0])
            payload = raw[1:]
            
            if msg_type == 'P':  # Palette
                if len(payload) >= 768:
                    self.palette = [
                        (payload[i*3], payload[i*3+1], payload[i*3+2])
                        for i in range(256)
                    ]
                    print("Palette updated")
            
            elif msg_type == 'F':  # Frame (RLE)
                decoded = self.rle_decode(payload)
                if len(decoded) >= STREAM_WIDTH * STREAM_HEIGHT:
                    with self.frame_lock:
                        self.frame_buffer = decoded[:STREAM_WIDTH * STREAM_HEIGHT]
                    self.frame_count += 1
        
        except Exception as e:
            print(f"Data processing error: {e}")
    
    def stream_thread(self):
        """Background thread to receive SSE stream"""
        stream_url = f"http://{self.esp_ip}/stream"
        print(f"Connecting to stream: {stream_url}")
        
        while self.running:
            try:
                response = requests.get(stream_url, stream=True, timeout=10)
                print("Stream connected!")
                
                for line in response.iter_lines():
                    if not self.running:
                        break
                    if line:
                        line_str = line.decode('utf-8', errors='ignore')
                        if line_str.startswith('data:'):
                            b64_data = line_str[5:].strip()
                            if b64_data:
                                self.process_sse_data(b64_data)
                
            except requests.exceptions.RequestException as e:
                print(f"Stream error: {e}")
                if self.running:
                    print("Reconnecting in 2 seconds...")
                    time.sleep(2)
            except Exception as e:
                print(f"Stream thread error: {e}")
                time.sleep(1)
    
    def render_frame(self):
        """Render current frame to pygame surface"""
        if self.keys_only:
            # Keys-only mode: just show status screen
            self.screen.fill((30, 30, 50))
            
            # Title
            title = self.font_large.render("ESP32 DOOM - Keys Only Mode", True, (255, 200, 0))
            self.screen.blit(title, (self.screen.get_width()//2 - title.get_width()//2, 20))
            
            # Serial status
            if self.serial and self.serial.is_open:
                status_color = (0, 255, 0)
                status_text = f"Serial: Connected ({self.serial_port})"
            else:
                status_color = (255, 0, 0)
                status_text = "Serial: DISCONNECTED"
            
            status = self.font.render(status_text, True, status_color)
            self.screen.blit(status, (self.screen.get_width()//2 - status.get_width()//2, 70))
            
            # Instructions
            instructions = [
                "Game is displayed on LCD screen",
                "Keys captured from this window",
                "",
                "Controls: Arrow/WASD, Ctrl=Fire, Space=Use",
                "Press Ctrl+Q to quit"
            ]
            y = 110
            for line in instructions:
                text = self.font.render(line, True, (180, 180, 180))
                self.screen.blit(text, (self.screen.get_width()//2 - text.get_width()//2, y))
                y += 22
        else:
            # Stream mode: render the frame
            with self.frame_lock:
                pixels = pygame.PixelArray(self.surface)
                for y in range(STREAM_HEIGHT):
                    for x in range(STREAM_WIDTH):
                        idx = self.frame_buffer[y * STREAM_WIDTH + x]
                        color = self.palette[idx]
                        pixels[x, y] = color
                del pixels
            
            # Scale and blit to screen
            scaled = pygame.transform.scale(
                self.surface, 
                (STREAM_WIDTH * SCALE, STREAM_HEIGHT * SCALE)
            )
            self.screen.blit(scaled, (0, 0))
            
            # Update FPS counter
            now = time.time()
            if now - self.last_fps_time >= 1.0:
                self.fps = self.frame_count
                self.frame_count = 0
                self.last_fps_time = now
            
            # Draw status
            serial_status = "Serial: OK" if (self.serial and self.serial.is_open) else "Serial: DISCONNECTED"
            status_text = f"FPS: {self.fps} | {serial_status}"
            text_surface = self.font.render(status_text, True, (0, 255, 0))
            self.screen.blit(text_surface, (10, 10))
        
        pygame.display.flip()
    
    def handle_events(self):
        """Handle pygame events including keyboard"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
            
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_q and (event.mod & pygame.KMOD_CTRL):
                    self.running = False
                elif event.key in KEY_MAP:
                    self.send_key(KEY_MAP[event.key], True)
            
            elif event.type == pygame.KEYUP:
                if event.key in KEY_MAP:
                    self.send_key(KEY_MAP[event.key], False)
    
    def run(self):
        """Main game loop"""
        # Connect serial
        if not self.connect_serial():
            print("Warning: Running without serial control")
        
        # Start stream thread only in stream mode
        if not self.keys_only:
            global requests
            try:
                import requests as req
                requests = req
            except ImportError:
                print("ERROR: 'requests' module required for stream mode")
                print("Install with: pip install requests")
                print("Or use --keys-only mode for LCD display")
                pygame.quit()
                return
            
            stream_thread = threading.Thread(target=self.stream_thread, daemon=True)
            stream_thread.start()
        
        print("\nControls:")
        print("  Arrow keys / WASD - Move")
        print("  Ctrl - Fire")
        print("  Space - Use/Open")
        print("  Shift - Run")
        print("  Enter - Select")
        print("  Escape - Menu")
        print("  Tab - Automap")
        print("  1-7 - Weapons")
        print("  Ctrl+Q - Quit")
        print()
        
        if self.keys_only:
            print("MODE: Keys only - Game displayed on LCD")
        else:
            print("MODE: Stream - Game displayed in this window")
        print()
        
        # Main loop
        while self.running:
            self.handle_events()
            self.render_frame()
            self.clock.tick(60)  # Cap at 60 FPS display
        
        # Cleanup
        if self.serial:
            self.serial.close()
        pygame.quit()


def main():
    parser = argparse.ArgumentParser(
        description="ESP32 DOOM Player - Stream viewer or keys-only controller",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Stream mode (WiFi video + serial keys):
    python doom_player.py --ip 192.168.1.92 --port COM3
    
  Keys-only mode (for LCD display):
    python doom_player.py --keys-only --port COM3
"""
    )
    parser.add_argument(
        "--ip", "-i",
        default="192.168.1.92",
        help="ESP32 IP address for stream mode (default: 192.168.1.92)"
    )
    parser.add_argument(
        "--port", "-p",
        default="COM3",
        help="Serial port (default: COM3)"
    )
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=115200,
        help="Serial baud rate (default: 115200)"
    )
    parser.add_argument(
        "--scale", "-s",
        type=int,
        default=2,
        help="Display scale factor for stream mode (default: 2)"
    )
    parser.add_argument(
        "--keys-only", "-k",
        action="store_true",
        help="Keys-only mode: only send keyboard input (use with LCD display)"
    )
    
    args = parser.parse_args()
    
    global SCALE
    SCALE = args.scale
    
    print("=" * 50)
    print("ESP32 DOOM Player")
    print("=" * 50)
    
    if args.keys_only:
        print("Mode: KEYS ONLY (for LCD display)")
    else:
        print("Mode: STREAM (WiFi video)")
        print(f"ESP32 IP: {args.ip}")
    
    print(f"Serial Port: {args.port}")
    print(f"Baud Rate: {args.baud}")
    
    if not args.keys_only:
        print(f"Scale: {args.scale}x")
    
    print("=" * 50)
    
    player = DoomPlayer(
        serial_port=args.port, 
        baud_rate=args.baud,
        esp_ip=args.ip if not args.keys_only else None,
        keys_only=args.keys_only
    )
    player.run()


if __name__ == "__main__":
    main()
