"""
Pill Reminder - MicroPython for ESP8266
=======================================

Two buttons (AM/PM) that light up at scheduled times.
Press the button to acknowledge you took your pill.

Hardware:
  - ESP8266 (Wemos D1 Mini or similar)
  - 2x Illuminated arcade buttons

Author: Your friendly AI assistant
License: Do whatever you want with it
"""

import machine
import network
import ntptime
import time
import json
import socket

# =============================================================================
# CONFIGURATION - EDIT THESE
# =============================================================================

WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

# Pill times (24-hour format)
AM_PILL_HOUR = 7
AM_PILL_MINUTE = 30

PM_PILL_HOUR = 18
PM_PILL_MINUTE = 0

# Timezone offset from UTC (e.g., EST = -5, PST = -8)
# Adjust for daylight saving manually or use a smarter approach
TIMEZONE_OFFSET = -5  # Eastern Time

# How long to keep the light on if not acknowledged (hours)
# After this, it stays on but we log it as "missed"
REMINDER_TIMEOUT_HOURS = 4

# =============================================================================
# PIN CONFIGURATION
# =============================================================================

# Wemos D1 Mini pin mapping
# D1 = GPIO5, D2 = GPIO4, D5 = GPIO14, D6 = GPIO12

PIN_AM_BUTTON = 5   # D1 - AM button switch input
PIN_PM_BUTTON = 4   # D2 - PM button switch input
PIN_AM_LED = 14     # D5 - AM button LED output
PIN_PM_LED = 12     # D6 - PM button LED output

# =============================================================================
# GLOBAL STATE
# =============================================================================

state = {
    "am": {
        "lit": False,
        "taken": False,
        "taken_time": None,
        "lit_time": None
    },
    "pm": {
        "lit": False,
        "taken": False,
        "taken_time": None,
        "lit_time": None
    },
    "last_reset_day": -1,
    "wifi_connected": False,
    "time_synced": False
}

# History for the last 7 days
history = []

# =============================================================================
# HARDWARE SETUP
# =============================================================================

def setup_pins():
    """Initialize GPIO pins"""
    global am_button, pm_button, am_led, pm_led
    
    # Buttons with internal pullup - pressed = LOW
    am_button = machine.Pin(PIN_AM_BUTTON, machine.Pin.IN, machine.Pin.PULL_UP)
    pm_button = machine.Pin(PIN_PM_BUTTON, machine.Pin.IN, machine.Pin.PULL_UP)
    
    # LEDs as outputs - HIGH = ON
    am_led = machine.Pin(PIN_AM_LED, machine.Pin.OUT)
    pm_led = machine.Pin(PIN_PM_LED, machine.Pin.OUT)
    
    # Start with LEDs off
    am_led.off()
    pm_led.off()
    
    print("Pins configured")

# =============================================================================
# WIFI & TIME
# =============================================================================

def connect_wifi():
    """Connect to WiFi network"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if wlan.isconnected():
        state["wifi_connected"] = True
        print(f"Already connected: {wlan.ifconfig()[0]}")
        return True
    
    print(f"Connecting to {WIFI_SSID}...")
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    
    # Wait up to 20 seconds for connection
    for _ in range(40):
        if wlan.isconnected():
            state["wifi_connected"] = True
            print(f"Connected! IP: {wlan.ifconfig()[0]}")
            return True
        time.sleep(0.5)
    
    print("WiFi connection failed")
    state["wifi_connected"] = False
    return False

def sync_time():
    """Sync time via NTP"""
    try:
        ntptime.settime()
        state["time_synced"] = True
        print(f"Time synced: {get_local_time_str()}")
        return True
    except Exception as e:
        print(f"NTP sync failed: {e}")
        state["time_synced"] = False
        return False

def get_local_time():
    """Get local time tuple adjusted for timezone"""
    utc = time.time()
    local = utc + (TIMEZONE_OFFSET * 3600)
    return time.localtime(local)

def get_local_time_str():
    """Get formatted local time string"""
    t = get_local_time()
    return f"{t[0]:04d}-{t[1]:02d}-{t[2]:02d} {t[3]:02d}:{t[4]:02d}:{t[5]:02d}"

def get_time_str_short():
    """Get short time string HH:MM"""
    t = get_local_time()
    return f"{t[3]:02d}:{t[4]:02d}"

# =============================================================================
# PILL REMINDER LOGIC
# =============================================================================

def check_pill_times():
    """Check if it's time to light up a button"""
    t = get_local_time()
    hour = t[3]
    minute = t[4]
    day = t[2]
    
    # Reset at midnight
    if day != state["last_reset_day"]:
        reset_daily()
        state["last_reset_day"] = day
    
    # Check AM pill time
    if not state["am"]["taken"]:
        if hour == AM_PILL_HOUR and minute >= AM_PILL_MINUTE:
            if not state["am"]["lit"]:
                light_am(True)
                state["am"]["lit_time"] = get_time_str_short()
        elif hour > AM_PILL_HOUR:
            if not state["am"]["lit"]:
                light_am(True)
                state["am"]["lit_time"] = get_time_str_short()
    
    # Check PM pill time
    if not state["pm"]["taken"]:
        if hour == PM_PILL_HOUR and minute >= PM_PILL_MINUTE:
            if not state["pm"]["lit"]:
                light_pm(True)
                state["pm"]["lit_time"] = get_time_str_short()
        elif hour > PM_PILL_HOUR:
            if not state["pm"]["lit"]:
                light_pm(True)
                state["pm"]["lit_time"] = get_time_str_short()

