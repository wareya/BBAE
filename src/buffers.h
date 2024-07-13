#ifndef INCL_BUFFERS
#define INCL_BUFFERS

#include <stdint.h>
#include <stddef.h> // size_t
#include <string.h> // memcpy
#include <assert.h> // assert
#include <stdlib.h> // realloc

#ifndef W_WLIB_FUNCPREFIX
#define W_WLIB_FUNCPREFIX static inline
#endif

#ifndef BUF_REALLOC
#define BUF_REALLOC (realloc)
#endif

typedef struct {
    uint8_t * data;
    size_t len;
    size_t cap;
    size_t cur;
} byte_buffer;

W_WLIB_FUNCPREFIX uint64_t byteswap_int(uint64_t value, uint8_t bytecount);

W_WLIB_FUNCPREFIX void bytes_reserve(byte_buffer * buf, size_t extra);
W_WLIB_FUNCPREFIX void byte_push(byte_buffer * buf, uint8_t byte);
W_WLIB_FUNCPREFIX void bytes_push(byte_buffer * buf, const uint8_t * bytes, size_t count);
W_WLIB_FUNCPREFIX void bytes_push_int(byte_buffer * buf, uint64_t value, size_t count);
W_WLIB_FUNCPREFIX uint8_t byte_pop(byte_buffer * buf);
W_WLIB_FUNCPREFIX uint64_t bytes_pop_int(byte_buffer * buf, size_t count);

#ifndef W_NOIMPL

W_WLIB_FUNCPREFIX uint64_t byteswap_int(uint64_t value, uint8_t bytecount)
{
    uint64_t ret = 0;
    for (uint8_t i = 0; i < bytecount; i++)
        ((uint8_t *)&ret)[i] = ((uint8_t *)&value)[bytecount - 1 - i];
    return ret;
}

W_WLIB_FUNCPREFIX void bytes_reserve(byte_buffer * buf, size_t extra)
{
    if (buf->cap < 8)
        buf->cap = 8;
    while (buf->len + extra >= buf->cap)
        buf->cap <<= 1;
    uint8_t * next = (uint8_t *)BUF_REALLOC(buf->data, buf->cap);
    assert(next);
    buf->data = next;
    if (!buf->data)
        buf->cap = 0;
}
W_WLIB_FUNCPREFIX void byte_push(byte_buffer * buf, uint8_t byte)
{
    if (buf->len >= buf->cap || buf->data == 0)
    {
        buf->cap = buf->cap << 1;
        if (buf->cap < 8)
            buf->cap = 8;
        uint8_t * next = (uint8_t *)BUF_REALLOC(buf->data, buf->cap);
        assert(next);
        buf->data = next;
    }
    buf->data[buf->len] = byte;
    buf->len += 1;
}
W_WLIB_FUNCPREFIX void bytes_push(byte_buffer * buf, const uint8_t * bytes, size_t count)
{
    bytes_reserve(buf, count);
    assert(buf->len + count <= buf->cap);
    memcpy(&buf->data[buf->len], bytes, count);
    buf->len += count;
}
W_WLIB_FUNCPREFIX void bytes_push_int(byte_buffer * buf, uint64_t value, size_t count)
{
    bytes_reserve(buf, count);
    for (size_t i = 0; i < count; i++)
    {
        byte_push(buf, value);
        value >>= 8;
    }
}
W_WLIB_FUNCPREFIX uint8_t byte_pop(byte_buffer * buf)
{
    return buf->data[buf->cur++];
}
W_WLIB_FUNCPREFIX uint64_t bytes_pop_int(byte_buffer * buf, size_t count)
{
    uint64_t ret = 0;
    for (size_t i = 0; i < count; i++)
        ret |= ((uint64_t)buf->data[buf->cur + i]) << (8 * i);
    buf->cur += count;
    return ret;
}

#endif // ndef W_NOIMPL

#endif // INCL_BUFFERS
