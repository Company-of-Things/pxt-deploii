#ifndef DEPLOII_DECODE_H
#define DEPLOII_DECODE_H

#include "pxt.h"

/*
 * Decode a MessagePack message from Deploii into a TypeScript array:
 *
 *   [moduleId, data, cardId, type, protocol]   (indexes DEPLOII_MSG_* in deploii_config.h)
 *
 * The message must be a map. Values are matched by key name ("moduleId", "data", "cardId",
 * "type", "protocol"); entries with other key names are matched by position instead, in the
 * same order as the Arduino library's msgStruct. Missing fields are undefined.
 *
 * Values become TypeScript values: nil -> null, bool -> boolean, integers/floats -> number,
 * str -> string, bin -> Buffer, array -> array, map -> array of [key, value, key, value, ...],
 * ext -> undefined.
 *
 * Returns NULL if the message is not valid MessagePack or not a map.
 */
RefCollection *deploii_decode_message(const uint8_t *data, size_t size);

#endif // DEPLOII_DECODE_H
