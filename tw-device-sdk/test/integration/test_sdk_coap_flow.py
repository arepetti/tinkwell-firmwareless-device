#!/usr/bin/env python3
"""
test_sdk_coap_flow.py -- Integration test for CoAP resource dispatch.

Starts the native POSIX binary, sends UDP CoAP-like requests,
and verifies responses.

Requires a running device binary (or use pytest fixtures).

Usage:
    pytest test_sdk_coap_flow.py -v
"""

import os
import socket
import subprocess
import time

import pytest

DEVICE_HOST = os.getenv("TW_DEVICE_HOST", "127.0.0.1")
DEVICE_PORT = int(os.getenv("TW_DEVICE_PORT", "5683"))
TIMEOUT = 2.0


def send_coap(method: str, path: str, payload: str = "") -> str:
    """Send a simple text-mode CoAP request and return the response."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)
    msg = f"{method} {path}\n{payload}"
    sock.sendto(msg.encode(), (DEVICE_HOST, DEVICE_PORT))
    try:
        data, _ = sock.recvfrom(2048)
        return data.decode()
    except socket.timeout:
        return ""
    finally:
        sock.close()


class TestCoAPResources:
    """Test suite for the CoAP resource dispatch (text protocol stub)."""

    def test_get_temperature(self):
        resp = send_coap("GET", "/tw/sensor/temperature")
        assert resp, "no response from device"
        # The POSIX PAL returns the default fake temperature.
        assert "215" in resp or "temperature" in resp.lower()

    def test_get_humidity(self):
        resp = send_coap("GET", "/tw/sensor/humidity")
        assert resp, "no response from device"

    def test_get_mode(self):
        resp = send_coap("GET", "/tw/mode")
        assert resp, "no response from device"
        assert any(m in resp.lower() for m in ["off", "on", "auto", "mode"])

    def test_get_status(self):
        resp = send_coap("GET", "/tw/status")
        assert resp, "no response from device"

    def test_unknown_resource_returns_error(self):
        resp = send_coap("GET", "/tw/nonexistent")
        # Expect a 4.04-like response or empty.
        if resp:
            assert "4.04" in resp or "not found" in resp.lower()

    def test_put_relay(self):
        resp = send_coap("PUT", "/tw/relay", "1")
        assert resp, "no response from device"
