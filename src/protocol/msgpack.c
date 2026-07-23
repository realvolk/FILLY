#include "msgpack.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void msgpack_writer_init(MsgPackWriter *w, uint8_t *buf, size_t size) {
    w->buf = buf; w->size = size; w->pos = 0;
}
static void write_byte(MsgPackWriter *w, uint8_t b) {
    if (w->pos < w->size) w->buf[w->pos++] = b;
}
static void write_bytes(MsgPackWriter *w, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len && w->pos < w->size; i++) w->buf[w->pos++] = data[i];
}

void msgpack_write_nil(MsgPackWriter *w) { write_byte(w, 0xc0); }
void msgpack_write_bool(MsgPackWriter *w, bool val) { write_byte(w, val ? 0xc3 : 0xc2); }
void msgpack_write_int(MsgPackWriter *w, int64_t val) {
    if (val >= 0) {
        if (val <= 127) write_byte(w, (uint8_t)val);
        else if (val <= 255) { write_byte(w, 0xcc); write_byte(w, (uint8_t)val); }
        else if (val <= 65535) { write_byte(w, 0xcd); write_byte(w, (uint8_t)(val>>8)); write_byte(w, (uint8_t)val); }
        else if (val <= 0xFFFFFFFF) { write_byte(w, 0xce); for(int i=3;i>=0;i--) write_byte(w, (uint8_t)(val>>(8*i))); }
        else { write_byte(w, 0xcf); for(int i=7;i>=0;i--) write_byte(w, (uint8_t)(val>>(8*i))); }
    } else {
        if (val >= -32) write_byte(w, (uint8_t)(0xe0 | (val & 0x1f)));
        else if (val >= -128) { write_byte(w, 0xd0); write_byte(w, (uint8_t)val); }
        else if (val >= -32768) { write_byte(w, 0xd1); write_byte(w, (uint8_t)(val>>8)); write_byte(w, (uint8_t)val); }
        else if (val >= -0x80000000LL) { write_byte(w, 0xd2); for(int i=3;i>=0;i--) write_byte(w, (uint8_t)(val>>(8*i))); }
        else { write_byte(w, 0xd3); for(int i=7;i>=0;i--) write_byte(w, (uint8_t)(val>>(8*i))); }
    }
}
void msgpack_write_float(MsgPackWriter *w, double val) { write_byte(w, 0xcb); uint64_t bits; memcpy(&bits, &val, 8); for(int i=7;i>=0;i--) write_byte(w, (uint8_t)(bits>>(8*i))); }
void msgpack_write_str(MsgPackWriter *w, const char *str, size_t len) {
    if (len <= 31) write_byte(w, (uint8_t)(0xa0 | len));
    else if (len <= 255) { write_byte(w, 0xd9); write_byte(w, (uint8_t)len); }
    else if (len <= 65535) { write_byte(w, 0xda); write_byte(w, (uint8_t)(len>>8)); write_byte(w, (uint8_t)len); }
    else { write_byte(w, 0xdb); for(int i=3;i>=0;i--) write_byte(w, (uint8_t)(len>>(8*i))); }
    write_bytes(w, (const uint8_t*)str, len);
}
void msgpack_write_array_header(MsgPackWriter *w, size_t count) {
    if (count <= 15) write_byte(w, (uint8_t)(0x90 | count));
    else if (count <= 65535) { write_byte(w, 0xdc); write_byte(w, (uint8_t)(count>>8)); write_byte(w, (uint8_t)count); }
    else { write_byte(w, 0xdd); for(int i=3;i>=0;i--) write_byte(w, (uint8_t)(count>>(8*i))); }
}
void msgpack_write_map_header(MsgPackWriter *w, size_t count) {
    if (count <= 15) write_byte(w, (uint8_t)(0x80 | count));
    else if (count <= 65535) { write_byte(w, 0xde); write_byte(w, (uint8_t)(count>>8)); write_byte(w, (uint8_t)count); }
    else { write_byte(w, 0xdf); for(int i=3;i>=0;i--) write_byte(w, (uint8_t)(count>>(8*i))); }
}

void msgpack_reader_init(MsgPackReader *r, const uint8_t *buf, size_t size) { r->buf=buf; r->size=size; r->pos=0; }
static uint8_t read_byte(MsgPackReader *r) { if(r->pos<r->size) return r->buf[r->pos++]; return 0; }
int msgpack_read_type(MsgPackReader *r) { if(r->pos>=r->size) return -1; return r->buf[r->pos]; }
bool msgpack_read_bool(MsgPackReader *r) { uint8_t b = read_byte(r); return b==0xc3; }
int64_t msgpack_read_int(MsgPackReader *r) {
    uint8_t b = read_byte(r);
    if(b<=0x7f) return b;
    if((b&0xe0)==0xe0) return (int64_t)((int8_t)b);
    switch(b){
        case 0xcc: return read_byte(r);
        case 0xcd: return (read_byte(r)<<8)|read_byte(r);
        case 0xce: { uint32_t v=0; for(int i=0;i<4;i++) v=(v<<8)|read_byte(r); return v; }
        case 0xcf: { uint64_t v=0; for(int i=0;i<8;i++) v=(v<<8)|read_byte(r); return (int64_t)v; }
        case 0xd0: return (int64_t)((int8_t)read_byte(r));
        case 0xd1: { int16_t v=(read_byte(r)<<8)|read_byte(r); return v; }
        case 0xd2: { int32_t v=0; for(int i=0;i<4;i++) v=(v<<8)|read_byte(r); return v; }
        case 0xd3: { int64_t v=0; for(int i=0;i<8;i++) v=(v<<8)|read_byte(r); return v; }
    }
    return 0;
}
double msgpack_read_float(MsgPackReader *r) { uint8_t b = read_byte(r); if(b!=0xcb) return 0; uint64_t bits=0; for(int i=0;i<8;i++) bits=(bits<<8)|read_byte(r); double val; memcpy(&val,&bits,8); return val; }
char *msgpack_read_str(MsgPackReader *r, size_t *len) {
    uint8_t b = read_byte(r); size_t l=0;
    if((b&0xe0)==0xa0) l=b&0x1f;
    else if(b==0xd9) l=read_byte(r);
    else if(b==0xda) l=(read_byte(r)<<8)|read_byte(r);
    else if(b==0xdb) { for(int i=0;i<4;i++) l=(l<<8)|read_byte(r); }
    char *s=malloc(l+1); for(size_t i=0;i<l;i++) s[i]=read_byte(r); s[l]=0;
    if (len) *len = l;
    return s;
}
size_t msgpack_read_array(MsgPackReader *r) { uint8_t b=read_byte(r); if((b&0xf0)==0x90) return b&0xf; else if(b==0xdc) return (read_byte(r)<<8)|read_byte(r); else if(b==0xdd) { size_t c=0; for(int i=0;i<4;i++) c=(c<<8)|read_byte(r); return c; } return 0; }
size_t msgpack_read_map(MsgPackReader *r) { uint8_t b=read_byte(r); if((b&0xf0)==0x80) return b&0xf; else if(b==0xde) return (read_byte(r)<<8)|read_byte(r); else if(b==0xdf) { size_t c=0; for(int i=0;i<4;i++) c=(c<<8)|read_byte(r); return c; } return 0; }