# Pill Reminder

A simple ESP8266-based device with two illuminated buttons (AM/PM) that light up at scheduled times to remind you to take your medication.

## Features

- **AM button** lights up at 7:30 AM (configurable)
- **PM button** lights up at 6:00 PM (configurable)
- Press the button to acknowledge you took your pill (turns off light)
- Light stays on until you press it (no more "did I take it?")
- Web interface to check status from any device
- JSON API for integration with other systems
- 7-day history tracking
- Automatic NTP time sync

## Hardware Required

| Part | Approx Cost | Notes |
|------|-------------|-------|
| Wemos D1 Mini (ESP8266) | $4-6 | Or any ESP8266 dev board |
| Hi-Link HLK-PM01 | $3-5 | AC-DC 5V 3W isolated power supply |
| 2x 24mm LED Arcade Buttons | $5-8 | 5V illuminated, with microswitch |
| Project enclosure | $5-10 | Plastic project box |
| Misc wire, solder | $0 | You probably have this |
| **Total** | **~$20-30** | |

## Wiring

See `WIRING.txt` for detailed diagram.

Quick summary:
```
Hi-Link HLK-PM01:
  AC in  → Mains (be careful!)
  +Vo    → 5V rail
  -Vo    → GND rail

Wemos D1 Mini:
  5V     ← 5V rail
  GND    ← GND rail
  D1     ← AM button switch (other side to GND)
  D2     ← PM button switch (other side to GND)
  D5     → AM button LED+ (LED- to GND)
  D6     → PM button LED+ (LED- to GND)
```

## Software Setup

### 1. Install MicroPython on ESP8266

First, install `esptool`:
```bash
pip install esptool
```

Download the latest MicroPython firmware for ESP8266:
https://micropython.org/download/esp8266/

Erase and flash:
```bash
# Find your serial port (e.g., /dev/ttyUSB0 on Linux, COM3 on Windows)
esptool.py --port /dev/ttyUSB0 erase_flash
esptool.py --port /dev/ttyUSB0 --baud 460800 write_flash --flash_size=detect 0 esp8266-20xxxxxx-vx.xx.bin
```

### 2. Configure the Code

Edit `main.py` and change these settings near the top:

```python
WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

AM_PILL_HOUR = 7
AM_PILL_MINUTE = 30

PM_PILL_HOUR = 18
PM_PILL_MINUTE = 0

TIMEZONE_OFFSET = -5  # EST. Use -8 for PST, etc.
```

### 3. Upload Files

You can use any of these tools:

**Option A: ampy (command line)**
```bash
pip install adafruit-ampy

ampy --port /dev/ttyUSB0 put boot.py
ampy --port /dev/ttyUSB0 put main.py
```

**Option B: Thonny IDE (graphical)**
1. Download Thonny: https://thonny.org/
2. Connect to your ESP8266
3. Copy files to the device

**Option C: WebREPL (wireless, after initial setup)**
1. Connect via serial first
2. Run `import webrepl_setup` and follow prompts
3. Use WebREPL client: http://micropython.org/webrepl/

### 4. Test It

After uploading, reset the device. You should see:
- LEDs flash briefly (test)
- Serial output showing WiFi connection
- Web server starting

Try accessing `http://<device-ip>/` in a browser.

## Web Interface

Once running, access these URLs:

| URL | Description |
|-----|-------------|
| `http://<ip>/` | Nice HTML status page |
| `http://<ip>/status` | JSON current state |
| `http://<ip>/history` | JSON 7-day history |
| `http://<ip>/config` | JSON configuration |

Example `/status` response:
```json
{
  "time": "2024-11-15 08:45:23",
  "am": {
    "lit": false,
    "taken": true,
    "taken_time": "07:42",
    "lit_time": "07:30"
  },
  "pm": {
    "lit": false,
    "taken": false,
    "taken_time": null,
    "lit_time": null
  },
  "wifi": true,
  "time_synced": true
}
```

## Integration Ideas

Since it has a JSON API, you could:

- **Home Assistant**: Add a REST sensor to show pill status on your dashboard
- **Day Trading App**: Show a reminder in your trading interface
- **Phone Script**: Hit the API and send yourself a push notification
- **Grafana**: Log history to InfluxDB and visualize trends

Example curl:
```bash
curl http://192.168.1.100/status | jq
```

## Troubleshooting

**LEDs don't light up:**
- Check wiring polarity (+ to GPIO, - to GND)
- Some arcade buttons are 12V, make sure you got 5V ones
- Try a 100-200 ohm resistor in series if too dim/bright

**WiFi won't connect:**
- Double-check SSID and password (case-sensitive!)
- ESP8266 only supports 2.4GHz WiFi
- Try moving closer to router

**Time is wrong:**
- Check `TIMEZONE_OFFSET` setting
- NTP uses UTC, you need to offset for your timezone
- Daylight saving time needs manual adjustment

**Buttons don't respond:**
- Check the switch wiring (NO to GPIO, COM to GND)
- Internal pullup should pull the pin HIGH, pressing pulls LOW
- Add some serial prints to debug

## Safety Notes

⚠️ **MAINS VOLTAGE IS DANGEROUS**

- Always work with AC power disconnected
- Use the Hi-Link module as intended - it's isolated
- Double-check all connections before powering on
- Use proper strain relief on AC wires
- Consider a fuse on the AC input
- Ensure enclosure is fully insulated

## License

Do whatever you want with this. No warranty, no liability.
If it helps you remember your meds, that's great!
