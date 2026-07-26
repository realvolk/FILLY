/* Stripped cJSON for FILLY — only the functions used by the codebase.
 * Original copyright: Dave Gamble and cJSON contributors, MIT license.
 */
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>

#define true ((cJSON_bool)1)
#define false ((cJSON_bool)0)

#ifndef NAN
#define NAN (0.0/0.0)
#endif

static void *(*cjson_malloc)(size_t) = malloc;
static void (*cjson_free)(void *) = free;

static unsigned char* cJSON_strdup(const unsigned char* string) {
    size_t length;
    unsigned char *copy = NULL;
    if (string == NULL) return NULL;
    length = strlen((const char*)string) + 1;
    copy = (unsigned char*)cjson_malloc(length);
    if (copy) memcpy(copy, string, length);
    return copy;
}

static cJSON *cJSON_New_Item(void) {
    cJSON* node = (cJSON*)cjson_malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

CJSON_PUBLIC(void) cJSON_Delete(cJSON *item) {
    cJSON *next;
    while (item) {
        next = item->next;
        if (!(item->type & cJSON_IsReference) && item->child)
            cJSON_Delete(item->child);
        if (!(item->type & cJSON_IsReference) && item->valuestring) {
            cjson_free(item->valuestring);
            item->valuestring = NULL;
        }
        if (!(item->type & cJSON_StringIsConst) && item->string) {
            cjson_free(item->string);
            item->string = NULL;
        }
        cjson_free(item);
        item = next;
    }
}

typedef struct {
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth;
} parse_buffer;

#define can_read(buffer, size) ((buffer) && ((buffer)->offset + size) <= (buffer)->length)
#define can_access_at_index(buffer, index) ((buffer) && ((buffer)->offset + index) < (buffer)->length)
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

static parse_buffer *buffer_skip_whitespace(parse_buffer *buffer) {
    if (!buffer || !buffer->content) return NULL;
    while (can_access_at_index(buffer, 0) && buffer_at_offset(buffer)[0] <= 32)
        buffer->offset++;
    if (buffer->offset == buffer->length) buffer->offset--;
    return buffer;
}

static cJSON_bool parse_value(cJSON *item, parse_buffer *input_buffer);
static cJSON_bool parse_string(cJSON *item, parse_buffer *input_buffer);
static cJSON_bool parse_number(cJSON *item, parse_buffer *input_buffer);
static cJSON_bool parse_array(cJSON *item, parse_buffer *input_buffer);
static cJSON_bool parse_object(cJSON *item, parse_buffer *input_buffer);

static unsigned parse_hex4(const unsigned char *input) {
    unsigned h = 0;
    for (int i = 0; i < 4; i++) {
        if (input[i] >= '0' && input[i] <= '9') h += (unsigned)(input[i] - '0');
        else if (input[i] >= 'A' && input[i] <= 'F') h += 10 + input[i] - 'A';
        else if (input[i] >= 'a' && input[i] <= 'f') h += 10 + input[i] - 'a';
        else return 0;
        if (i < 3) h <<= 4;
    }
    return h;
}

static unsigned char utf16_literal_to_utf8(const unsigned char *input, const unsigned char *input_end,
                                            unsigned char **output_pointer) {
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input;
    unsigned char utf8_length = 0, utf8_position = 0, sequence_length = 0, first_byte_mark = 0;

    if ((input_end - first_sequence) < 6) goto fail;
    first_code = parse_hex4(first_sequence + 2);
    if ((first_code >= 0xDC00) && (first_code <= 0xDFFF)) goto fail;

    if ((first_code >= 0xD800) && (first_code <= 0xDBFF)) {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12;
        if ((input_end - second_sequence) < 6) goto fail;
        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u')) goto fail;
        second_code = parse_hex4(second_sequence + 2);
        if ((second_code < 0xDC00) || (second_code > 0xDFFF)) goto fail;
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    } else {
        sequence_length = 6;
        codepoint = first_code;
    }

    if (codepoint < 0x80) { utf8_length = 1; }
    else if (codepoint < 0x800) { utf8_length = 2; first_byte_mark = 0xC0; }
    else if (codepoint < 0x10000) { utf8_length = 3; first_byte_mark = 0xE0; }
    else if (codepoint <= 0x10FFFF) { utf8_length = 4; first_byte_mark = 0xF0; }
    else goto fail;

    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--) {
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    if (utf8_length > 1)
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    else
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    *output_pointer += utf8_length;
    return sequence_length;

fail:
    return 0;
}

static cJSON_bool parse_string(cJSON *item, parse_buffer *input_buffer) {
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;
    size_t allocation_length = 0, skipped_bytes = 0;

    if (buffer_at_offset(input_buffer)[0] != '\"') goto fail;

    while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"')) {
        if (input_end[0] == '\\') {
            if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length) goto fail;
            skipped_bytes++;
            input_end++;
        }
        input_end++;
    }
    if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"')) goto fail;

    allocation_length = (size_t)(input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
    output = (unsigned char*)cjson_malloc(allocation_length + 1);
    if (!output) goto fail;

    output_pointer = output;
    while (input_pointer < input_end) {
        if (*input_pointer != '\\') {
            *output_pointer++ = *input_pointer++;
        } else {
            unsigned char seq_len = 2;
            if ((input_end - input_pointer) < 1) goto fail;
            switch (input_pointer[1]) {
                case 'b': *output_pointer++ = '\b'; break;
                case 'f': *output_pointer++ = '\f'; break;
                case 'n': *output_pointer++ = '\n'; break;
                case 'r': *output_pointer++ = '\r'; break;
                case 't': *output_pointer++ = '\t'; break;
                case '\"': case '\\': case '/': *output_pointer++ = input_pointer[1]; break;
                case 'u':
                    seq_len = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                    if (seq_len == 0) goto fail;
                    break;
                default: goto fail;
            }
            input_pointer += seq_len;
        }
    }
    *output_pointer = '\0';
    item->type = cJSON_String;
    item->valuestring = (char*)output;
    input_buffer->offset = (size_t)(input_end - input_buffer->content) + 1;
    return true;

fail:
    if (output) cjson_free(output);
    if (input_pointer) input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    return false;
}

