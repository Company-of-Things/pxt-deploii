#include "deploii_decode.h"
#include "deploii_config.h"
#include "cmp.h"

using namespace pxt;

// ---------------------------------------------------------------------------
// CMP reader over a memory buffer
// ---------------------------------------------------------------------------

struct MemReader
{
    const uint8_t *data;
    size_t size;
    size_t pos;
};

static bool memRead(cmp_ctx_t *ctx, void *out, size_t count)
{
    MemReader *r = (MemReader *)ctx->buf;
    if (count > r->size - r->pos)
        return false;
    memcpy(out, r->data + r->pos, count);
    r->pos += count;
    return true;
}

static bool memSkip(cmp_ctx_t *ctx, size_t count)
{
    MemReader *r = (MemReader *)ctx->buf;
    if (count > r->size - r->pos)
        return false;
    r->pos += count;
    return true;
}

// Point at the next `count` bytes in the buffer without copying them
static const char *memTake(cmp_ctx_t *ctx, size_t count)
{
    MemReader *r = (MemReader *)ctx->buf;
    if (count > r->size - r->pos)
        return NULL;
    const char *p = (const char *)(r->data + r->pos);
    r->pos += count;
    return p;
}

// ---------------------------------------------------------------------------
// MessagePack -> TypeScript values
// ---------------------------------------------------------------------------

static TValue decodeValue(cmp_ctx_t *ctx, int depth, bool *ok);

// Decode `count` values into a new TypeScript array
static TValue decodeList(cmp_ctx_t *ctx, uint32_t count, int depth, bool *ok)
{
    if (depth >= DEPLOII_DECODE_MAX_DEPTH)
    {
        // Too deep: skip the contents
        for (uint32_t i = 0; i < count && *ok; i++)
            decodeValue(ctx, depth + 1, ok);
        return TAG_UNDEFINED;
    }

    RefCollection *list = Array_::mk();
    registerGCObj(list); // keep alive while the elements are allocated
    for (uint32_t i = 0; i < count && *ok; i++)
    {
        TValue item = decodeValue(ctx, depth + 1, ok);
        if (*ok)
            Array_::push(list, item);
    }
    unregisterGCObj(list);
    return (TValue)list;
}

static TValue decodeValue(cmp_ctx_t *ctx, int depth, bool *ok)
{
    cmp_object_t obj;
    if (!cmp_read_object(ctx, &obj))
    {
        *ok = false;
        return TAG_UNDEFINED;
    }

    switch (obj.type)
    {
    case CMP_TYPE_NIL:
        return TAG_NULL;

    case CMP_TYPE_BOOLEAN:
        return fromBool(obj.as.boolean);

    case CMP_TYPE_POSITIVE_FIXNUM:
    case CMP_TYPE_UINT8:
        return fromInt(obj.as.u8);
    case CMP_TYPE_UINT16:
        return fromInt(obj.as.u16);
    case CMP_TYPE_UINT32:
        return obj.as.u32 <= 0x7fffffff ? fromInt((int)obj.as.u32) : fromDouble((NUMBER)obj.as.u32);
    case CMP_TYPE_UINT64:
        return fromDouble((NUMBER)obj.as.u64);

    case CMP_TYPE_NEGATIVE_FIXNUM:
    case CMP_TYPE_SINT8:
        return fromInt(obj.as.s8);
    case CMP_TYPE_SINT16:
        return fromInt(obj.as.s16);
    case CMP_TYPE_SINT32:
        return fromInt(obj.as.s32);
    case CMP_TYPE_SINT64:
        return fromDouble((NUMBER)obj.as.s64);

    case CMP_TYPE_FLOAT:
        return fromDouble((NUMBER)obj.as.flt);
    case CMP_TYPE_DOUBLE:
        return fromDouble((NUMBER)obj.as.dbl);

    case CMP_TYPE_FIXSTR:
    case CMP_TYPE_STR8:
    case CMP_TYPE_STR16:
    case CMP_TYPE_STR32:
    {
        const char *p = memTake(ctx, obj.as.str_size);
        if (p == NULL)
        {
            *ok = false;
            return TAG_UNDEFINED;
        }
        return (TValue)mkString(p, (int)obj.as.str_size);
    }

    case CMP_TYPE_BIN8:
    case CMP_TYPE_BIN16:
    case CMP_TYPE_BIN32:
    {
        const char *p = memTake(ctx, obj.as.bin_size);
        if (p == NULL)
        {
            *ok = false;
            return TAG_UNDEFINED;
        }
        return (TValue)mkBuffer(p, (int)obj.as.bin_size);
    }

    case CMP_TYPE_FIXARRAY:
    case CMP_TYPE_ARRAY16:
    case CMP_TYPE_ARRAY32:
        return decodeList(ctx, obj.as.array_size, depth, ok);

    case CMP_TYPE_FIXMAP:
    case CMP_TYPE_MAP16:
    case CMP_TYPE_MAP32:
        // Static TypeScript has no dynamic objects: a map becomes [key, value, key, value, ...]
        return decodeList(ctx, obj.as.map_size * 2, depth, ok);

    default:
        // ext types (cmp_read_object has read the header, skip the payload)
        if (!memSkip(ctx, obj.as.ext.size))
            *ok = false;
        return TAG_UNDEFINED;
    }
}

// ---------------------------------------------------------------------------
// Top-level message
// ---------------------------------------------------------------------------

static int fieldIndex(const char *key, uint32_t len)
{
    static const char *const names[DEPLOII_MSG_FIELD_COUNT] = {
        "moduleId", // DEPLOII_MSG_MODULE_ID
        "data",     // DEPLOII_MSG_DATA
        "cardId",   // DEPLOII_MSG_CARD_ID
        "type",     // DEPLOII_MSG_TYPE
        "protocol", // DEPLOII_MSG_PROTOCOL
    };
    for (int i = 0; i < DEPLOII_MSG_FIELD_COUNT; i++)
        if (strlen(names[i]) == len && memcmp(names[i], key, len) == 0)
            return i;
    return -1;
}

RefCollection *deploii_decode_message(const uint8_t *data, size_t size)
{
    MemReader reader = {data, size, 0};
    cmp_ctx_t ctx;
    cmp_init(&ctx, &reader, memRead, memSkip, NULL);

    uint32_t pairs;
    if (!cmp_read_map(&ctx, &pairs))
        return NULL;

    RefCollection *msg = Array_::mk();
    registerGCObj(msg);
    for (int i = 0; i < DEPLOII_MSG_FIELD_COUNT; i++)
        Array_::push(msg, TAG_UNDEFINED);

    bool ok = true;
    for (uint32_t i = 0; i < pairs && ok; i++)
    {
        uint32_t keyLen;
        if (!cmp_read_str_size(&ctx, &keyLen))
        {
            ok = false;
            break;
        }
        const char *key = memTake(&ctx, keyLen);
        if (key == NULL)
        {
            ok = false;
            break;
        }

        int field = fieldIndex(key, keyLen);
        if (field < 0 && i < DEPLOII_MSG_FIELD_COUNT)
            field = (int)i; // unknown key name: use the Arduino library's field order

        TValue value = decodeValue(&ctx, 0, &ok);
        if (ok && field >= 0)
            Array_::setAt(msg, field, value);
    }

    unregisterGCObj(msg);
    return ok ? msg : NULL;
}
