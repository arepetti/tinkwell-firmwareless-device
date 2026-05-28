#!/usr/bin/env python3
"""
test_applet_push.py -- Integration test: push a WASM applet and verify.

Usage:
    pytest test_applet_push.py -v
"""

import os
import socket

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


class TestAppletPush:

    def test_applet_status_initial(self):
        resp = send("GET", "/tw/applet/status")
        if not resp:
            pytest.skip("device not reachable")
        assert "state=" in resp

    def test_applet_push_begin(self):
        resp = send("POST", "/tw/applet/push", "size=256")
        if not resp:
            pytest.skip("device not reachable")
        assert "2.01" in resp or "created" in resp.lower()

    def test_applet_push_block(self):
        # Start a push session.
        send("POST", "/tw/applet/push", "size=256")
        # Send a dummy block.
        resp = send("POST", "/tw/applet/push", "A" * 128)
        if not resp:
            pytest.skip("device not reachable")

    def test_applet_commit(self):
        send("POST", "/tw/applet/push", "size=64")
        send("POST", "/tw/applet/push", "B" * 64)
        resp = send("POST", "/tw/applet/commit", "version=1.0.0")
        if not resp:
            pytest.skip("device not reachable")
        assert "2.04" in resp or "changed" in resp.lower()