static cJSON_bool parse_number(cJSON *item, parse_buffer *input_buffer) {
    double number = 0;
    unsigned char *after_end = NULL;
    size_t number_string_length = 0;
    unsigned char *number_c_string;
    cJSON_bool has_decimal_point = false;
    size_t i;

    for (i = 0; can_access_at_index(input_buffer, i); i++) {
        char c = buffer_at_offset(input_buffer)[i];
        if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'e' || c == 'E') {
            number_string_length++;
        } else if (c == '.') {
            number_string_length++;
            has_decimal_point = true;
        } else break;
    }
    number_c_string = (unsigned char*)cjson_malloc(number_string_length + 1);
    if (!number_c_string) return false;
    memcpy(number_c_string, buffer_at_offset(input_buffer), number_string_length);
    number_c_string[number_string_length] = '\0';
    if (has_decimal_point) {
        for (i = 0; i < number_string_length; i++)
            if (number_c_string[i] == '.') number_c_string[i] = '.';
    }
    number = strtod((const char*)number_c_string, (char**)&after_end);
    if (number_c_string == after_end) { cjson_free(number_c_string); return false; }
    item->valuedouble = number;
    item->valueint = (number >= INT_MAX) ? INT_MAX : (number <= (double)INT_MIN) ? INT_MIN : (int)number;
    item->type = cJSON_Number;
    input_buffer->offset += (size_t)(after_end - number_c_string);
    cjson_free(number_c_string);
    return true;
}

