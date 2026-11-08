#ifndef DEPLOII_HASH_H
#define DEPLOII_HASH_H

#include <stdint.h>
#include <stddef.h>

/*
 * Hash a Deploii board ID into the 16-bit UUID of the BLE service.
 *
 * str: UTF-8 bytes of the board ID (null-terminated)
 * len: number of bytes, terminator not included
 */
uint16_t deploii_hash(const char *str, size_t len);

#endif // DEPLOII_HASH_H
