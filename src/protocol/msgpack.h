#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *buf;
    size_t size;
    size_t pos;
} MsgPackWriter;

typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t pos;
} MsgPackReader;

void msgpack_writer_init(MsgPackWriter *w, uint8_t *buf, size_t size);
void msgpack_write_nil(MsgPackWriter *w);
void msgpack_write_bool(MsgPackWriter *w, bool val);
void msgpack_write_int(MsgPackWriter *w, int64_t val);
void msgpack_write_float(MsgPackWriter *w, double val);
void msgpack_write_str(MsgPackWriter *w, const char *str, size_t len);
void msgpack_write_array_header(MsgPackWriter *w, size_t count);
void msgpack_write_map_header(MsgPackWriter *w, size_t count);

void msgpack_reader_init(MsgPackReader *r, const uint8_t *buf, size_t size);
int msgpack_read_type(MsgPackReader *r);
bool msgpack_read_bool(MsgPackReader *r);
int64_t msgpack_read_int(MsgPackReader *r);
double msgpack_read_float(MsgPackReader *r);
char *msgpack_read_str(MsgPackReader *r, size_t *len);
size_t msgpack_read_array(MsgPackReader *r);
size_t msgpack_read_map(MsgPackReader *r);