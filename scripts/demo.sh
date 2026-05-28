#!/usr/bin/env bash
# demo.sh -- Run the complete Tinkwell Firmwareless device demo.
#
# Builds the thermostat example (POSIX), starts the hub, runs the
# device, and shows heartbeats arriving.  Ctrl-C to stop.
#
# Usage: ./scripts/demo.sh

set -u

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly DEVICE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly THERMOSTAT_BUILD="${DEVICE_DIR}/examples/thermostat/build"
THERMOSTAT_BIN="${THERMOSTAT_BUILD}/thermostat"
if [[ ! -x "${THERMOSTAT_BIN}" && -x "${THERMOSTAT_BUILD}/thermostat.exe" ]]; then
  THERMOSTAT_BIN="${THERMOSTAT_BUILD}/thermostat.exe"
fi
readonly THERMOSTAT_BIN

# Colors (omit if not a TTY)
if [[ -t 1 ]]; then
  readonly C_RESET=$'\033[0m'
  readonly C_RED=$'\033[0;31m'
  readonly C_GREEN=$'\033[0;32m'
  readonly C_YELLOW=$'\033[1;33m'
  readonly C_CYAN=$'\033[0;36m'
else
  readonly C_RESET='' C_RED='' C_GREEN='' C_YELLOW='' C_CYAN=''
fi

die() {
  echo "${C_RED}error:${C_RESET} $*" >&2
  exit 1
}

info() { echo "${C_CYAN}==>${C_RESET} $*"; }
ok()   { echo "${C_GREEN}ok:${C_RESET} $*"; }
warn() { echo "${C_YELLOW}warn:${C_RESET} $*"; }

HUB_PID=""
THERMO_PID=""

cleanup() {
  local sig="${1:-}"
  if [[ -n "$sig" ]]; then
    info "received ${sig}, stopping demo..."
  fi
  if [[ -n "${THERMO_PID}" ]] && kill -0 "${THERMO_PID}" 2>/dev/null; then
    kill "${THERMO_PID}" 2>/dev/null || true
    wait "${THERMO_PID}" 2>/dev/null || true
  fi
  if [[ -n "${HUB_PID}" ]] && kill -0 "${HUB_PID}" 2>/dev/null; then
    kill "${HUB_PID}" 2>/dev/null || true
    wait "${HUB_PID}" 2>/dev/null || true
  fi
}

trap 'cleanup INT; exit 130' INT
trap 'cleanup TERM; exit 143' TERM

if ! command -v cmake >/dev/null 2>&1; then
  die "cmake not found in PATH"
fi
if ! command -v tw >/dev/null 2>&1; then
  die "'tw' CLI not found in PATH (needed for firmwareless-hub)"
fi

info "building thermostat (PAL_BACKEND=posix)..."
if ! cmake -B "${THERMOSTAT_BUILD}" -S "${DEVICE_DIR}/examples/thermostat" -DPAL_BACKEND=posix; then
  die "cmake configure failed"
fi
if ! cmake --build "${THERMOSTAT_BUILD}"; then
  die "cmake --build failed"
fi

if [[ ! -x "${THERMOSTAT_BIN}" ]]; then
  die "thermostat binary not found or not executable: ${THERMOSTAT_BIN}"
fi
ok "built ${THERMOSTAT_BIN}"

info "starting firmwareless-hub..."
tw firmwareless-hub start &
HUB_PID=$!
sleep 2
if ! kill -0 "${HUB_PID}" 2>/dev/null; then
  die "hub process exited immediately (pid ${HUB_PID}); check 'tw firmwareless-hub start'"
fi

info "starting thermostat device..."
"${THERMOSTAT_BIN}" &
THERMO_PID=$!

info "waiting 3s for first heartbeat..."
sleep 3
if ! kill -0 "${THERMO_PID}" 2>/dev/null; then
  cleanup ""
  die "thermostat exited during startup (pid ${THERMO_PID})"
fi

info "sending test query (GET /tw/info)..."
tw coap send get /tw/info || warn "CoAP probe returned non-zero (device may still be fine)"

echo ""
echo "${C_GREEN}Demo running. Press Ctrl-C to stop.${C_RESET}"
echo ""

set +e
wait "${HUB_PID}" "${THERMO_PID}"
wait_status=$?
set -e
cleanup ""
exit "${wait_status}"
