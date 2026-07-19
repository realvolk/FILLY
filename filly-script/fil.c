#include "fil.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

typedef enum {
    TOK_WHEN, TOK_THEN, TOK_ELSE, TOK_END, TOK_LET, TOK_SET, TOK_TO,
    TOK_REJECT, TOK_WITH, TOK_ACCEPT, TOK_SHOW, TOK_FOR, TOK_EACH,
    TOK_IN, TOK_DO, TOK_IS, TOK_NOT, TOK_MATCHES, TOK_AND, TOK_OR,
    TOK_EMPTY, TOK_OF, TOK_LESS, TOK_GREATER, TOK_THAN,
    TOK_IDENT, TOK_STRING, TOK_NUMBER, TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int num;
} Token;

typedef struct {
    const char *src;
    int pos;
    Token current;
    const char *(*store_get)(const char *key);
    const char *value;
} Parser;

static sigjmp_buf fil_timeout_jmp;
static volatile sig_atomic_t fil_timed_out = 0;

static void fil_alarm_handler(int sig) {
    (void)sig;
    fil_timed_out = 1;
    siglongjmp(fil_timeout_jmp, 1);
}

static void skip_ws(Parser *p) {
    while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t' || p->src[p->pos] == '\n' || p->src[p->pos] == '\r')
        p->pos++;
}

static Token make_token(TokenType type, char *text, int num) {
    Token t = { .type = type, .text = text, .num = num };
    return t;
}

static void next_token(Parser *p) {
    skip_ws(p);
    free(p->current.text);
    p->current.text = NULL;

    if (p->src[p->pos] == '\0') {
        p->current = make_token(TOK_EOF, NULL, 0);
        return;
    }

    if (p->src[p->pos] == '"') {
        int start = ++p->pos;
        while (p->src[p->pos] && p->src[p->pos] != '"') p->pos++;
        int len = p->pos - start;
        char *str = malloc(len + 1);
        memcpy(str, p->src + start, len);
        str[len] = '\0';
        if (p->src[p->pos] == '"') p->pos++;
        p->current = make_token(TOK_STRING, str, 0);
        return;
    }

    if (isdigit(p->src[p->pos])) {
        int start = p->pos;
        while (isdigit(p->src[p->pos])) p->pos++;
        int len = p->pos - start;
        char *num = malloc(len + 1);
        memcpy(num, p->src + start, len);
        num[len] = '\0';
        p->current = make_token(TOK_NUMBER, NULL, atoi(num));
        free(num);
        return;
    }

    if (isalpha(p->src[p->pos]) || p->src[p->pos] == '_') {
        int start = p->pos;
        while (isalnum(p->src[p->pos]) || p->src[p->pos] == '_') p->pos++;
        int len = p->pos - start;
        char *word = malloc(len + 1);
        memcpy(word, p->src + start, len);
        word[len] = '\0';

        if (strcmp(word, "when") == 0) p->current = make_token(TOK_WHEN, word, 0);
        else if (strcmp(word, "then") == 0) p->current = make_token(TOK_THEN, word, 0);
        else if (strcmp(word, "else") == 0) p->current = make_token(TOK_ELSE, word, 0);
        else if (strcmp(word, "end") == 0) p->current = make_token(TOK_END, word, 0);
        else if (strcmp(word, "let") == 0) p->current = make_token(TOK_LET, word, 0);
        else if (strcmp(word, "set") == 0) p->current = make_token(TOK_SET, word, 0);
        else if (strcmp(word, "to") == 0) p->current = make_token(TOK_TO, word, 0);
        else if (strcmp(word, "reject") == 0) p->current = make_token(TOK_REJECT, word, 0);
        else if (strcmp(word, "with") == 0) p->current = make_token(TOK_WITH, word, 0);
        else if (strcmp(word, "accept") == 0) p->current = make_token(TOK_ACCEPT, word, 0);
        else if (strcmp(word, "show") == 0) p->current = make_token(TOK_SHOW, word, 0);
        else if (strcmp(word, "for") == 0) p->current = make_token(TOK_FOR, word, 0);
        else if (strcmp(word, "each") == 0) p->current = make_token(TOK_EACH, word, 0);
        else if (strcmp(word, "in") == 0) p->current = make_token(TOK_IN, word, 0);
        else if (strcmp(word, "do") == 0) p->current = make_token(TOK_DO, word, 0);
        else if (strcmp(word, "is") == 0) p->current = make_token(TOK_IS, word, 0);
        else if (strcmp(word, "not") == 0) p->current = make_token(TOK_NOT, word, 0);
        else if (strcmp(word, "matches") == 0) p->current = make_token(TOK_MATCHES, word, 0);
        else if (strcmp(word, "and") == 0) p->current = make_token(TOK_AND, word, 0);
        else if (strcmp(word, "or") == 0) p->current = make_token(TOK_OR, word, 0);
        else if (strcmp(word, "empty") == 0) p->current = make_token(TOK_EMPTY, word, 0);
        else if (strcmp(word, "of") == 0) p->current = make_token(TOK_OF, word, 0);
        else if (strcmp(word, "less") == 0) p->current = make_token(TOK_LESS, word, 0);
        else if (strcmp(word, "greater") == 0) p->current = make_token(TOK_GREATER, word, 0);
        else if (strcmp(word, "than") == 0) p->current = make_token(TOK_THAN, word, 0);
        else p->current = make_token(TOK_IDENT, word, 0);
        return;
    }

    p->pos++;
    p->current = make_token(TOK_EOF, NULL, 0);
}