def reset_daily():
    """Reset state at midnight"""
    # Save today's record to history before reset
    if state["am"]["lit_time"] or state["pm"]["lit_time"]:
        record = {
            "date": get_local_time_str()[:10],
            "am_taken": state["am"]["taken"],
            "am_time": state["am"]["taken_time"],
            "pm_taken": state["pm"]["taken"],
            "pm_time": state["pm"]["taken_time"]
        }
        history.append(record)
        # Keep only last 7 days
        while len(history) > 7:
            history.pop(0)
    
    # Reset for new day
    state["am"] = {"lit": False, "taken": False, "taken_time": None, "lit_time": None}
    state["pm"] = {"lit": False, "taken": False, "taken_time": None, "lit_time": None}
    
    light_am(False)
    light_pm(False)
    
    print("Daily reset complete")

def light_am(on):
    """Control AM LED"""
    state["am"]["lit"] = on
    if on:
        am_led.on()
        print("AM LED ON - time to take morning pill!")
    else:
        am_led.off()

def light_pm(on):
    """Control PM LED"""
    state["pm"]["lit"] = on
    if on:
        pm_led.on()
        print("PM LED ON - time to take evening pill!")
    else:
        pm_led.off()

def acknowledge_am():
    """AM button was pressed"""
    if state["am"]["lit"]:
        light_am(False)
        state["am"]["taken"] = True
        state["am"]["taken_time"] = get_time_str_short()
        print(f"AM pill taken at {state['am']['taken_time']}")

def acknowledge_pm():
    """PM button was pressed"""
    if state["pm"]["lit"]:
        light_pm(False)
        state["pm"]["taken"] = True
        state["pm"]["taken_time"] = get_time_str_short()
        print(f"PM pill taken at {state['pm']['taken_time']}")

# =============================================================================
# BUTTON HANDLING
# =============================================================================

# Debounce tracking
last_am_press = 0
last_pm_press = 0
DEBOUNCE_MS = 300

def check_buttons():
    """Check button states with debounce"""
    global last_am_press, last_pm_press
    
    now = time.ticks_ms()
    
    # AM button (active LOW with pullup)
    if am_button.value() == 0:
        if time.ticks_diff(now, last_am_press) > DEBOUNCE_MS:
            last_am_press = now
            acknowledge_am()
    
    # PM button (active LOW with pullup)
    if pm_button.value() == 0:
        if time.ticks_diff(now, last_pm_press) > DEBOUNCE_MS:
            last_pm_press = now
            acknowledge_pm()

# =============================================================================
# WEB SERVER
# =============================================================================

def start_web_server():
    """Start a simple HTTP server for status queries"""
    addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(1)
    s.setblocking(False)
    print(f"Web server listening on port 80")
    return s

def handle_web_request(server_socket):
    """Handle incoming HTTP requests (non-blocking)"""
    try:
        cl, addr = server_socket.accept()
    except OSError:
        return  # No connection waiting
    
    try:
        cl.settimeout(2)
        request = cl.recv(1024).decode()
        
        # Parse the request path
        path = "/"
        if "GET " in request:
            path = request.split("GET ")[1].split(" ")[0]
        
        # Route the request
        if path == "/status" or path == "/status/":
            response = json.dumps({
                "time": get_local_time_str(),
                "am": state["am"],
                "pm": state["pm"],
                "wifi": state["wifi_connected"],
                "time_synced": state["time_synced"]
            })
            content_type = "application/json"
        
        elif path == "/history" or path == "/history/":
            response = json.dumps(history)
            content_type = "application/json"
        
        elif path == "/config" or path == "/config/":
            response = json.dumps({
                "am_time": f"{AM_PILL_HOUR:02d}:{AM_PILL_MINUTE:02d}",
                "pm_time": f"{PM_PILL_HOUR:02d}:{PM_PILL_MINUTE:02d}",
                "timezone": TIMEZONE_OFFSET
            })
            content_type = "application/json"
        
        else:
            # Serve a simple HTML page
            response = generate_html()
            content_type = "text/html"
        
        # Send response
        cl.send("HTTP/1.1 200 OK\r\n")
        cl.send(f"Content-Type: {content_type}\r\n")
        cl.send("Connection: close\r\n\r\n")
        cl.send(response)
    
    except Exception as e:
        print(f"Web request error: {e}")
    
    finally:
        cl.close()

