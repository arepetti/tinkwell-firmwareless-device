#!/usr/bin/env python3
"""
test_thermostat_ota.py -- Integration tests for OTA push flow.

Requires a running thermostat binary.

Usage:
    pytest test_thermostat_ota.py -v
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


class TestOTAPush:

    def test_ota_status_idle(self):
        resp = send("GET", "/tw/ota/status")
        if not resp:
            pytest.skip("device not reachable")
        assert "state=0" in resp or "idle" in resp.lower()

    def test_ota_begin_requires_size(self):
        resp = send("POST", "/tw/ota/begin")
        if not resp:
            pytest.skip("device not reachable")
        assert "4.00" in resp or "bad" in resp.lower()

    def test_ota_begin_with_size(self):
        resp = send("POST", "/tw/ota/begin", "size=1024")
        if not resp:
            pytest.skip("device not reachable")
        assert "2.01" in resp or "created" in resp.lower()

    def test_ota_commit_without_begin_fails(self):
        resp = send("POST", "/tw/ota/commit")
        if not resp:
            pytest.skip("device not reachable")
        assert "4.08" in resp or "incomplete" in resp.lower()
