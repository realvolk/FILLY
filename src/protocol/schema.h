#pragma once
#include "cJSON.h"
#include <stdbool.h>

bool schema_validate(const char *json_str, const char *schema_str, char **error_out);