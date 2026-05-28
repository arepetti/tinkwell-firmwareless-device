#!/usr/bin/env python3
"""
fake_sensor.py -- Feeds simulated sensor data to the POSIX PAL.

The POSIX I2C PAL reads temperature/humidity from environment
variables TW_FAKE_TEMP and TW_FAKE_HUMID.  This script generates
realistic sensor traces and exports them, or writes to a pipe.

Usage:
    # Static value (tenths of C / tenths of %RH):
    python fake_sensor.py --temp 215 --humid 450

    # Sinusoidal wave (simulates day/night cycle):
    python fake_sensor.py --wave --period 60 --min 180 --max 260

    # Random walk:
    python fake_sensor.py --random --start 210 --step 5
"""

import argparse
import math
import os
import random
import sys
import time


def static_mode(temp: int, humid: int):
    """Set constant sensor values."""
    os.environ["TW_FAKE_TEMP"] = str(temp)
    os.environ["TW_FAKE_HUMID"] = str(humid)
    print(f"TW_FAKE_TEMP={temp}  TW_FAKE_HUMID={humid}")
    print("Press Ctrl+C to stop")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass


def wave_mode(period: float, tmin: int, tmax: int, humid: int):
    """Sinusoidal temperature variation."""
    t0 = time.time()
    try:
        while True:
            elapsed = time.time() - t0
            phase = (elapsed / period) * 2 * math.pi
            temp = int(tmin + (tmax - tmin) * (0.5 + 0.5 * math.sin(phase)))
            os.environ["TW_FAKE_TEMP"] = str(temp)
            os.environ["TW_FAKE_HUMID"] = str(humid)
            sys.stdout.write(f"\rtemp={temp/10:.1f}°C  humid={humid/10:.1f}%  ")
            sys.stdout.flush()
            time.sleep(1)
    except KeyboardInterrupt:
        print()


def random_mode(start: int, step: int, humid: int):
    """Random walk temperature."""
    temp = start
    try:
        while True:
            temp += random.randint(-step, step)
            temp = max(0, min(500, temp))
            os.environ["TW_FAKE_TEMP"] = str(temp)
            os.environ["TW_FAKE_HUMID"] = str(humid)
            sys.stdout.write(f"\rtemp={temp/10:.1f}°C  humid={humid/10:.1f}%  ")
            sys.stdout.flush()
            time.sleep(1)
    except KeyboardInterrupt:
        print()


def main():
    parser = argparse.ArgumentParser(description="Fake sensor data generator")
    parser.add_argument("--temp", type=int, default=215)
    parser.add_argument("--humid", type=int, default=450)
    parser.add_argument("--wave", action="store_true")
    parser.add_argument("--period", type=float, default=60)
    parser.add_argument("--min", type=int, default=180, dest="tmin")
    parser.add_argument("--max", type=int, default=260, dest="tmax")
    parser.add_argument("--random", action="store_true")
    parser.add_argument("--start", type=int, default=210)
    parser.add_argument("--step", type=int, default=5)
    args = parser.parse_args()

    if args.wave:
        wave_mode(args.period, args.tmin, args.tmax, args.humid)
    elif args.random:
        random_mode(args.start, args.step, args.humid)
    else:
        static_mode(args.temp, args.humid)


if __name__ == "__main__":
    main()
