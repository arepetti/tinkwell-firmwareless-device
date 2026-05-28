#!/usr/bin/env python3
"""
qemu_run.py -- Launch the firmware in QEMU for testing.

Wraps `idf.py qemu` with network port forwarding and optional
automatic test execution.

Usage:
    # Basic launch:
    python qemu_run.py --project examples/thermostat/esp-idf

    # With CoAP port forwarding:
    python qemu_run.py --project examples/thermostat/esp-idf --forward 5683

    # Run, wait for boot, then execute tests:
    python qemu_run.py --project examples/thermostat/esp-idf \\
                       --test pytest test/integration/
"""

import argparse
import os
import signal
import subprocess
import sys
import time


def find_idf_path():
    idf = os.environ.get("IDF_PATH")
    if idf and os.path.isdir(idf):
        return idf
    default = os.path.expanduser("~/esp/esp-idf")
    if os.path.isdir(default):
        return default
    print("Error: IDF_PATH not set and ~/esp/esp-idf not found",
          file=sys.stderr)
    sys.exit(1)


def run_qemu(project_dir: str, forward_port: int, timeout: int):
    """Start QEMU with port forwarding."""
    nic_arg = f"user,hostfwd=udp::{forward_port}-:{forward_port}"

    cmd = [
        sys.executable, "-m", "idf_py",
        "--project-dir", project_dir,
        "qemu", "monitor",
        "--", "-nic", nic_arg,
    ]

    print(f"[qemu] starting: {' '.join(cmd)}")
    print(f"[qemu] CoAP forwarded: host:{forward_port} -> guest:{forward_port}")

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    start = time.time()
    booted = False

    try:
        for line in iter(proc.stdout.readline, ""):
            sys.stdout.write(line)
            if "entering main loop" in line.lower():
                booted = True
                print(f"\n[qemu] device booted ({time.time() - start:.1f}s)")
                break
            if time.time() - start > timeout:
                print(f"\n[qemu] timeout after {timeout}s", file=sys.stderr)
                break
    except KeyboardInterrupt:
        pass

    return proc, booted


def run_tests(test_cmd: str, port: int):
    """Execute test suite against the running QEMU instance."""
    env = os.environ.copy()
    env["TW_DEVICE_HOST"] = "127.0.0.1"
    env["TW_DEVICE_PORT"] = str(port)

    print(f"\n[qemu] running tests: {test_cmd}")
    result = subprocess.run(test_cmd, shell=True, env=env)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description="QEMU launcher")
    parser.add_argument("--project", required=True, help="ESP-IDF project dir")
    parser.add_argument("--forward", type=int, default=5683,
                        help="UDP port to forward")
    parser.add_argument("--timeout", type=int, default=60,
                        help="Boot timeout in seconds")
    parser.add_argument("--test", type=str, default="",
                        help="Test command to run after boot")
    args = parser.parse_args()

    proc, booted = run_qemu(args.project, args.forward, args.timeout)

    rc = 0
    if booted and args.test:
        rc = run_tests(args.test, args.forward)

    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=5)

    sys.exit(rc)


if __name__ == "__main__":
    main()
