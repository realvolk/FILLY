/* cJSON.c — Minimal NDJSON parser/serializer for FILLY
 * -----------------------------------------------------------------------------
 * API-compatible subset of cJSON by Dave Gamble (cJSON v1.7.19, MIT).
 * https://github.com/DaveGamble/cJSON
 *
 * Implements exactly the functions FILLY uses.  No unicode escapes, no
 * pretty printing, no comments, no locale hacks, no hook system.
 * -----------------------------------------------------------------------------
 */
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

/* ---------------------------------------------------------------------------
 * Parser
 * --------------------------------------------------------------------------- */
typedef struct { const char *src; int pos; int len; } PBuf;

static void p_skip(PBuf *p) {
    while (p->pos < p->len && ((unsigned char)p->src[p->pos]) <= 32) p->pos++;
}
static cJSON *p_new(void) { return calloc(1, sizeof(cJSON)); }

static char *p_string(PBuf *p) {
    if (p->src[p->pos] != '"') return NULL;
    p->pos++;
    int s = p->pos;
    while (p->pos < p->len && p->src[p->pos] != '"') { if (p->src[p->pos] == '\\') p->pos++; p->pos++; }
    if (p->pos >= p->len) return NULL;
    int rl = p->pos - s; p->pos++;
    char *o = malloc(rl + 1); int w = 0;
    for (int i = s; i < s + rl; i++) {
        if (p->src[i] == '\\' && i + 1 < s + rl) {
            i++;
            switch (p->src[i]) {
                case '"': o[w++]='"'; break; case '\\': o[w++]='\\'; break;
                case '/': o[w++]='/'; break; case 'b': o[w++]='\b'; break;
                case 'f': o[w++]='\f'; break; case 'n': o[w++]='\n'; break;
                case 'r': o[w++]='\r'; break; case 't': o[w++]='\t'; break;
                default: o[w++]=p->src[i]; break;
            }
        } else o[w++] = p->src[i];
    }
    o[w] = 0; return o;
}

static cJSON *p_val(PBuf *p);

static cJSON *p_arr(PBuf *p) {
    if (p->src[p->pos] != '[') return NULL;
    p->pos++; p_skip(p);
    cJSON *h = NULL, *t = NULL;
    if (p->src[p->pos] == ']') { p->pos++; goto d; }
    do {
        cJSON *it = p_val(p); if (!it) { cJSON_Delete(h); return NULL; }
        if (!h) h = t = it; else { t->next = it; it->prev = t; t = it; }
        p_skip(p);
    } while (p->src[p->pos] == ',' && p->pos++ && (p_skip(p), 1));
    if (p->src[p->pos] != ']') { cJSON_Delete(h); return NULL; }
    p->pos++;
d:  cJSON *a = p_new(); a->type = cJSON_Array; a->child = h; return a;
}

static cJSON *p_obj(PBuf *p) {
    if (p->src[p->pos] != '{') return NULL;
    p->pos++; p_skip(p);
    cJSON *h = NULL, *t = NULL;
    if (p->src[p->pos] == '}') { p->pos++; goto d; }
    do {
        cJSON *it = p_new();
        it->string = p_string(p); if (!it->string) { cJSON_Delete(it); cJSON_Delete(h); return NULL; }
        p_skip(p);
        if (p->src[p->pos] != ':') { cJSON_Delete(it); cJSON_Delete(h); return NULL; }
        p->pos++; p_skip(p);
        cJSON *v = p_val(p); if (!v) { cJSON_Delete(it); cJSON_Delete(h); return NULL; }
        it->type = v->type; it->valuestring = v->valuestring; v->valuestring = NULL;
        it->valueint = v->valueint; it->valuedouble = v->valuedouble;
        it->child = v->child; v->child = NULL; cJSON_Delete(v);
        if (!h) h = t = it; else { t->next = it; it->prev = t; t = it; }
        p_skip(p);
    } while (p->src[p->pos] == ',' && p->pos++ && (p_skip(p), 1));
    if (p->src[p->pos] != '}') { cJSON_Delete(h); return NULL; }
    p->pos++;
d:  cJSON *o = p_new(); o->type = cJSON_Object; o->child = h; return o;
}

static cJSON *p_val(PBuf *p) {
    p_skip(p); if (p->pos >= p->len) return NULL;
    if (p->src[p->pos] == '"') { char *s = p_string(p); if (!s) return NULL; cJSON *n = p_new(); n->type = cJSON_String; n->valuestring = s; return n; }
    if (p->src[p->pos] == '{') return p_obj(p);
    if (p->src[p->pos] == '[') return p_arr(p);
    if (!strncmp(p->src+p->pos, "true", 4)) { p->pos += 4; cJSON *n = p_new(); n->type = cJSON_True; return n; }
    if (!strncmp(p->src+p->pos, "false", 5)) { p->pos += 5; cJSON *n = p_new(); n->type = cJSON_False; return n; }
    if (!strncmp(p->src+p->pos, "null", 4)) { p->pos += 4; return p_new(); }
    char *e; double d = strtod(p->src + p->pos, &e);
    if (e == p->src + p->pos) return NULL;
    p->pos = e - p->src;
    cJSON *n = p_new(); n->type = cJSON_Number; n->valuedouble = d;
    n->valueint = (d >= INT_MAX) ? INT_MAX : (d <= INT_MIN) ? INT_MIN : (int)d;
    return n;
}

