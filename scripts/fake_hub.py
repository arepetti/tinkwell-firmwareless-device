#!/usr/bin/env python3
"""
fake_hub.py -- Simulates a Tinkwell edge hub for local development.

Wraps ``tw coap server --mailbox`` to provide binary CoAP (RFC 7252)
heartbeat handling with protobuf-encoded hub-push command dispatch.

The hub responds to heartbeats with HeartbeatReply {pending: N} then
sends each queued command as an individual CoAP POST to the device.

Usage:
    python fake_hub.py                        # default port 5684
    python fake_hub.py --port 5684 --verbose
    python fake_hub.py --queue reboot
    python fake_hub.py --queue 'set-config:{"entries":[{"key":"mode","value":"cool"}]}'
"""

import argparse
import shutil
import subprocess
import sys

DEFAULT_PORT = 5684
DEFAULT_PREFIX = "tw"


def main():
    parser = argparse.ArgumentParser(
        description="Tinkwell fake hub (wraps tw coap server)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"UDP port (default: {DEFAULT_PORT})")
    parser.add_argument("--prefix", default=DEFAULT_PREFIX,
                        help=f"CoAP path prefix for commands (default: {DEFAULT_PREFIX})")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show request payloads")
    parser.add_argument("--queue", action="append", default=[],
                        help="Pre-queue command[:json] (repeatable)")
    args = parser.parse_args()

    tw = shutil.which("tw")
    if not tw:
        print("Error: 'tw' CLI not found on PATH. Install Tinkwell CLI first.",
              file=sys.stderr)
        sys.exit(1)

    cmd = [tw, "coap", "server",
           "--port", str(args.port),
           "--mailbox", "/hub/heartbeat",
           "--prefix", args.prefix]

    for q in args.queue:
        cmd += ["--queue", q]

    if args.verbose:
        cmd.append("--log-payload")

    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