static bool match(Parser *p, TokenType t) {
    if (p->current.type == t) {
        next_token(p);
        return true;
    }
    return false;
}

static int eval_primary(Parser *p, char *result, int maxlen);
static int eval_comparison(Parser *p, char *result, int maxlen);
static int eval_expression(Parser *p, char *result, int maxlen);

static int eval_function(Parser *p, char *result, int maxlen) {
    char func[64];
    strncpy(func, p->current.text, sizeof(func)-1);
    next_token(p);
    if (!match(p, TOK_OF)) return 0;
    char arg[256];
    if (!eval_primary(p, arg, sizeof(arg))) return 0;

    if (strcmp(func, "length") == 0) { snprintf(result, maxlen, "%d", (int)strlen(arg)); return 1; }
    if (strcmp(func, "uppercase") == 0) { for (int i = 0; arg[i]; i++) arg[i] = toupper(arg[i]); strncpy(result, arg, maxlen); return 1; }
    if (strcmp(func, "lowercase") == 0) { for (int i = 0; arg[i]; i++) arg[i] = tolower(arg[i]); strncpy(result, arg, maxlen); return 1; }
    if (strcmp(func, "trim") == 0) {
        char *start = arg; while (*start == ' ') start++;
        char *end = start + strlen(start) - 1; while (end > start && *end == ' ') end--;
        *(end+1) = '\0'; strncpy(result, start, maxlen); return 1;
    }
    if (strcmp(func, "match") == 0 && match(p, TOK_WITH)) {
        char pattern[256]; if (!eval_primary(p, pattern, sizeof(pattern))) return 0;
        regex_t re; if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) == 0) {
            int rc = regexec(&re, arg, 0, NULL, 0); regfree(&re);
            snprintf(result, maxlen, "%s", rc == 0 ? "1" : ""); return 1;
        }
    }
    return 0;
}

static int eval_primary(Parser *p, char *result, int maxlen) {
    if (match(p, TOK_EMPTY)) { result[0] = '\0'; return 1; }
    if (match(p, TOK_NOT)) { char inner[256]; if (!eval_primary(p, inner, sizeof(inner))) return 0; snprintf(result, maxlen, "%s", inner[0] ? "" : "1"); return 1; }
    if (p->current.type == TOK_IDENT) {
        char *key = p->current.text; next_token(p);
        if (strcmp(key, "value") == 0) { strncpy(result, p->value ? p->value : "", maxlen); return 1; }
        if (strncmp(key, "store.", 6) == 0) { const char *val = p->store_get(key + 6); strncpy(result, val ? val : "", maxlen); return 1; }
        const char *val = p->store_get(key); strncpy(result, val ? val : "", maxlen); return 1;
    }
    if (p->current.type == TOK_STRING) { strncpy(result, p->current.text, maxlen); next_token(p); return 1; }
    if (p->current.type == TOK_NUMBER) { snprintf(result, maxlen, "%d", p->current.num); next_token(p); return 1; }
    if (isalpha(p->current.text ? p->current.text[0] : 0) && p->current.text) { char *saved = strdup(p->current.text); next_token(p); if (p->current.type == TOK_OF) { free(saved); return eval_function(p, result, maxlen); } free(saved); return 0; }
    return eval_function(p, result, maxlen);
}