static cJSON_bool parse_value(cJSON *item, parse_buffer *input_buffer) {
    if (!input_buffer || !input_buffer->content) return false;
    if (can_read(input_buffer, 4) && !strncmp((const char*)buffer_at_offset(input_buffer), "null", 4)) {
        item->type = cJSON_NULL; input_buffer->offset += 4; return true;
    }
    if (can_read(input_buffer, 5) && !strncmp((const char*)buffer_at_offset(input_buffer), "false", 5)) {
        item->type = cJSON_False; input_buffer->offset += 5; return true;
    }
    if (can_read(input_buffer, 4) && !strncmp((const char*)buffer_at_offset(input_buffer), "true", 4)) {
        item->type = cJSON_True; item->valueint = 1; input_buffer->offset += 4; return true;
    }
    if (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == '\"')
        return parse_string(item, input_buffer);
    if (can_access_at_index(input_buffer, 0) &&
        (buffer_at_offset(input_buffer)[0] == '-' ||
         (buffer_at_offset(input_buffer)[0] >= '0' && buffer_at_offset(input_buffer)[0] <= '9')))
        return parse_number(item, input_buffer);
    if (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == '[')
        return parse_array(item, input_buffer);
    if (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == '{')
        return parse_object(item, input_buffer);
    return false;
}

static cJSON_bool parse_array(cJSON *item, parse_buffer *input_buffer) {
    cJSON *head = NULL, *current_item = NULL;
    if (input_buffer->depth >= 1000) return false;
    input_buffer->depth++;
    if (buffer_at_offset(input_buffer)[0] != '[') goto fail;
    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == ']') goto success;
    if (!can_access_at_index(input_buffer, 0)) { input_buffer->offset--; goto fail; }
    input_buffer->offset--;
    do {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) goto fail;
        if (!head) current_item = head = new_item;
        else { current_item->next = new_item; new_item->prev = current_item; current_item = new_item; }
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer)) goto fail;
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == ',');
    if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']') goto fail;
success:
    input_buffer->depth--;
    if (head) head->prev = current_item;
    item->type = cJSON_Array;
    item->child = head;
    input_buffer->offset++;
    return true;
fail:
    if (head) cJSON_Delete(head);
    return false;
}

static cJSON_bool parse_object(cJSON *item, parse_buffer *input_buffer) {
    cJSON *head = NULL, *current_item = NULL;
    if (input_buffer->depth >= 1000) return false;
    input_buffer->depth++;
    if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != '{') goto fail;
    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == '}') goto success;
    if (!can_access_at_index(input_buffer, 0)) { input_buffer->offset--; goto fail; }
    input_buffer->offset--;
    do {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) goto fail;
        if (!head) current_item = head = new_item;
        else { current_item->next = new_item; new_item->prev = current_item; current_item = new_item; }
        if (!can_access_at_index(input_buffer, 1)) goto fail;
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer)) goto fail;
        buffer_skip_whitespace(input_buffer);
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;
        if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ':') goto fail;
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer)) goto fail;
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && buffer_at_offset(input_buffer)[0] == ',');
    if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != '}') goto fail;
success:
    input_buffer->depth--;
    if (head) head->prev = current_item;
    item->type = cJSON_Object;
    item->child = head;
    input_buffer->offset++;
    return true;
fail:
    if (head) cJSON_Delete(head);
    return false;
}

CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value) {
    parse_buffer buffer;
    cJSON *item;
    if (!value) return NULL;
    buffer.content = (const unsigned char*)value;
    buffer.length = strlen(value) + 1;
    buffer.offset = 0;
    buffer.depth = 0;
    item = cJSON_New_Item();
    if (!item) return NULL;
    if (!parse_value(item, buffer_skip_whitespace(&buffer))) { cJSON_Delete(item); return NULL; }
    return item;
}

typedef struct {
    unsigned char *buffer;
    size_t length;
    size_t offset;
    cJSON_bool noalloc;
} printbuffer;

static unsigned char* ensure(printbuffer *p, size_t needed) {
    unsigned char *newbuffer;
    size_t newsize;
    if (!p || !p->buffer) return NULL;
    if (p->length > 0 && p->offset >= p->length) return NULL;
    if (needed > INT_MAX) return NULL;
    needed += p->offset + 1;
    if (needed <= p->length) return p->buffer + p->offset;
    if (p->noalloc) return NULL;
    newsize = needed * 2;
    newbuffer = (unsigned char*)realloc(p->buffer, newsize);
    if (!newbuffer) { free(p->buffer); p->length = 0; p->buffer = NULL; return NULL; }
    p->length = newsize;
    p->buffer = newbuffer;
    return newbuffer + p->offset;
}

