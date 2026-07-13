#include "render.h"
#include <stdlib.h>
#include <string.h>

TextStyle textstyle_normal(void) {
    TextStyle s = { .fg = "7", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_accent(void) {
    TextStyle s = { .fg = "2", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_muted(void) {
    TextStyle s = { .fg = "8", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_selected(void) {
    TextStyle s = { .fg = "2", .bg = "0", .bold = true, .italic = false, .underline = false };
    return s;
}

Rect rect_new(int x, int y, int w, int h) {
    Rect r = { .x = x, .y = y, .w = w, .h = h };
    return r;
}

EdgeInsets edgeinsets_zero(void) {
    EdgeInsets e = { .top = 0, .bottom = 0, .left = 0, .right = 0 };
    return e;
}

ListItem listitem_new(const char *label) {
    ListItem li = { .label = strdup(label), .meta = NULL };
    return li;
}