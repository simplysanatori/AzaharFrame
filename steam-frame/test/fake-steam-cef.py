#!/usr/bin/env python3
# AzaharFrame flatfix: a stand-in for steam-cef.py in mock runs (AZAHARFRAME_TEST_CEF). Steam and
# SteamVR are never asked. Every move of the window's frame answers at once ("docked"), except the
# dashboard moves numbered in AZ_FAKE_CEF_HANG_DASHBOARD (comma-separated, counting from 1), which
# never answer, as when vrcmd never sees the window's overlay. AZ_FAKE_CEF_COUNT names the file
# that keeps the count.
import os
import sys
import time

args = sys.argv[1:]
if len(args) >= 2 and args[0] == "--screen":
    where = args[1]
    if where == "dashboard":
        count_file = os.environ.get("AZ_FAKE_CEF_COUNT", "")
        n = 1
        if count_file:
            try:
                with open(count_file) as f:
                    n = int(f.read().strip() or "0") + 1
            except (OSError, ValueError):
                n = 1
            with open(count_file, "w") as f:
                f.write(str(n))
        if str(n) in os.environ.get("AZ_FAKE_CEF_HANG_DASHBOARD", "").split(","):
            time.sleep(600)  # never answers; the program stops it
        print(f"dashboard: docked (fake, move {n}), dashboard hidden")
    else:
        print(f"{where}: docked (fake)")
else:
    print("null")
sys.exit(0)
