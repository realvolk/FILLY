package filly

/*
#cgo LDFLAGS: -lfilly
#include <stdlib.h>
#include "core/client.h"
*/
import "C"
import "unsafe"

type Client struct {
    c *C.FillyClient
}

func Connect(socket string) (*Client, error) {
    cs := C.CString(socket)
    defer C.free(unsafe.Pointer(cs))
    c := C.filly_client_connect(cs)
    if c == nil {
        return nil, fmt.Errorf("cannot connect to %s", socket)
    }
    return &Client{c: c}, nil
}

func (c *Client) Send(json string) {
    cj := C.CString(json)
    defer C.free(unsafe.Pointer(cj))
    C.filly_client_send_request(c.c, cj)
    var result *C.cJSON
    var cancelled C.bool
    C.filly_client_get_response(c.c, &result, &cancelled)
}

func (c *Client) SendKey(keycode int, ch byte) {
    C.filly_client_send_key(c.c, C.int(keycode), C.char(ch))
}

func (c *Client) Close() {
    C.filly_client_disconnect(c.c)
}