static int eval_comparison(Parser *p, char *result, int maxlen) {
    char left[256]; if (!eval_primary(p, left, sizeof(left))) return 0;
    if (match(p, TOK_IS)) {
        bool negate = match(p, TOK_NOT);
        if (match(p, TOK_EMPTY)) { snprintf(result, maxlen, "%s", negate ? (left[0] ? "1" : "") : (left[0] ? "" : "1")); return 1; }
        if (match(p, TOK_LESS)) { match(p, TOK_THAN); char right[256]; if (!eval_primary(p, right, sizeof(right))) return 0; int l = atoi(left), r = atoi(right); snprintf(result, maxlen, "%s", negate ? (l >= r ? "1" : "") : (l < r ? "1" : "")); return 1; }
        if (match(p, TOK_GREATER)) { match(p, TOK_THAN); char right[256]; if (!eval_primary(p, right, sizeof(right))) return 0; int l = atoi(left), r = atoi(right); snprintf(result, maxlen, "%s", negate ? (l <= r ? "1" : "") : (l > r ? "1" : "")); return 1; }
        char right[256]; if (!eval_primary(p, right, sizeof(right))) return 0;
        bool eq = strcmp(left, right) == 0; snprintf(result, maxlen, "%s", negate ? (eq ? "" : "1") : (eq ? "1" : "")); return 1;
    }
    if (match(p, TOK_MATCHES)) { char pattern[256]; if (!eval_primary(p, pattern, sizeof(pattern))) return 0; regex_t re; if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) == 0) { int rc = regexec(&re, left, 0, NULL, 0); regfree(&re); snprintf(result, maxlen, "%s", rc == 0 ? "1" : ""); return 1; } return 0; }
    strncpy(result, left, maxlen); return 1;
}

static int eval_expression(Parser *p, char *result, int maxlen) {
    char left[256]; if (!eval_comparison(p, left, sizeof(left))) return 0;
    while (match(p, TOK_AND) || match(p, TOK_OR)) {
        bool is_or = (p->current.type == TOK_OR); next_token(p);
        char right[256]; if (!eval_comparison(p, right, sizeof(right))) return 0;
        snprintf(left, sizeof(left), "%s", (is_or ? (left[0] || right[0]) : (left[0] && right[0])) ? "1" : "");
    }
    strncpy(result, left, maxlen); return 1;
}

typedef struct {
    char **set_keys; char **set_vals; int set_count;
    bool rejected; char *error_msg; bool show_widget;
    const char *(*store_get)(const char *key); const char *value; int loop_count;
} EvalState;

static bool eval_statement(Parser *p, EvalState *s);
static bool eval_block(Parser *p, EvalState *s) {
    match(p, TOK_THEN);
    while (p->current.type != TOK_END && p->current.type != TOK_ELSE && p->current.type != TOK_EOF)
        if (!eval_statement(p, s)) return false;
    return true;
}

