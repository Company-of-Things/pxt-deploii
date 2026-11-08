#include "deploii_hash.h"

/*
 * TODO: implement the hash.
 *
 * It must give the same result as the hash Deploii uses for the board ID.
 * For reference, the Arduino library uses DEPLOII_fnv1a16() in src/deploii_utils.h.
 *
 * Until this is implemented every board advertises service UUID 0x0000.
 */
uint16_t deploii_hash(const char *str, size_t len)
{
    (void) len;
    const uint16_t FNV_OFFSET_BASIS = 0x811C;
    const uint16_t FNV_PRIME = 0x0101;

    uint16_t hash = FNV_OFFSET_BASIS;

    while (*str)
    {
        hash ^= (uint8_t)(*str++);
        hash *= FNV_PRIME;
    }

    return hash;
}