static void update_offset(printbuffer *buffer) {
    if (buffer && buffer->buffer)
        buffer->offset += strlen((const char*)(buffer->buffer + buffer->offset));
}

static cJSON_bool print_number(const cJSON *item, printbuffer *output_buffer) {
    unsigned char *output_pointer;
    int length;
    unsigned char number_buffer[26];
    if (!output_buffer) return false;
    if (isnan(item->valuedouble) || isinf(item->valuedouble))
        length = sprintf((char*)number_buffer, "null");
    else if (item->valuedouble == (double)item->valueint)
        length = sprintf((char*)number_buffer, "%d", item->valueint);
    else
        length = sprintf((char*)number_buffer, "%1.15g", item->valuedouble);
    output_pointer = ensure(output_buffer, length + 1);
    if (!output_pointer) return false;
    memcpy(output_pointer, number_buffer, length + 1);
    output_buffer->offset += length;
    return true;
}

static cJSON_bool print_string_ptr(const unsigned char *input, printbuffer *output_buffer) {
    const unsigned char *input_pointer;
    unsigned char *output, *output_pointer;
    size_t output_length, escape_characters = 0;
    if (!output_buffer) return false;
    if (!input) {
        output = ensure(output_buffer, 3);
        if (!output) return false;
        strcpy((char*)output, "\"\"");
        return true;
    }
    for (input_pointer = input; *input_pointer; input_pointer++)
        if (*input_pointer == '\"' || *input_pointer == '\\' || *input_pointer < 32)
            escape_characters++;
    output_length = (size_t)(input_pointer - input) + escape_characters;
    output = ensure(output_buffer, output_length + 3);
    if (!output) return false;
    output_pointer = output;
    *output_pointer++ = '\"';
    for (input_pointer = input; *input_pointer; input_pointer++) {
        if (*input_pointer >= 32 && *input_pointer != '\"' && *input_pointer != '\\')
            *output_pointer++ = *input_pointer;
        else {
            *output_pointer++ = '\\';
            switch (*input_pointer) {
                case '\"': *output_pointer++ = '\"'; break;
                case '\\': *output_pointer++ = '\\'; break;
                case '\b': *output_pointer++ = 'b'; break;
                case '\f': *output_pointer++ = 'f'; break;
                case '\n': *output_pointer++ = 'n'; break;
                case '\r': *output_pointer++ = 'r'; break;
                case '\t': *output_pointer++ = 't'; break;
                default: sprintf((char*)output_pointer, "u%04x", *input_pointer); output_pointer += 4; break;
            }
        }
    }
    *output_pointer++ = '\"';
    *output_pointer = '\0';
    output_buffer->offset += (output_pointer - output);
    return true;
}

static cJSON_bool print_string(const cJSON *item, printbuffer *p) {
    return print_string_ptr((const unsigned char*)item->valuestring, p);
}

static cJSON_bool print_value(const cJSON *item, printbuffer *output_buffer);
static cJSON_bool print_array(const cJSON *item, printbuffer *output_buffer);
static cJSON_bool print_object(const cJSON *item, printbuffer *output_buffer);

static cJSON_bool print_value(const cJSON *item, printbuffer *output_buffer) {
    unsigned char *output;
    if (!item || !output_buffer) return false;
    switch (item->type & 0xFF) {
        case cJSON_NULL:  output = ensure(output_buffer, 5); if (output) strcpy((char*)output, "null"); return output != NULL;
        case cJSON_False: output = ensure(output_buffer, 6); if (output) strcpy((char*)output, "false"); return output != NULL;
        case cJSON_True:  output = ensure(output_buffer, 5); if (output) strcpy((char*)output, "true"); return output != NULL;
        case cJSON_Number: return print_number(item, output_buffer);
        case cJSON_String: return print_string(item, output_buffer);
        case cJSON_Array:  return print_array(item, output_buffer);
        case cJSON_Object: return print_object(item, output_buffer);
        default: return false;
    }
}