static bool eval_statement(Parser *p, EvalState *s) {
    if (s->loop_count > 10000) return false;
    if (match(p, TOK_WHEN)) {
        char cond[256]; if (!eval_expression(p, cond, sizeof(cond))) return false;
        if (cond[0]) { if (!eval_block(p, s)) return false; if (match(p, TOK_ELSE)) while (p->current.type != TOK_END && p->current.type != TOK_EOF) next_token(p); }
        else { while (p->current.type != TOK_THEN && p->current.type != TOK_EOF) next_token(p); next_token(p); while (p->current.type != TOK_ELSE && p->current.type != TOK_END && p->current.type != TOK_EOF) if (!eval_statement(p, s)) return false; if (match(p, TOK_ELSE)) eval_block(p, s); }
        match(p, TOK_END); return true;
    }
    if (match(p, TOK_LET) || match(p, TOK_SET)) {
        if (p->current.type != TOK_IDENT) return false;
        char *key = strdup(p->current.text); next_token(p);
        if (!match(p, TOK_TO)) { free(key); return false; }
        char val[256]; if (!eval_expression(p, val, sizeof(val))) { free(key); return false; }
        s->set_count++; s->set_keys = realloc(s->set_keys, s->set_count * sizeof(char *)); s->set_vals = realloc(s->set_vals, s->set_count * sizeof(char *));
        s->set_keys[s->set_count-1] = key; s->set_vals[s->set_count-1] = strdup(val); return true;
    }
    if (match(p, TOK_REJECT)) { if (!match(p, TOK_WITH)) return false; if (p->current.type != TOK_STRING) return false; s->rejected = true; s->error_msg = strdup(p->current.text); next_token(p); return true; }
    if (match(p, TOK_ACCEPT)) { s->rejected = false; s->error_msg = NULL; return true; }
    if (match(p, TOK_SHOW)) { match(p, TOK_WHEN); char cond[256]; if (!eval_expression(p, cond, sizeof(cond))) return false; s->show_widget = cond[0] ? true : false; return true; }
    if (match(p, TOK_FOR)) {
        match(p, TOK_EACH); if (p->current.type != TOK_IDENT) return false; char *var = strdup(p->current.text); next_token(p);
        if (!match(p, TOK_IN)) { free(var); return false; } if (p->current.type != TOK_IDENT) { free(var); return false; } char *arr_key = strdup(p->current.text); next_token(p);
        if (!match(p, TOK_DO)) { free(var); free(arr_key); return false; }
        const char *arr_str = s->store_get(arr_key);
        if (arr_str && arr_str[0] == '[') { cJSON *arr = cJSON_Parse(arr_str); if (arr && arr->type == cJSON_Array) { int count = cJSON_GetArraySize(arr); for (int i = 0; i < count && s->loop_count < 10000; i++) { s->loop_count++; cJSON *item = cJSON_GetArrayItem(arr, i); if (item && item->valuestring) { s->set_count++; s->set_keys = realloc(s->set_keys, s->set_count * sizeof(char *)); s->set_vals = realloc(s->set_vals, s->set_count * sizeof(char *)); s->set_keys[s->set_count-1] = strdup(var); s->set_vals[s->set_count-1] = strdup(item->valuestring); } } cJSON_Delete(arr); } }
        match(p, TOK_END); free(var); free(arr_key); return true;
    }
    return false;
}

FilResult *fil_eval(const char *script, const char *(*store_get)(const char *key), const char *current_value) {
    Parser p = { .src = script, .pos = 0, .store_get = store_get, .value = current_value };
    next_token(&p);
    EvalState s = { .store_get = store_get, .value = current_value, .loop_count = 0 };

    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fil_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, &old_sa);
    alarm(1);
    fil_timed_out = 0;

    if (sigsetjmp(fil_timeout_jmp, 1) == 0) {
        while (p.current.type != TOK_EOF) if (!eval_statement(&p, &s)) break;
    }

    alarm(0);
    sigaction(SIGALRM, &old_sa, NULL);
    free(p.current.text);

    FilResult *r = calloc(1, sizeof(FilResult));
    if (fil_timed_out) {
        r->accepted = false;
        r->error_msg = strdup("Script execution timed out");
        return r;
    }
    r->accepted = !s.rejected;
    r->error_msg = s.error_msg;
    r->set_keys = s.set_keys;
    r->set_vals = s.set_vals;
    r->set_count = s.set_count;
    r->show_widget = s.show_widget;
    return r;
}

void fil_result_free(FilResult *r) {
    if (!r) return;
    free(r->error_msg);
    for (int i = 0; i < r->set_count; i++) { free(r->set_keys[i]); free(r->set_vals[i]); }
    free(r->set_keys); free(r->set_vals); free(r);
}