#!/usr/bin/env python3
"""
TW Firmwareless interactive provisioning CLI.

Sends binary CoAP (via ``tw coap send``) over LAN / SoftAP or raw kvtext over
BLE GATT to Tinkwell firmwareless devices. Supports factory and hub provisioning
phases, device info queries, and provisioning reset.

Requires Python 3.7+ and the ``tw`` CLI (Tinkwell CLI) on PATH for LAN/SoftAP
CoAP interactions.  Optional dependency: bleak (``pip install bleak``) for
``ble`` transport mode.

See ``--help`` on each subcommand for options and examples.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
from typing import Any, Callable, Dict, List, Optional, Tuple, Union  # noqa: F401

# ---------------------------------------------------------------------------
# Constants (wire paths, BLE UUIDs)
# ---------------------------------------------------------------------------

PATH_PROVISION_SET = "/tw/provision/set"
PATH_PROVISION_INFO = "/tw/provision/info"

# 128-bit service UUID (advertised); used for BLE scan filtering.
BLE_SERVICE_UUID = "a9e12001-e237-4f6a-9f5b-bc3b9a1c3d6a"
# Bluetooth SIG base UUID for 16-bit UUIDs 0x1201 / 0x1202 (config / status).
BLE_CHAR_CONFIG = "00001201-0000-1000-8000-00805f9b34fb"
BLE_CHAR_STATUS = "00001202-0000-1000-8000-00805f9b34fb"

# Safe chunk size for BLE writes before MTU negotiation (ATT payload).
BLE_WRITE_CHUNK = 200


# ---------------------------------------------------------------------------
# Terminal colors (ANSI + NO_COLOR + fallback)
# ---------------------------------------------------------------------------


def _color_enabled() -> bool:
    if os.environ.get("NO_COLOR", ""):
        return False
    return sys.stdout.isatty()


class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    CYAN = "\033[36m"


def _c(code: str, text: str) -> str:
    if not _color_enabled():
        return text
    return code + text + Colors.RESET


def err(msg: str) -> None:
    print(_c(Colors.RED, msg), file=sys.stderr)


def warn(msg: str) -> None:
    print(_c(Colors.YELLOW, msg), file=sys.stderr)


def info_line(msg: str) -> None:
    print(_c(Colors.CYAN, msg))


def ok(msg: str) -> None:
    print(_c(Colors.GREEN, msg))


# ---------------------------------------------------------------------------
# kvtext builder / parser
# ---------------------------------------------------------------------------


class KvtextBuilder:
    """Build kvtext payloads (key=value lines, LF-terminated)."""

    _KEY_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")

    def __init__(self) -> None:
        self._lines: List[str] = []

    def add(self, key: str, value: Union[str, int]) -> None:
        if not self._KEY_RE.match(key):
            raise ValueError("invalid kvtext key: {!r}".format(key))
        sval = str(value)
        line = "{}={}".format(key, sval)
        if len(line.encode("utf-8")) + 1 > 128:
            raise ValueError("kvtext line exceeds 128 bytes: {!r}".format(key))
        self._lines.append(line)

    def build(self) -> str:
        if not self._lines:
            return ""
        return "\n".join(self._lines) + "\n"

    def factory_payload(
        self,
        vendor_id: Optional[int] = None,
        product_id: Optional[int] = None,
        vendor_name: Optional[str] = None,
        product_name: Optional[str] = None,
        variant: Optional[int] = None,
        serial_number: Optional[int] = None,
        uuid: Optional[str] = None,
        key_type: Optional[int] = None,
        key: Optional[str] = None,
    ) -> str:
        self._lines = []
        self.add("provision-phase", "factory")
        if vendor_id is not None:
            self.add("vendor-id", vendor_id)
        if product_id is not None:
            self.add("product-id", product_id)
        if vendor_name is not None:
            self.add("vendor-display-name", vendor_name)
        if product_name is not None:
            self.add("product-display-name", product_name)
        if variant is not None:
            self.add("variant", variant)
        if serial_number is not None:
            self.add("serial-number", serial_number)
        if uuid is not None:
            self.add("uuid", uuid)
        if key_type is not None:
            self.add("key-type", key_type)
        if key is not None:
            self.add("psk", key)
        self.add("provision-cmd", "commit")
        return self.build()

    def hub_payload(
        self,
        ssid: Optional[str] = None,
        password: Optional[str] = None,
        hub_url: Optional[str] = None,
        uuid: Optional[str] = None,
    ) -> str:
        self._lines = []
        self.add("provision-phase", "hub")
        if ssid is not None:
            self.add("ssid", ssid)
        if password is not None:
            self.add("password", password)
        if hub_url is not None:
            self.add("hub-url", hub_url)
        if uuid is not None:
            self.add("uuid", uuid)
        self.add("provision-cmd", "commit")
        return self.build()

    def reset_payload(self) -> str:
        self._lines = []
        self.add("provision-cmd", "reset")
        return self.build()


class KvtextParser:
    """Parse kvtext responses into an ordered list of (key, value) pairs."""

    @staticmethod
    def parse(text: str) -> List[Tuple[str, str]]:
        result: List[Tuple[str, str]] = []
        for raw_line in text.splitlines():
            line = raw_line.strip()
            if not line:
                continue
            if line[0] in "#;":
                continue
            if line.startswith("//"):
                continue
            if line.startswith("[") and line.endswith("]"):
                continue
            if "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip()
            if key:
                result.append((key, value))
        return result

    @staticmethod
    def as_dict(pairs: List[Tuple[str, str]]) -> Dict[str, str]:
        return {k: v for k, v in pairs}


# ---------------------------------------------------------------------------
# Transports
# ---------------------------------------------------------------------------


class Transport:
    """Abstract transport: send request, return raw response bytes."""

    def send_request(self, method: str, path: str, payload: str) -> bytes:
        raise NotImplementedError


class TwCoapTransport(Transport):
    """
    Binary CoAP transport using the ``tw coap send`` CLI command.

    All LAN/SoftAP CoAP interactions are proxied through the Tinkwell CLI,
    which implements RFC 7252 binary CoAP.  The tw CLI must be on PATH.
    """

    def __init__(self, host: str, port: int, timeout: float) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self._tw = shutil.which("tw")
        if not self._tw:
            err("'tw' CLI not found on PATH. Install the Tinkwell CLI first.")
            raise SystemExit(2)

    def send_request(self, method: str, path: str, payload: str) -> bytes:
        uri = "coap://{}:{}{}".format(self.host, self.port, path)
        cmd: list = [self._tw, "coap", "send", method.upper(), uri,
                     "--non-interactive"]
        if payload:
            cmd.extend(["--data", payload])
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout + 5,
            )
        except FileNotFoundError as e:
            raise RuntimeError("tw CLI not found: {}".format(e)) from e
        except subprocess.TimeoutExpired as e:
            raise TimeoutError("tw coap send timed out") from e

        if result.returncode != 0:
            stderr_msg = result.stderr.strip()
            if stderr_msg:
                raise RuntimeError("tw coap send failed: {}".format(stderr_msg))
            raise RuntimeError("tw coap send failed (exit code {})".format(
                result.returncode))

        return result.stdout.encode("utf-8") if result.stdout else b""


class BleTransport(Transport):
    """
    BLE: write raw kvtext to config characteristic 0x1201; read status 0x1202.

    The device firmware parses kvtext on the config characteristic; it does not
    accept the UDP CoAP line prefix. For ``info``, the status characteristic is
    read (full identity is available via LAN/SoftAP ``GET /tw/provision/info``).
    """

    def __init__(self, device_addr: str, timeout: float) -> None:
        self.device_addr = device_addr
        self.timeout = timeout

    def _ensure_bleak(self):
        try:
            import bleak  # noqa: F401
        except ImportError as e:
            err(
                "bleak is not installed. Install with: pip install bleak\n"
                "BLE transport requires the bleak library."
            )
            raise SystemExit(2) from e

    async def _write_chunks(self, client: Any, data: bytes) -> None:
        for i in range(0, len(data), BLE_WRITE_CHUNK):
            chunk = data[i : i + BLE_WRITE_CHUNK]
            await client.write_gatt_char(BLE_CHAR_CONFIG, chunk, response=True)

    def send_request(self, method: str, path: str, payload: str) -> bytes:
        self._ensure_bleak()
        from bleak import BleakClient

        m = method.upper()
        if m == "GET" and path == PATH_PROVISION_INFO:
            return asyncio.run(self._do_get(BleakClient))
        if m == "POST" and path == PATH_PROVISION_SET:
            return asyncio.run(self._do_post(BleakClient, payload))
        raise ValueError(
            "BLE transport only supports GET {} and POST {} (got {} {})".format(
                PATH_PROVISION_INFO, PATH_PROVISION_SET, method, path
            )
        )

    async def _do_get(self, bleak_client_cls: Any) -> bytes:
        async with bleak_client_cls(self.device_addr, timeout=self.timeout) as client:
            return await client.read_gatt_char(BLE_CHAR_STATUS)

    async def _do_post(self, bleak_client_cls: Any, payload: str) -> bytes:
        body = payload.encode("utf-8")
        async with bleak_client_cls(self.device_addr, timeout=self.timeout) as client:
            await self._write_chunks(client, body)
            try:
                return await client.read_gatt_char(BLE_CHAR_STATUS)
            except Exception:
                return b""


# ---------------------------------------------------------------------------
# Provision client
# ---------------------------------------------------------------------------


class ProvisionClient:
    """High-level provisioning operations over a ``Transport``."""

    def __init__(self, transport: Transport) -> None:
        self.transport = transport
        self._kb = KvtextBuilder()

    def info(self) -> bytes:
        return self.transport.send_request("GET", PATH_PROVISION_INFO, "")

    def factory(
        self,
        vendor_id: Optional[int] = None,
        product_id: Optional[int] = None,
        vendor_name: Optional[str] = None,
        product_name: Optional[str] = None,
        variant: Optional[int] = None,
        serial_number: Optional[int] = None,
        uuid: Optional[str] = None,
        key_type: Optional[int] = None,
        key: Optional[str] = None,
    ) -> bytes:
        body = self._kb.factory_payload(
            vendor_id=vendor_id,
            product_id=product_id,
            vendor_name=vendor_name,
            product_name=product_name,
            variant=variant,
            serial_number=serial_number,
            uuid=uuid,
            key_type=key_type,
            key=key,
        )
        return self.transport.send_request("POST", PATH_PROVISION_SET, body)

    def hub(
        self,
        ssid: Optional[str] = None,
        password: Optional[str] = None,
        hub_url: Optional[str] = None,
        uuid: Optional[str] = None,
    ) -> bytes:
        body = self._kb.hub_payload(
            ssid=ssid,
            password=password,
            hub_url=hub_url,
            uuid=uuid,
        )
        return self.transport.send_request("POST", PATH_PROVISION_SET, body)

    def reset(self) -> bytes:
        body = self._kb.reset_payload()
        return self.transport.send_request("POST", PATH_PROVISION_SET, body)


# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------


def decode_response(raw: bytes) -> str:
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("utf-8", errors="replace")


def emit_result(
    json_mode: bool,
    verbose: bool,
    command: str,
    raw: bytes,
    *,
    extra: Optional[Dict[str, Any]] = None,
) -> None:
    text = decode_response(raw)
    pairs = KvtextParser.parse(text)
    data = KvtextParser.as_dict(pairs)

    if verbose:
        info_line("[verbose] raw response (text):")
        sys.stdout.write(text)
        if not text.endswith("\n"):
            sys.stdout.write("\n")

    if json_mode:
        out: Dict[str, Any] = {
            "ok": True,
            "command": command,
            "raw_text": text,
            "kv": data,
        }
        if extra:
            out.update(extra)
        print(json.dumps(out, indent=2))
        return

    if not pairs:
        ok(text.strip() if text.strip() else "(empty response)")
        return

    for k, v in pairs:
        print("{}: {}".format(k, v))


def emit_error(json_mode: bool, message: str, code: int = 1) -> None:
    if json_mode:
        print(json.dumps({"ok": False, "error": message}, indent=2))
    else:
        err(message)
    sys.exit(code)


# ---------------------------------------------------------------------------
# BLE scan
# ---------------------------------------------------------------------------


def run_ble_scan(timeout: float, json_mode: bool) -> None:
    try:
        from bleak import BleakScanner
    except ImportError as e:
        err("bleak is not installed. Install with: pip install bleak")
        raise SystemExit(2) from e

    async def _scan():
        devices = await BleakScanner.discover(timeout=timeout)

        found = []
        for d in devices:
            name = d.name or ""
            uuids = [str(u).lower() for u in (d.metadata.get("uuids") or [])]
            if name.startswith("TW-") or any(
                BLE_SERVICE_UUID.lower() == u for u in uuids
            ):
                found.append(
                    {
                        "address": d.address,
                        "name": name,
                        "rssi": d.rssi,
                    }
                )
        return found

    devices = asyncio.run(_scan())
    if json_mode:
        print(json.dumps({"ok": True, "devices": devices}, indent=2))
        return

    if not devices:
        warn("No TW-* provisioning devices found.")
        return

    info_line("Address              Name                RSSI")
    for d in devices:
        print(
            "{:20} {:19} {}".format(
                d["address"],
                d["name"] or "(no name)",
                d["rssi"],
            )
        )


# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------


def _factory_kwargs(ns: argparse.Namespace) -> Dict[str, Any]:
    return {
        "vendor_id": getattr(ns, "vendor_id", None),
        "product_id": getattr(ns, "product_id", None),
        "vendor_name": getattr(ns, "vendor_name", None),
        "product_name": getattr(ns, "product_name", None),
        "variant": getattr(ns, "variant", None),
        "serial_number": getattr(ns, "serial_number", None),
        "uuid": getattr(ns, "uuid", None),
        "key_type": getattr(ns, "key_type", None),
        "key": getattr(ns, "key", None),
    }


def _hub_kwargs(ns: argparse.Namespace) -> Dict[str, Any]:
    return {
        "ssid": getattr(ns, "ssid", None),
        "password": getattr(ns, "password", None),
        "hub_url": getattr(ns, "hub_url", None),
        "uuid": getattr(ns, "uuid", None),
    }


def add_factory_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--vendor-id", type=int, default=None, help="Vendor ID (integer)")
    p.add_argument("--product-id", type=int, default=None, help="Product ID (integer)")
    p.add_argument("--vendor-name", default=None, help="Maps to vendor-display-name")
    p.add_argument("--product-name", default=None, help="Maps to product-display-name")
    p.add_argument("--variant", type=int, default=None, help="Variant byte 0-255")
    p.add_argument("--serial-number", type=int, default=None, help="Factory serial")
    p.add_argument(
        "--uuid",
        default=None,
        help="32-char hex UUID (16 bytes)",
    )
    p.add_argument(
        "--key-type",
        type=int,
        default=None,
        help="0=none, 1=Ed25519, 2=PSK",
    )
    p.add_argument(
        "--key",
        default=None,
        help="Hex-encoded key material (maps to psk= in kvtext)",
    )


def add_hub_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--ssid", default=None, help="Wi-Fi SSID")
    p.add_argument("--password", default=None, help="Wi-Fi password")
    p.add_argument("--hub-url", default=None, help="Hub endpoint, e.g. coap://host:5684")
    p.add_argument(
        "--uuid",
        default=None,
        help="32-char hex UUID (optional hub phase)",
    )


def run_command(
    transport: Transport,
    command: Optional[str],
    ns: argparse.Namespace,
    json_mode: bool,
    verbose: bool,
) -> None:
    client = ProvisionClient(transport)

    if command == "info":
        raw = client.info()
        emit_result(json_mode, verbose, "info", raw)
        return

    if command == "factory":
        try:
            raw = client.factory(**_factory_kwargs(ns))
        except ValueError as e:
            emit_error(json_mode, str(e), 2)
        emit_result(json_mode, verbose, "factory", raw)
        return

    if command == "hub":
        try:
            raw = client.hub(**_hub_kwargs(ns))
        except ValueError as e:
            emit_error(json_mode, str(e), 2)
        emit_result(json_mode, verbose, "hub", raw)
        return

    if command == "reset":
        raw = client.reset()
        emit_result(json_mode, verbose, "reset", raw)
        return

    emit_error(json_mode, "unknown command: {!r}".format(command), 2)


def interactive_loop(
    make_transport: Callable[[], Transport],
    json_mode: bool,
    verbose: bool,
    build_ns: Callable[[], argparse.Namespace],
) -> None:
    try:
        import readline  # noqa: F401
    except ImportError:
        pass

    info_line("Interactive mode. Type 'help', 'quit', or a command (info, factory, hub, reset).")

    while True:
        try:
            line = input("TW Provision> ")
        except KeyboardInterrupt:
            print()
            break
        except EOFError:
            print()
            break
        line = line.strip()
        if not line:
            continue
        low = line.lower()
        if low in ("quit", "exit", "q"):
            break
        if low == "help":
            print(
                textwrap.dedent(
                    """\
                    Commands:
                      info                    GET /tw/provision/info
                      factory [options]       Factory-phase kvtext + commit
                      hub [options]           Hub-phase kvtext + commit
                      reset                   provision-cmd=reset
                      quit                    Exit interactive mode

                    Re-run with --help for global and transport options."""
                )
            )
            continue

        try:
            parts = shlex.split(line)
        except ValueError as e:
            err("parse error: {}".format(e))
            continue

        if not parts:
            continue

        sub = parts[0]
        if sub not in ("info", "factory", "hub", "reset"):
            err("unknown command: {!r} (try 'help')".format(sub))
            continue

        parser = argparse.ArgumentParser(prog=sub, add_help=False)
        if sub == "factory":
            add_factory_args(parser)
        elif sub == "hub":
            add_hub_args(parser)
        try:
            cmd_ns = parser.parse_args(parts[1:])
        except SystemExit:
            continue

        base = build_ns()
        for k, v in vars(cmd_ns).items():
            setattr(base, k, v)

        try:
            transport = make_transport()
            run_command(transport, sub, base, json_mode, verbose)
        except TimeoutError:
            err("CoAP timeout: device did not respond")
        except RuntimeError as e:
            err(str(e))
        except OSError as e:
            err("I/O error: {}".format(e))
        except Exception as e:
            err("{}: {}".format(type(e).__name__, e))


def build_parser() -> argparse.ArgumentParser:
    epilog = textwrap.dedent(
        """\
        examples:
          # Interactive LAN session (defaults: host 127.0.0.1, port 5683)
          %(prog)s lan

          # Non-interactive: device info over UDP
          %(prog)s lan info --host 127.0.0.1 --port 5683

          # Factory provisioning (identity fields)
          %(prog)s lan factory --vendor-id 42 --product-id 100

          # Hub provisioning (Wi-Fi + hub URL)
          %(prog)s lan hub --ssid MyNetwork --password secret --hub-url coap://192.168.1.10:5684

          # SoftAP (join the device AP first, default host 192.168.4.1)
          %(prog)s softap hub --ssid MyNetwork --password secret --hub-url coap://192.168.1.10:5684

          # BLE: scan for TW-* devices, then provision
          %(prog)s ble --scan
          %(prog)s ble --device AA:BB:CC:DD:EE:FF hub --ssid MyNetwork --password secret --hub-url coap://192.168.1.10:5684

          # Machine-readable JSON output
          %(prog)s --json lan info

        Environment:
          NO_COLOR   If set, disable ANSI colors.
        """
    )

    parser = argparse.ArgumentParser(
        description="TW Firmwareless provisioning CLI (LAN / SoftAP / BLE).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON instead of colored text",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Dump raw kvtext responses to stdout",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        metavar="SEC",
        help="Socket/BLE I/O timeout in seconds (default: 5)",
    )

    sub = parser.add_subparsers(dest="transport", required=True)

    lan = sub.add_parser("lan", help="Binary CoAP (via tw CLI) to a device on the LAN")
    lan.add_argument("--host", default="127.0.0.1", help="Target host (default: 127.0.0.1)")
    lan.add_argument("--port", type=int, default=5683, help="UDP port (default: 5683)")
    lan_cmd = lan.add_subparsers(dest="command", required=False)
    p_info = lan_cmd.add_parser("info", help="GET /tw/provision/info")
    p_factory = lan_cmd.add_parser("factory", help="Factory-phase provisioning")
    add_factory_args(p_factory)
    p_hub = lan_cmd.add_parser("hub", help="Hub-phase provisioning")
    add_hub_args(p_hub)
    lan_cmd.add_parser("reset", help="Send provision-cmd=reset")

    soft = sub.add_parser(
        "softap",
        help="Binary CoAP via device SoftAP (connect to device Wi-Fi first)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Note: connect your workstation to the device's SoftAP (e.g. TW-Prov-XXXX) "
            "before using this mode."
        ),
    )
    soft.add_argument(
        "--host",
        default="192.168.4.1",
        help="AP gateway IP (default: 192.168.4.1)",
    )
    soft.add_argument("--port", type=int, default=5683, help="UDP port (default: 5683)")
    soft_cmd = soft.add_subparsers(dest="command", required=False)
    soft_cmd.add_parser("info", help="GET /tw/provision/info")
    sp_factory = soft_cmd.add_parser("factory", help="Factory-phase provisioning")
    add_factory_args(sp_factory)
    sp_hub = soft_cmd.add_parser("hub", help="Hub-phase provisioning")
    add_hub_args(sp_hub)
    soft_cmd.add_parser("reset", help="Send provision-cmd=reset")

    ble = sub.add_parser("ble", help="BLE GATT provisioning (requires bleak)")
    ble.add_argument(
        "--device",
        metavar="ADDR",
        help="BLE address (from --scan); required for commands except --scan",
    )
    ble.add_argument(
        "--scan",
        action="store_true",
        help="Scan and list TW-* devices, then exit",
    )
    ble_cmd = ble.add_subparsers(dest="command", required=False)
    ble_cmd.add_parser("info", help="Read BLE status characteristic (see docs)")
    bp_factory = ble_cmd.add_parser("factory", help="Factory-phase provisioning")
    add_factory_args(bp_factory)
    bp_hub = ble_cmd.add_parser("hub", help="Hub-phase provisioning")
    add_hub_args(bp_hub)
    ble_cmd.add_parser("reset", help="Send provision-cmd=reset")

    return parser


def main(argv: Optional[List[str]] = None) -> None:
    argv = argv if argv is not None else sys.argv[1:]
    parser = build_parser()
    args = parser.parse_args(argv)

    json_mode = args.json
    verbose = args.verbose
    timeout = args.timeout

    if args.transport == "ble" and args.scan:
        run_ble_scan(timeout, json_mode)
        return

    if args.transport == "ble" and not args.device:
        emit_error(
            json_mode,
            "BLE requires --device ADDR (or use --scan to list devices).",
            2,
        )

    if args.transport in ("lan", "softap"):
        host = args.host
        port = args.port

        def make_coap() -> Transport:
            return TwCoapTransport(host, port, timeout)

        if args.command is None:

            def build_ns() -> argparse.Namespace:
                return argparse.Namespace(
                    vendor_id=None,
                    product_id=None,
                    vendor_name=None,
                    product_name=None,
                    variant=None,
                    serial_number=None,
                    uuid=None,
                    key_type=None,
                    key=None,
                    ssid=None,
                    password=None,
                    hub_url=None,
                )

            interactive_loop(make_coap, json_mode, verbose, lambda: build_ns())
            return

        try:
            run_command(make_coap(), args.command, args, json_mode, verbose)
        except TimeoutError:
            emit_error(json_mode, "CoAP timeout: device did not respond", 1)
        except RuntimeError as e:
            emit_error(json_mode, str(e), 1)
        except OSError as e:
            emit_error(json_mode, "I/O error: {}".format(e), 1)
        except Exception as e:
            emit_error(json_mode, "{}: {}".format(type(e).__name__, e), 1)
        return

    if args.transport == "ble":
        addr = args.device

        def make_ble() -> Transport:
            return BleTransport(addr, timeout)

        if args.command is None:

            def build_ns_ble() -> argparse.Namespace:
                return argparse.Namespace(
                    vendor_id=None,
                    product_id=None,
                    vendor_name=None,
                    product_name=None,
                    variant=None,
                    serial_number=None,
                    uuid=None,
                    key_type=None,
                    key=None,
                    ssid=None,
                    password=None,
                    hub_url=None,
                )

            interactive_loop(make_ble, json_mode, verbose, lambda: build_ns_ble())
            return

        try:
            run_command(make_ble(), args.command, args, json_mode, verbose)
        except OSError as e:
            emit_error(json_mode, "I/O error: {}".format(e), 1)
        except asyncio.TimeoutError:
            emit_error(json_mode, "BLE operation timed out", 1)
        except Exception as e:
            emit_error(json_mode, "{}: {}".format(type(e).__name__, e), 1)
        return


if __name__ == "__main__":
    main()
