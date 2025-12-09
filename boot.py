# boot.py - runs on every boot
# This file is executed first, before main.py

import gc
gc.collect()

# Disable debug output on TX pin if you need it for other purposes
# import esp
# esp.osdebug(None)

# If you want WebREPL for wireless debugging/file transfer:
# import webrepl
# webrepl.start()

print("Boot complete, starting main.py...")
