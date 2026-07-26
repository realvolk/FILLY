/* Stripped cJSON.h for FILLY */
#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C" {
#endif

#define CJSON_CDECL
#define CJSON_STDCALL
#define CJSON_PUBLIC(type) type

#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 19

#include <stddef.h>

#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7)

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

typedef int cJSON_bool;

#define cJSON_ArrayForEach(element, array) \
    for (element = ((array) ? (array)->child : NULL); element != NULL; element = element->next)

CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value);
CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item);
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item);

CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean);
CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num);
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string);
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count);

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON *object, const char *name, const char *string);
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON *object, const char *name, const double number);
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON *object, const char *name, const cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON *object, const char *name);

CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array);
CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index);
CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON *object, const char *string);

CJSON_PUBLIC(char *) cJSON_GetStringValue(const cJSON *item);
CJSON_PUBLIC(double) cJSON_GetNumberValue(const cJSON *item);

CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON *item);

CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON *item);

#ifdef __cplusplus
}
#endif

#endif