cJSON *cJSON_Parse(const char *v) {
    if (!v) return NULL;
    PBuf p = { .src = v, .pos = 0, .len = (int)strlen(v) };
    return p_val(&p);
}

/* ---------------------------------------------------------------------------
 * Serializer
 * --------------------------------------------------------------------------- */
typedef struct { char *b; int cap; int len; } SBuf;

static int s_write(SBuf *s, const char *d, int dl) {
    if (s->len + dl + 1 > s->cap) {
        int nc = s->cap ? s->cap * 2 : 256;
        while (nc < s->len + dl + 1) nc *= 2;
        char *nb = realloc(s->b, nc); if (!nb) return 0;
        s->b = nb; s->cap = nc;
    }
    memcpy(s->b + s->len, d, dl); s->len += dl; s->b[s->len] = 0; return 1;
}
static int s_putc(SBuf *s, char c) { return s_write(s, &c, 1); }
static int s_str(SBuf *s, const char *v) {
    if (!v) return s_write(s, "\"\"", 2);
    s_putc(s, '"');
    for (const char *c = v; *c; c++) {
        switch (*c) {
            case '"': s_write(s, "\\\"", 2); break; case '\\': s_write(s, "\\\\", 2); break;
            case '\b': s_write(s, "\\b", 2); break; case '\f': s_write(s, "\\f", 2); break;
            case '\n': s_write(s, "\\n", 2); break; case '\r': s_write(s, "\\r", 2); break;
            case '\t': s_write(s, "\\t", 2); break; default: s_putc(s, *c); break;
        }
    }
    return s_putc(s, '"');
}
static int s_val(SBuf *s, const cJSON *n);

static int s_arr(SBuf *s, const cJSON *n) {
    s_putc(s, '[');
    for (cJSON *c = n->child; c; c = c->next) { if (!s_val(s, c)) return 0; if (c->next) s_putc(s, ','); }
    return s_putc(s, ']');
}
static int s_obj(SBuf *s, const cJSON *n) {
    s_putc(s, '{');
    for (cJSON *c = n->child; c; c = c->next) {
        if (!s_str(s, c->string)) return 0;
        s_putc(s, ':');
        if (!s_val(s, c)) return 0;
        if (c->next) s_putc(s, ',');
    }
    return s_putc(s, '}');
}
static int s_val(SBuf *s, const cJSON *n) {
    if (!n) return s_write(s, "null", 4);
    switch (n->type) {
        case cJSON_NULL:  return s_write(s, "null", 4);
        case cJSON_True:  return s_write(s, "true", 4);
        case cJSON_False: return s_write(s, "false", 5);
        case cJSON_String: return s_str(s, n->valuestring);
        case cJSON_Number: { char t[32]; snprintf(t, sizeof(t), (n->valuedouble==(int)n->valuedouble)?"%d":"%g", n->valuedouble==(int)n->valuedouble?n->valueint:(int)n->valuedouble); return s_write(s, t, strlen(t)); }
        case cJSON_Array:  return s_arr(s, n);
        case cJSON_Object: return s_obj(s, n);
    }
    return 0;
}
char *cJSON_PrintUnformatted(const cJSON *n) { SBuf s = {0}; if (!s_val(&s, n)) { free(s.b); return NULL; } return s.b; }

/* ---------------------------------------------------------------------------
 * Constructors / Mutation / Access / Type checks / Memory
 * --------------------------------------------------------------------------- */
void cJSON_Delete(cJSON *n) { if (!n) return; cJSON_Delete(n->child); cJSON_Delete(n->next); free(n->string); free(n->valuestring); free(n); }
cJSON *cJSON_CreateNull(void) { return calloc(1, sizeof(cJSON)); }
cJSON *cJSON_CreateBool(cJSON_bool b) { cJSON *n = calloc(1, sizeof(cJSON)); n->type = b ? cJSON_True : cJSON_False; return n; }
cJSON *cJSON_CreateNumber(double d) { cJSON *n = calloc(1, sizeof(cJSON)); n->type = cJSON_Number; n->valuedouble = d; n->valueint = (int)d; return n; }
cJSON *cJSON_CreateString(const char *s) { cJSON *n = calloc(1, sizeof(cJSON)); n->type = cJSON_String; n->valuestring = s ? strdup(s) : strdup(""); return n; }
cJSON *cJSON_CreateArray(void) { cJSON *n = calloc(1, sizeof(cJSON)); n->type = cJSON_Array; return n; }
cJSON *cJSON_CreateObject(void) { cJSON *n = calloc(1, sizeof(cJSON)); n->type = cJSON_Object; return n; }