static cJSON_bool print_array(const cJSON *item, printbuffer *output_buffer) {
    unsigned char *output_pointer;
    cJSON *current = item->child;
    if (!output_buffer) return false;
    output_pointer = ensure(output_buffer, 1);
    if (!output_pointer) return false;
    *output_pointer = '[';
    output_buffer->offset++;
    while (current) {
        if (!print_value(current, output_buffer)) return false;
        update_offset(output_buffer);
        if (current->next) {
            output_pointer = ensure(output_buffer, 2);
            if (!output_pointer) return false;
            *output_pointer++ = ',';
            *output_pointer = '\0';
            output_buffer->offset++;
        }
        current = current->next;
    }
    output_pointer = ensure(output_buffer, 2);
    if (!output_pointer) return false;
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->offset++;
    return true;
}

static cJSON_bool print_object(const cJSON *item, printbuffer *output_buffer) {
    unsigned char *output_pointer;
    cJSON *current = item->child;
    if (!output_buffer) return false;
    output_pointer = ensure(output_buffer, 1);
    if (!output_pointer) return false;
    *output_pointer = '{';
    output_buffer->offset++;
    while (current) {
        if (!print_string_ptr((unsigned char*)current->string, output_buffer)) return false;
        update_offset(output_buffer);
        output_pointer = ensure(output_buffer, 1);
        if (!output_pointer) return false;
        *output_pointer++ = ':';
        *output_pointer = '\0';
        output_buffer->offset++;
        if (!print_value(current, output_buffer)) return false;
        update_offset(output_buffer);
        if (current->next) {
            output_pointer = ensure(output_buffer, 2);
            if (!output_pointer) return false;
            *output_pointer++ = ',';
            *output_pointer = '\0';
            output_buffer->offset++;
        }
        current = current->next;
    }
    output_pointer = ensure(output_buffer, 2);
    if (!output_pointer) return false;
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->offset++;
    return true;
}

static unsigned char *print(const cJSON *item) {
    printbuffer buffer;
    unsigned char *printed;
    buffer.buffer = (unsigned char*)cjson_malloc(256);
    if (!buffer.buffer) return NULL;
    buffer.length = 256;
    buffer.offset = 0;
    buffer.noalloc = false;
    if (!print_value(item, &buffer)) { cjson_free(buffer.buffer); return NULL; }
    update_offset(&buffer);
    printed = (unsigned char*)realloc(buffer.buffer, buffer.offset + 1);
    if (printed) printed[buffer.offset] = '\0';
    return printed;
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item) {
    return (char*)print(item);
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_NULL;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_True;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_False;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = boolean ? cJSON_True : cJSON_False;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint = (num >= INT_MAX) ? INT_MAX : (num <= (double)INT_MIN) ? INT_MIN : (int)num;
    }
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)string);
        if (!item->valuestring) { cJSON_Delete(item); return NULL; }
    }
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Array;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Object;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(arr, cJSON_CreateString(strings[i]));
    return arr;
}

static void suffix_object(cJSON *prev, cJSON *item) { prev->next = item; item->prev = prev; }

static cJSON_bool add_item_to_array(cJSON *array, cJSON *item) {
    cJSON *child;
    if (!item || !array || array == item) return false;
    child = array->child;
    if (!child) {
        array->child = item;
        item->prev = item;
        item->next = NULL;
    } else {
        if (child->prev) suffix_object(child->prev, item);
        array->child->prev = item;
    }
    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item) { return add_item_to_array(array, item); }

