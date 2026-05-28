#!/usr/bin/env python3
"""
test_thermostat_coap.py -- Integration tests for thermostat CoAP resources.

Requires a running thermostat binary (POSIX or QEMU).

Usage:
    ./build/thermostat &
    pytest test_thermostat_coap.py -v
"""

import os
import socket

import pytest

HOST = os.getenv("TW_DEVICE_HOST", "127.0.0.1")
PORT = int(os.getenv("TW_DEVICE_PORT", "5683"))
TIMEOUT = 2.0


def query(method: str, path: str, payload: str = "") -> str:
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


class TestThermostatResources:

    def test_read_temperature(self):
        resp = query("GET", "/tw/sensor/temperature")
        assert resp, "device not reachable"
        # Default fake temp is 21.5°C = 215 tenths.
        assert "21" in resp

    def test_read_humidity(self):
        resp = query("GET", "/tw/sensor/humidity")
        assert resp

    def test_read_mode_default_is_auto(self):
        resp = query("GET", "/tw/mode")
        assert resp
        assert "auto" in resp.lower() or "2" in resp

    def test_set_mode_off(self):
        resp = query("PUT", "/tw/mode", "0")
        assert resp

    def test_read_relay_state(self):
        resp = query("GET", "/tw/relay")
        assert resp

    def test_read_status(self):
        resp = query("GET", "/tw/status")
        assert resp
        assert "thermostat" in resp.lower() or "fw=" in resp
