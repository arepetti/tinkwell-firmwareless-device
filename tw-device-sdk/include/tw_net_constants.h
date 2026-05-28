/*
 * tw_net_constants.h -- Network-related constants for the TW Device SDK.
 *
 * Buffer sizes and limits derived from IEEE 802.11 and common WiFi/BLE
 * specifications.  All public constants use the TW_ prefix.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_NET_CONSTANTS_H
#define TW_NET_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * WiFi -- IEEE 802.11-2020
 * -----------------------------------------------------------------------*/

/**
 * @brief Maximum WiFi SSID buffer size including NUL terminator.
 *
 * IEEE 802.11-2020 section 9.4.2.2 defines the SSID element as 0-32
 * octets.  Add 1 for C string NUL terminator.
 */
#define TW_WIFI_SSID_BUF_SIZE  33

/**
 * @brief Maximum WiFi password buffer size including NUL terminator.
 *
 * WPA2-Personal passphrases are 8-63 printable ASCII characters
 * (IEEE 802.11-2020 section 12.7.1.3).  Some implementations also
 * accept a 64-character hex PSK.  Add 1 for NUL terminator.
 */
#define TW_WIFI_PASS_BUF_SIZE  65

/* -------------------------------------------------------------------------
 * MAC address -- IEEE 802
 * -----------------------------------------------------------------------*/

/** @brief IEEE 802 MAC address length in bytes (48-bit / EUI-48). */
#define TW_MAC_ADDR_LEN        6

/* -------------------------------------------------------------------------
 * Hostname / address buffers
 * -----------------------------------------------------------------------*/

/**
 * @brief Maximum hostname buffer size including NUL terminator.
 *
 * RFC 1035 section 2.3.4 limits DNS names to 253 characters, but
 * embedded use cases typically work with short names or IP literals.
 * 64 bytes is sufficient for IPv4/IPv6 literals and short hostnames.
 */
#define TW_HOST_BUF_SIZE       64

#ifdef __cplusplus
}
#endif

#endif /* TW_NET_CONSTANTS_H */