def generate_html():
    """Generate a simple status HTML page"""
    am_status = "✅ Taken" if state["am"]["taken"] else ("💊 TAKE NOW" if state["am"]["lit"] else "⏳ Waiting")
    pm_status = "✅ Taken" if state["pm"]["taken"] else ("💊 TAKE NOW" if state["pm"]["lit"] else "⏳ Waiting")
    
    am_color = "#4CAF50" if state["am"]["taken"] else ("#FF9800" if state["am"]["lit"] else "#9E9E9E")
    pm_color = "#4CAF50" if state["pm"]["taken"] else ("#FF9800" if state["pm"]["lit"] else "#9E9E9E")
    
    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Pill Reminder</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="30">
    <style>
        body {{ font-family: Arial, sans-serif; text-align: center; padding: 20px; background: #1a1a2e; color: #eee; }}
        .pill-box {{ display: inline-block; margin: 20px; padding: 30px; border-radius: 15px; min-width: 150px; }}
        .am {{ background: {am_color}; }}
        .pm {{ background: {pm_color}; }}
        h1 {{ color: #fff; }}
        .time {{ font-size: 0.9em; color: #aaa; }}
        .status {{ font-size: 1.5em; font-weight: bold; }}
        .taken-time {{ font-size: 0.8em; margin-top: 10px; }}
    </style>
</head>
<body>
    <h1>💊 Pill Reminder</h1>
    <p class="time">{get_local_time_str()}</p>
    
    <div class="pill-box am">
        <div>☀️ Morning</div>
        <div class="status">{am_status}</div>
        <div class="taken-time">{state["am"]["taken_time"] or ""}</div>
    </div>
    
    <div class="pill-box pm">
        <div>🌙 Evening</div>
        <div class="status">{pm_status}</div>
        <div class="taken-time">{state["pm"]["taken_time"] or ""}</div>
    </div>
    
    <p style="margin-top: 40px; font-size: 0.8em;">
        <a href="/status" style="color: #4fc3f7;">JSON Status</a> |
        <a href="/history" style="color: #4fc3f7;">History</a>
    </p>
</body>
</html>"""
    return html

# =============================================================================
# MAIN LOOP
# =============================================================================

def main():
    """Main entry point"""
    print("\n" + "="*50)
    print("    PILL REMINDER - Starting up...")
    print("="*50 + "\n")
    
    # Initialize hardware
    setup_pins()
    
    # Quick LED test
    print("Testing LEDs...")
    am_led.on()
    time.sleep(0.3)
    am_led.off()
    pm_led.on()
    time.sleep(0.3)
    pm_led.off()
    print("LED test complete")
    
    # Connect to WiFi
    if not connect_wifi():
        print("Running in offline mode - time won't be accurate!")
    else:
        # Sync time
        for attempt in range(3):
            if sync_time():
                break
            print(f"Retry NTP sync ({attempt + 1}/3)...")
            time.sleep(2)
    
    # Start web server
    server = None
    if state["wifi_connected"]:
        server = start_web_server()
    
    print("\n" + "="*50)
    print("    READY! Main loop starting...")
    print(f"    AM pill time: {AM_PILL_HOUR:02d}:{AM_PILL_MINUTE:02d}")
    print(f"    PM pill time: {PM_PILL_HOUR:02d}:{PM_PILL_MINUTE:02d}")
    print("="*50 + "\n")
    
    # Sync time periodically (every 6 hours)
    last_ntp_sync = time.time()
    NTP_SYNC_INTERVAL = 6 * 60 * 60
    
    # Main loop
    while True:
        try:
            # Check buttons
            check_buttons()
            
            # Check pill times
            if state["time_synced"]:
                check_pill_times()
            
            # Handle web requests
            if server:
                handle_web_request(server)
            
            # Periodic NTP resync
            if state["wifi_connected"] and time.time() - last_ntp_sync > NTP_SYNC_INTERVAL:
                sync_time()
                last_ntp_sync = time.time()
            
            # Small delay to prevent CPU spinning
            time.sleep_ms(50)
        
        except KeyboardInterrupt:
            print("\nShutting down...")
            am_led.off()
            pm_led.off()
            break
        
        except Exception as e:
            print(f"Error in main loop: {e}")
            time.sleep(1)

# =============================================================================
# RUN
# =============================================================================

if __name__ == "__main__":
    main()
