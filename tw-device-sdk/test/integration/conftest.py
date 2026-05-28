"""
Shared pytest configuration for integration tests (custom CLI options and fixtures).
"""

from __future__ import annotations

import socket
import sys
from pathlib import Path

_DEVICE_ROOT = Path(__file__).resolve().parents[3]
_SCRIPTS = _DEVICE_ROOT / "scripts"
if str(_SCRIPTS) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS))

from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    from provision import Transport


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--mode",
        action="store",
        default="lan",
        choices=("lan", "softap", "ble"),
        help="Transport: lan (default), softap, or ble",
    )
    parser.addoption(
        "--device-host",
        action="store",
        default="127.0.0.1",
        help="UDP target host for lan/softap (default: 127.0.0.1)",
    )
    parser.addoption(
        "--device-port",
        action="store",
        type=int,
        default=5683,
        help="UDP port for lan/softap (default: 5683)",
    )
    parser.addoption(
        "--ble-device",
        action="store",
        default=None,
        metavar="ADDR",
        help="BLE device address (required for --mode ble)",
    )
    parser.addoption(
        "--provision-timeout",
        action="store",
        type=float,
        default=5.0,
        help="Socket/BLE I/O timeout in seconds (default: 5)",
    )


@pytest.fixture
def transport_mode(request: pytest.FixtureRequest) -> str:
    """Selected transport mode: lan, softap, or ble."""
    return request.config.getoption("--mode")


@pytest.fixture
def transport(request: pytest.FixtureRequest) -> "Transport":
    """
    CoAP or BLE transport to the device under test.

    Skips if the device is unreachable (UDP) or BLE is not usable (missing bleak,
    missing --ble-device, or connection failure).
    """
    from provision import BleTransport, UdpTransport

    mode: str = request.config.getoption("--mode")
    timeout: float = request.config.getoption("--provision-timeout")

    if mode == "ble":
        pytest.importorskip("bleak", reason="bleak not installed; pip install bleak")
        addr = request.config.getoption("--ble-device")
        if not addr:
            pytest.skip("BLE mode requires --ble-device ADDR")
        t = BleTransport(addr, timeout)
        try:
            t.send_request("GET", "/tw/provision/info", "")
        except Exception as exc:
            pytest.skip("BLE device not available: {}".format(exc))
        return t

    host = request.config.getoption("--device-host")
    port = int(request.config.getoption("--device-port"))
    t = UdpTransport(host, port, timeout)
    try:
        t.send_request("GET", "/tw/provision/info", "")
    except socket.timeout:
        pytest.skip(
            "device not reachable at {}:{} ({} mode)".format(host, port, mode)
        )
    except OSError as exc:
        pytest.skip("device I/O error at {}:{}: {}".format(host, port, exc))
    return t


@pytest.fixture
def provision_client(transport: "Transport"):
    """High-level provisioning client bound to the active transport."""
    from provision import ProvisionClient

    return ProvisionClient(transport)