static cJSON_bool add_item_to_object(cJSON *object, const char *string, cJSON *item) {
    char *new_key;
    if (!object || !string || !item || object == item) return false;
    new_key = (char*)cJSON_strdup((const unsigned char*)string);
    if (!new_key) return false;
    if (item->string) cjson_free(item->string);
    item->string = new_key;
    return add_item_to_array(object, item);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    return add_item_to_object(object, string, item);
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON *object, const char *name, const char *string) {
    cJSON *item = cJSON_CreateString(string);
    if (add_item_to_object(object, name, item)) return item;
    cJSON_Delete(item);
    return NULL;
}
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON *object, const char *name, const double number) {
    cJSON *item = cJSON_CreateNumber(number);
    if (add_item_to_object(object, name, item)) return item;
    cJSON_Delete(item);
    return NULL;
}
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON *object, const char *name, const cJSON_bool boolean) {
    cJSON *item = cJSON_CreateBool(boolean);
    if (add_item_to_object(object, name, item)) return item;
    cJSON_Delete(item);
    return NULL;
}
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON *object, const char *name) {
    cJSON *item = cJSON_CreateNull();
    if (add_item_to_object(object, name, item)) return item;
    cJSON_Delete(item);
    return NULL;
}

static cJSON *get_array_item(const cJSON *array, size_t index) {
    cJSON *current = array ? array->child : NULL;
    while (current && index > 0) { index--; current = current->next; }
    return current;
}
CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index) {
    if (index < 0) return NULL;
    return get_array_item(array, (size_t)index);
}

static cJSON *get_object_item(const cJSON *object, const char *name) {
    cJSON *current;
    if (!object || !name) return NULL;
    current = object->child;
    while (current && current->string && strcmp(name, current->string) != 0)
        current = current->next;
    if (current && current->string) return current;
    return NULL;
}
CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON *object, const char *string) {
    return get_object_item(object, string);
}

CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array) {
    int size = 0;
    cJSON *child = array ? array->child : NULL;
    while (child) { size++; child = child->next; }
    return size;
}

CJSON_PUBLIC(char *) cJSON_GetStringValue(const cJSON *item) {
    return (cJSON_IsString(item)) ? item->valuestring : NULL;
}
CJSON_PUBLIC(double) cJSON_GetNumberValue(const cJSON *item) {
    return (cJSON_IsNumber(item)) ? item->valuedouble : NAN;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON *item) { return item && (item->type & 0xFF) == cJSON_String; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON *item) { return item && (item->type & 0xFF) == cJSON_Number; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON *item)  { return item && (item->type & 0xFF) == cJSON_Array; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON *item) { return item && (item->type & 0xFF) == cJSON_Object; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON *item)   { return item && (item->type & 0xFF) == cJSON_True; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse(const cJSON *item)  { return item && (item->type & 0xFF) == cJSON_False; }
CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON *item)   { return item && (item->type & (cJSON_True | cJSON_False)); }
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON *item)   { return item && (item->type & 0xFF) == cJSON_NULL; }

static cJSON *cJSON_Duplicate_rec(const cJSON *item, cJSON_bool recurse) {
    cJSON *newitem, *child, *next = NULL, *newchild = NULL;
    if (!item) return NULL;
    newitem = cJSON_New_Item();
    if (!newitem) return NULL;
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring) {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring);
        if (!newitem->valuestring) { cJSON_Delete(newitem); return NULL; }
    }
    if (item->string) {
        newitem->string = (char*)cJSON_strdup((unsigned char*)item->string);
        if (!newitem->string) { cJSON_Delete(newitem); return NULL; }
    }
    if (!recurse) return newitem;
    child = item->child;
    while (child) {
        newchild = cJSON_Duplicate_rec(child, true);
        if (!newchild) { cJSON_Delete(newitem); return NULL; }
        if (next) { next->next = newchild; newchild->prev = next; next = newchild; }
        else { newitem->child = newchild; next = newchild; }
        child = child->next;
    }
    if (newitem && newitem->child) newitem->child->prev = newchild;
    return newitem;
}
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse) {
    return cJSON_Duplicate_rec(item, recurse);
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON *item) {
    if (!parent || !item || (item != parent->child && item->prev == NULL)) return NULL;
    if (item != parent->child) item->prev->next = item->next;
    if (item->next) item->next->prev = item->prev;
    if (item == parent->child) parent->child = item->next;
    else if (item->next == NULL) parent->child->prev = item->prev;
    item->prev = item->next = NULL;
    return item;
}
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string) {
    cJSON *item = cJSON_GetObjectItem(object, string);
    return cJSON_DetachItemViaPointer(object, item);
}