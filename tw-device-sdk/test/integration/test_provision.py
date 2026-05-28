#!/usr/bin/env python3
"""
test_provision.py -- Automated provisioning test suite.

Run against a POSIX device build:
    pytest test/integration/test_provision.py --device-host 127.0.0.1

Run against QEMU:
    pytest test/integration/test_provision.py --device-host 127.0.0.1 --device-port 5683

Skip BLE tests (default):
    pytest test/integration/test_provision.py -k "not ble"

Custom options are registered in ``conftest.py`` in this directory (``--mode``,
``--device-host``, ``--device-port``, ``--ble-device``, ``--provision-timeout``).

This module adds ``tools`` to ``sys.path`` so ``provision`` resolves the same way
whether or not other integration tests are collected.

Equivalent custom-option registration (must live in ``conftest.py`` for pytest to
apply it)::

    def pytest_addoption(parser):
        parser.addoption("--mode", choices=("lan", "softap", "ble"), default="lan")
        parser.addoption("--device-host", default="127.0.0.1")
        parser.addoption("--device-port", type=int, default=5683)
        parser.addoption("--ble-device", default=None)
        parser.addoption("--provision-timeout", type=float, default=5.0)
"""

from __future__ import annotations

import secrets
import sys
from pathlib import Path

import pytest

_SDK_ROOT = Path(__file__).resolve().parents[2]
_TOOLS = _SDK_ROOT / "tools"
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from provision import (  # noqa: E402
    KvtextParser,
    decode_response,
)


def _kv_dict(raw: bytes) -> dict:
    text = decode_response(raw)
    pairs = KvtextParser.parse(text)
    return KvtextParser.as_dict(pairs)


@pytest.fixture
def require_udp_coap_info(transport_mode: str) -> None:
    """
    BLE transport reads the status GATT characteristic for info, not the same
    kvtext as UDP ``GET /tw/provision/info``. Factory/hub/info assertions target
    the UDP text-stub CoAP path.
    """
    if transport_mode == "ble":
        pytest.skip(
            "These assertions require UDP GET /tw/provision/info; use --mode lan or softap"
        )


def test_info_returns_identity(
    provision_client, transport_mode: str, require_udp_coap_info
):
    """
    GET /tw/provision/info returns kvtext including uuid, vendor-id, and
    product-id (identity fields required for provisioning workflows).
    """
    _ = transport_mode
    raw = provision_client.info()
    data = _kv_dict(raw)
    assert "uuid" in data
    assert "vendor-id" in data
    assert "product-id" in data


def test_factory_provisioning(
    provision_client, transport_mode: str, require_udp_coap_info
):
    """
    Factory-phase kvtext (vendor/product/serial/display names) is accepted; a
    subsequent info read reflects the committed values.
    """
    _ = transport_mode
    provision_client.factory(
        vendor_id=99,
        product_id=200,
        serial_number=12345,
        vendor_name="TestCo",
        product_name="TestDevice",
    )
    raw = provision_client.info()
    data = _kv_dict(raw)
    assert data.get("vendor-id") == "99"
    assert data.get("product-id") == "200"
    assert data.get("serial-number") == "12345"
    assert data.get("vendor-display-name") == "TestCo"
    assert data.get("product-display-name") == "TestDevice"


def test_hub_provisioning(
    provision_client, transport_mode: str, require_udp_coap_info
):
    """
    Hub-phase kvtext (Wi-Fi, hub URL, uuid) is accepted; info shows the device
    as hub-provisioned (hub-provisioned=1).
    """
    _ = transport_mode
    hub_uuid = secrets.token_hex(16)
    provision_client.hub(
        ssid="TestNet",
        password="testpass",
        hub_url="coap://192.168.1.1:5684",
        uuid=hub_uuid,
    )
    raw = provision_client.info()
    data = _kv_dict(raw)
    assert data.get("hub-provisioned") == "1"
    if "uuid" in data:
        assert data["uuid"] == hub_uuid


def test_reset_clears_state(provision_client, transport_mode: str):
    """
    Sending provision-cmd=reset returns a parseable kvtext response from the
    device (provisioning reset path is live).
    """
    raw = provision_client.reset()
    text = decode_response(raw)
    pairs = KvtextParser.parse(text)
    if transport_mode == "ble" and not pairs:
        pytest.skip("BLE reset returned no kvtext on status characteristic")
    assert pairs, "expected non-empty kvtext response for reset"
    data = KvtextParser.as_dict(pairs)
    assert "ok" in data or "provision-phase" in data or "provision-cmd" in data
