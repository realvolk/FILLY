#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool validate_node(cJSON *node, cJSON *schema, char **error);
static bool validate_type(cJSON *node, cJSON *schema, char **error) {
    cJSON *t = cJSON_GetObjectItem(schema, "type"); if (!t || !t->valuestring) return true;
    const char *type = t->valuestring;
    if (strcmp(type, "string") == 0 && !cJSON_IsString(node)) { *error = strdup("Expected string"); return false; }
    if (strcmp(type, "number") == 0 && !cJSON_IsNumber(node)) { *error = strdup("Expected number"); return false; }
    if (strcmp(type, "boolean") == 0 && !(cJSON_IsBool(node) || cJSON_IsTrue(node) || cJSON_IsFalse(node))) { *error = strdup("Expected boolean"); return false; }
    if (strcmp(type, "object") == 0 && !cJSON_IsObject(node)) { *error = strdup("Expected object"); return false; }
    if (strcmp(type, "array") == 0 && !cJSON_IsArray(node)) { *error = strdup("Expected array"); return false; }
    return true;
}

static bool validate_object(cJSON *node, cJSON *schema, char **error) {
    cJSON *props = cJSON_GetObjectItem(schema, "properties"); if (!props) return true;
    cJSON *required = cJSON_GetObjectItem(schema, "required");
    cJSON *child = props->child;
    while (child) {
        const char *key = child->string;
        cJSON *val_node = cJSON_GetObjectItem(node, key);
        if (!val_node && required) {
            cJSON *req; cJSON_ArrayForEach(req, required) if (req->valuestring && strcmp(req->valuestring, key) == 0) {
                char buf[256]; snprintf(buf, sizeof(buf), "Missing required field: %s", key); *error = strdup(buf); return false;
            }
        }
        if (val_node && !validate_node(val_node, child, error)) return false;
        child = child->next;
    }
    return true;
}

static bool validate_array(cJSON *node, cJSON *schema, char **error) {
    cJSON *items = cJSON_GetObjectItem(schema, "items"); if (!items) return true;
    cJSON *child = node->child;
    while (child) { if (!validate_node(child, items, error)) return false; child = child->next; }
    return true;
}

static bool validate_node(cJSON *node, cJSON *schema, char **error) {
    if (!schema) return true;
    if (!validate_type(node, schema, error)) return false;
    if (node->type == cJSON_Object) return validate_object(node, schema, error);
    if (node->type == cJSON_Array) return validate_array(node, schema, error);
    return true;
}

bool schema_validate(const char *json_str, const char *schema_str, char **error_out) {
    cJSON *node = cJSON_Parse(json_str); if (!node) { *error_out = strdup("Invalid JSON"); return false; }
    cJSON *schema = cJSON_Parse(schema_str); if (!schema) { *error_out = strdup("Invalid schema JSON"); cJSON_Delete(node); return false; }
    char *err = NULL; bool ok = validate_object(node, schema, &err);
    cJSON_Delete(node); cJSON_Delete(schema);
    if (!ok && error_out) *error_out = err; else free(err);
    return ok;
}