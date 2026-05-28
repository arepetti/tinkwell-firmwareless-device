#!/usr/bin/env python3
"""
test_applet_hotswap.py -- Integration test: replace a running applet.

Verifies that pushing a new applet while one is running correctly
unloads the old module and loads the new one.

Usage:
    pytest test_applet_hotswap.py -v
"""

import os
import socket
import time

import pytest

HOST = os.getenv("TW_DEVICE_HOST", "127.0.0.1")
PORT = int(os.getenv("TW_DEVICE_PORT", "5683"))
TIMEOUT = 2.0


def send(method: str, path: str, payload: str = "") -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)
    sock.sendto(f"{method} {path}\n{payload}".encode(), (HOST, PORT))
    try:
        data, _ = sock.recvfrom(2048)
        return data.decode()
    except socket.timeout:
        return ""
    finally:
        sock.close()


def push_applet(version: str, size: int = 64):
    """Helper: push a dummy applet through the full cycle."""
    send("POST", "/tw/applet/push", f"size={size}")
    send("POST", "/tw/applet/push", "X" * size)
    return send("POST", "/tw/applet/commit", f"version={version}")


class TestAppletHotswap:

    def test_initial_push(self):
        resp = push_applet("1.0.0")
        if not resp:
            pytest.skip("device not reachable")
        assert "2.04" in resp

    def test_second_push_replaces_first(self):
        push_applet("1.0.0")
        time.sleep(0.5)

        resp = push_applet("2.0.0")
        if not resp:
            pytest.skip("device not reachable")
        assert "2.04" in resp

        status = send("GET", "/tw/applet/status")
        assert "2.0.0" in status

    def test_status_shows_running_after_push(self):
        push_applet("3.0.0")
        time.sleep(0.5)

        status = send("GET", "/tw/applet/status")
        if not status:
            pytest.skip("device not reachable")
        # state=3 is TW_APPLET_RUNNING.
        assert "state=3" in status or "running" in status.lower()
