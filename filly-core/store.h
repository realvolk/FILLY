#pragma once
#include "cJSON.h"
#include <stdbool.h>

typedef struct Store_s Store;

Store *store_new(void);
void store_free(Store *s);
const char *store_get(Store *s, const char *key);
void store_set(Store *s, const char *key, const char *value);
bool store_get_bool(Store *s, const char *key);