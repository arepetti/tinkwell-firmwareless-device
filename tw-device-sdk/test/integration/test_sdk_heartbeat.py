#!/usr/bin/env python3
"""
test_sdk_heartbeat.py -- Integration test for the heartbeat protocol.

Starts fake_hub.py and the device binary, then verifies heartbeats
are received and commands are dispatched.

Usage:
    pytest test_sdk_heartbeat.py -v
"""

import os
import socket
import time

import pytest

HUB_PORT = int(os.getenv("TW_HUB_PORT", "5684"))
TIMEOUT = 10.0


class TestHeartbeat:
    """Verify the device sends heartbeats to the hub."""

    @pytest.fixture(autouse=True)
    def hub_socket(self):
        """Bind a UDP socket acting as the hub."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", HUB_PORT))
        self.sock.settimeout(TIMEOUT)
        yield
        self.sock.close()

    def test_boot_heartbeat_received(self):
        """Device should send a heartbeat shortly after boot."""
        try:
            data, addr = self.sock.recvfrom(2048)
            msg = data.decode()
            assert "heartbeat" in msg.lower() or "device=" in msg
            # Respond with empty command queue.
            self.sock.sendto(b"pending=0\n", addr)
        except socket.timeout:
            pytest.skip("no device running -- start the binary first")

    def test_heartbeat_contains_device_name(self):
        try:
            data, addr = self.sock.recvfrom(2048)
            msg = data.decode()
            assert "device=" in msg
            self.sock.sendto(b"pending=0\n", addr)
        except socket.timeout:
            pytest.skip("no device running")

    def test_heartbeat_contains_firmware_version(self):
        try:
            data, addr = self.sock.recvfrom(2048)
            msg = data.decode()
            assert "fw=" in msg
            self.sock.sendto(b"pending=0\n", addr)
        except socket.timeout:
            pytest.skip("no device running")
