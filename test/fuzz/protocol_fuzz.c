#include "protocol/protocol.h"
#include "protocol/schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *input = malloc(size + 1);
    memcpy(input, data, size);
    input[size] = '\0';

    WidgetRequest *req = widget_request_parse(input);
    if (req) widget_request_free(req);

    const char *schema = "{\"type\":\"object\",\"properties\":{\"widget\":{\"type\":\"string\"}}}";
    char *err = NULL;
    schema_validate(input, schema, &err);
    free(err);

    free(input);
    return 0;
}