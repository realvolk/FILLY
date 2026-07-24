/* cJSON.h — Minimal NDJSON parser/serializer for FILLY
 * -----------------------------------------------------------------------------
 * API-compatible subset of cJSON by Dave Gamble (cJSON v1.7.19, MIT).
 * https://github.com/DaveGamble/cJSON
 *
 * Only the functions FILLY actually calls are implemented.  The rest are
 * declared for link compatibility but will abort if called.
 * -----------------------------------------------------------------------------
 */
#ifndef cJSON__h
#define cJSON__h

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

cJSON *cJSON_Parse(const char *value);
char *cJSON_PrintUnformatted(const cJSON *item);
void cJSON_Delete(cJSON *item);

cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateBool(cJSON_bool b);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);

cJSON_bool cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
cJSON_bool cJSON_AddItemToArray(cJSON *array, cJSON *item);
cJSON *cJSON_AddStringToObject(cJSON *object, const char *name, const char *string);
cJSON *cJSON_AddNumberToObject(cJSON *object, const char *name, const double number);
cJSON *cJSON_AddBoolToObject(cJSON *object, const char *name, cJSON_bool b);
cJSON *cJSON_AddNullToObject(cJSON *object, const char *name);

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
int cJSON_GetArraySize(const cJSON *array);
char *cJSON_GetStringValue(const cJSON *item);
double cJSON_GetNumberValue(const cJSON *item);

cJSON_bool cJSON_IsString(const cJSON *item);
cJSON_bool cJSON_IsNumber(const cJSON *item);
cJSON_bool cJSON_IsArray(const cJSON *item);
cJSON_bool cJSON_IsObject(const cJSON *item);
cJSON_bool cJSON_IsTrue(const cJSON *item);
cJSON_bool cJSON_IsFalse(const cJSON *item);
cJSON_bool cJSON_IsBool(const cJSON *item);
cJSON_bool cJSON_IsNull(const cJSON *item);

cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string);

#endif