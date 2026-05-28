#!/usr/bin/env bash
#
# build_flash.sh -- Build and flash main + optional factory app.
#
# Usage:
#   ./scripts/build_flash.sh <example> [--factory] [--port <port>] [--target <target>]
#
# Examples:
#   ./scripts/build_flash.sh thermostat
#   ./scripts/build_flash.sh thermostat --factory --target esp32c3
#   ./scripts/build_flash.sh applet-device --factory --port /dev/ttyUSB0
#
# When --factory is specified:
#   1. Builds the factory app (extras/firmwareless/device/factory)
#   2. Builds the main example app
#   3. Merges both binaries using esptool.py merge_bin
#   4. Flashes the combined image
#
# When --factory is NOT specified:
#   1. Builds the main example app with the standard OTA partition table
#   2. Flashes it normally
#
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FACTORY_DIR="${DEVICE_DIR}/factory"
EXAMPLES_DIR="${DEVICE_DIR}/examples"

EXAMPLE=""
WITH_FACTORY=0
PORT="${ESPPORT:-/dev/ttyUSB0}"
TARGET="${IDF_TARGET:-esp32c3}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --factory)  WITH_FACTORY=1; shift ;;
        --port)     PORT="$2"; shift 2 ;;
        --target)   TARGET="$2"; shift 2 ;;
        *)
            if [[ -z "$EXAMPLE" ]]; then
                EXAMPLE="$1"
            fi
            shift ;;
    esac
done

if [[ -z "$EXAMPLE" ]]; then
    echo "Usage: $0 <example> [--factory] [--port <port>] [--target <target>]"
    echo "  examples: thermostat, applet-device"
    exit 1
fi

EXAMPLE_DIR="${EXAMPLES_DIR}/${EXAMPLE}/esp-idf"
if [[ ! -d "$EXAMPLE_DIR" ]]; then
    echo "Error: no ESP-IDF project for example '${EXAMPLE}' (missing directory):" >&2
    echo "  ${EXAMPLE_DIR}" >&2
    echo "This script only builds examples that contain an esp-idf/ tree." >&2
    exit 1
fi

export IDF_TARGET="${TARGET}"

echo "=== TW Device Build ==="
echo "  Example : ${EXAMPLE}"
echo "  Target  : ${TARGET}"
echo "  Port    : ${PORT}"
echo "  Factory : $([ $WITH_FACTORY -eq 1 ] && echo 'yes' || echo 'no')"
echo ""

if [[ $WITH_FACTORY -eq 1 ]]; then
    echo "--- Building factory app ---"
    cd "${FACTORY_DIR}"
    idf.py set-target "${TARGET}"
    idf.py build

    FACTORY_BIN="${FACTORY_DIR}/build/tw_factory.bin"
    FACTORY_ADDR="0x20000"

    echo ""
    echo "--- Building main app (${EXAMPLE}) ---"
    cd "${EXAMPLE_DIR}"

    # Override partition table to use the factory-aware layout.
    cp "${DEVICE_DIR}/partitions_factory.csv" partitions.csv
    echo "CONFIG_PARTITION_TABLE_CUSTOM=y" >> sdkconfig.defaults
    echo "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions.csv\"" >> sdkconfig.defaults

    idf.py set-target "${TARGET}"
    idf.py build

    MAIN_BIN="${EXAMPLE_DIR}/build/${EXAMPLE}.bin"
    MAIN_ADDR="0x60000"

    echo ""
    echo "--- Merging binaries ---"
    MERGED="${EXAMPLE_DIR}/build/merged.bin"

    esptool.py --chip "${TARGET}" merge_bin \
        --output "${MERGED}" \
        --flash_mode dio --flash_size 4MB \
        0x0000 "${EXAMPLE_DIR}/build/bootloader/bootloader.bin" \
        0x8000 "${EXAMPLE_DIR}/build/partition_table/partition-table.bin" \
        "${FACTORY_ADDR}" "${FACTORY_BIN}" \
        "${MAIN_ADDR}" "${MAIN_BIN}"

    echo ""
    echo "--- Flashing merged image ---"
    esptool.py --chip "${TARGET}" --port "${PORT}" write_flash 0x0 "${MERGED}"

    # Clean up override.
    rm -f partitions.csv

else
    echo "--- Building main app (${EXAMPLE}) ---"
    cd "${EXAMPLE_DIR}"
    idf.py set-target "${TARGET}"
    idf.py build

    echo ""
    echo "--- Flashing ---"
    idf.py --port "${PORT}" flash
fi

echo ""
echo "=== Done ==="