cJSON_bool cJSON_AddItemToArray(cJSON *a, cJSON *it) {
    if (!a || !it || a->type != cJSON_Array) return 0;
    if (!a->child) { a->child = it; it->prev = it; return 1; }
    cJSON *l = a->child->prev ? a->child->prev : a->child; l->next = it; it->prev = l; a->child->prev = it; return 1;
}
cJSON_bool cJSON_AddItemToObject(cJSON *o, const char *k, cJSON *it) {
    if (!o || !k || !it || o->type != cJSON_Object) return 0;
    it->string = strdup(k); return cJSON_AddItemToArray(o, it);
}
cJSON *cJSON_AddStringToObject(cJSON *o, const char *k, const char *v) { cJSON *n = cJSON_CreateString(v); return cJSON_AddItemToObject(o, k, n) ? n : (cJSON_Delete(n), NULL); }
cJSON *cJSON_AddNumberToObject(cJSON *o, const char *k, double v) { cJSON *n = cJSON_CreateNumber(v); return cJSON_AddItemToObject(o, k, n) ? n : (cJSON_Delete(n), NULL); }
cJSON *cJSON_AddBoolToObject(cJSON *o, const char *k, cJSON_bool v) { cJSON *n = cJSON_CreateBool(v); return cJSON_AddItemToObject(o, k, n) ? n : (cJSON_Delete(n), NULL); }
cJSON *cJSON_AddNullToObject(cJSON *o, const char *k) { cJSON *n = cJSON_CreateNull(); return cJSON_AddItemToObject(o, k, n) ? n : (cJSON_Delete(n), NULL); }

cJSON *cJSON_GetObjectItem(const cJSON *o, const char *k) { if (!o || !k) return NULL; for (cJSON *c = o->child; c; c = c->next) if (c->string && !strcmp(c->string, k)) return c; return NULL; }
cJSON *cJSON_GetArrayItem(const cJSON *a, int i) { if (!a || i < 0) return NULL; int x = 0; for (cJSON *c = a->child; c; c = c->next, x++) if (x == i) return c; return NULL; }
int cJSON_GetArraySize(const cJSON *a) { if (!a) return 0; int n = 0; for (cJSON *c = a->child; c; c = c->next) n++; return n; }
char *cJSON_GetStringValue(const cJSON *n) { return (n && n->type == cJSON_String) ? n->valuestring : NULL; }
double cJSON_GetNumberValue(const cJSON *n) { return (n && n->type == cJSON_Number) ? n->valuedouble : NAN; }

cJSON_bool cJSON_IsString(const cJSON *n) { return n && n->type == cJSON_String; }
cJSON_bool cJSON_IsNumber(const cJSON *n) { return n && n->type == cJSON_Number; }
cJSON_bool cJSON_IsArray(const cJSON *n)  { return n && n->type == cJSON_Array; }
cJSON_bool cJSON_IsObject(const cJSON *n) { return n && n->type == cJSON_Object; }
cJSON_bool cJSON_IsTrue(const cJSON *n)   { return n && n->type == cJSON_True; }
cJSON_bool cJSON_IsFalse(const cJSON *n)  { return n && n->type == cJSON_False; }
cJSON_bool cJSON_IsBool(const cJSON *n)   { return n && (n->type == cJSON_True || n->type == cJSON_False); }
cJSON_bool cJSON_IsNull(const cJSON *n)   { return n && n->type == cJSON_NULL; }

cJSON *cJSON_Duplicate(const cJSON *n, cJSON_bool rec) {
    (void)rec;
    if (!n) return NULL;
    cJSON *o = calloc(1, sizeof(cJSON));
    o->type = n->type; o->valueint = n->valueint; o->valuedouble = n->valuedouble;
    if (n->string) o->string = strdup(n->string);
    if (n->valuestring) o->valuestring = strdup(n->valuestring);
    if (n->child) {
        o->child = cJSON_Duplicate(n->child, 1);
        cJSON *s = n->child->next, *d = o->child;
        while (s) { d->next = cJSON_Duplicate(s, 1); d->next->prev = d; d = d->next; s = s->next; }
    }
    return o;
}
cJSON *cJSON_DetachItemFromObject(cJSON *o, const char *k) {
    if (!o || !k) return NULL;
    for (cJSON *c = o->child; c; c = c->next) {
        if (c->string && !strcmp(c->string, k)) {
            if (c->prev) c->prev->next = c->next;
            if (c->next) c->next->prev = c->prev;
            if (o->child == c) o->child = c->next;
            if (o->child && o->child->prev == c) o->child->prev = c->prev;
            c->next = c->prev = NULL; return c;
        }
    }
    return NULL;
}