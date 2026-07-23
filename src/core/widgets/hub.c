#include "hub.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct HubItem_s { char *id, *label, *value, *widget, **choices, *placeholder, *message, *display; int choice_count; bool disk_picker; struct{char**keys;char**vals;int count;}visible_if; } HubItem;
typedef struct HubCategory_s { char *id, *label, *summary_template; HubItem *items; int item_count; } HubCategory;
typedef enum { HUB_BROWSING, HUB_EDITING, HUB_CONFIRM_PROCEED, HUB_CONFIRM_QUIT } HubMode;

typedef struct { WidgetBase base; char *title; HubCategory *categories; int cat_count; char **actions; int action_count; int cat_idx, item_idx; char **keys, **vals; int val_count; HubMode mode; Widget *sub_widget; } HubData;
extern Arena *g_session_arena;

static char *hub_get(HubData *d, const char *key) { for(int i=0;i<d->val_count;i++) if(strcmp(d->keys[i],key)==0) return d->vals[i]; return NULL; }
static void hub_set(HubData *d, const char *key, const char *value) { for(int i=0;i<d->val_count;i++) if(strcmp(d->keys[i],key)==0) { free(d->vals[i]); d->vals[i]=strdup(value); return; } d->val_count++; d->keys=realloc(d->keys,d->val_count*sizeof(char*)); d->vals=realloc(d->vals,d->val_count*sizeof(char*)); d->keys[d->val_count-1]=strdup(key); d->vals[d->val_count-1]=strdup(value); }
static bool hub_visible(HubData *d, HubItem *item) { if(item->visible_if.count==0)return true; for(int i=0;i<item->visible_if.count;i++){char*v=hub_get(d,item->visible_if.keys[i]);if(!v)return false;char*cond=strdup(item->visible_if.vals[i]);char*tok=strtok(cond,",");bool matched=false;while(tok){while(*tok==' ')tok++;if(strcmp(v,tok)==0){matched=true;break;}tok=strtok(NULL,",");}free(cond);if(!matched)return false;}return true;}
static HubItem *hub_get_item(HubData *d) { HubCategory *cat = &d->categories[d->cat_idx]; int vi=0; for(int i=0;i<cat->item_count;i++){if(!hub_visible(d,&cat->items[i]))continue;if(vi==d->item_idx)return &cat->items[i];vi++;} return NULL; }
static int hub_visible_count(HubData *d) { HubCategory *cat = &d->categories[d->cat_idx]; int vc=0; for(int i=0;i<cat->item_count;i++) if(hub_visible(d,&cat->items[i])) vc++; return vc; }

static void hub_render(Widget *self, Rect area, RenderTree *out) {
    HubData *d = (HubData *)(self + 1);
    memset(out, 0, sizeof(*out));
    if (d->mode == HUB_EDITING && d->sub_widget) { d->sub_widget->vtable.render(d->sub_widget, area, out); return; }
    out->style_class = "container";
    int box_w = (int)(area.w * 0.95f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.95f); if (box_h > area.h - 2) box_h = area.h - 2;

    if (d->mode == HUB_CONFIRM_PROCEED || d->mode == HUB_CONFIRM_QUIT) {
        RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
        children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
        children[0].text.content = arena_strdup(g_session_arena, d->title); children[0].style_class = "text"; children[0].state = "title";
        children[1].type = RNODE_TEXT; children[1].rect = rect_new(1, 2, box_w - 2, 1);
        children[1].text.content = d->mode == HUB_CONFIRM_PROCEED ? "Proceed?" : "Quit without saving?"; children[1].style_class = "text";
        children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, 4, box_w - 2, 1);
        children[2].text.content = "[Y]es  [N]o"; children[2].style_class = "text";
        out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - 7) / 2, box_w, 7);
        out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
        out->container.children = children; out->container.child_count = 3; return;
    }

    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree)); int idx = 0;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, d->title); children[idx].style_class = "text"; children[idx].state = "title"; idx++;
    int left_w = box_w * 30 / 100, right_x = left_w + 2, right_w = box_w - right_x - 1;

    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, 1, left_w, box_h - 3);
    children[idx].list.item_count = d->cat_count; children[idx].list.selected = d->cat_idx;
    children[idx].list.items = arena_alloc(g_session_arena, d->cat_count * sizeof(ListItem));
    for (int i=0;i<d->cat_count;i++){char l[256];snprintf(l,sizeof(l),"%s %s",i==d->cat_idx?">":" ",d->categories[i].label);children[idx].list.items[i].label=arena_strdup(g_session_arena,l);}
    children[idx].style_class = "list"; idx++;

    HubCategory *cat = &d->categories[d->cat_idx]; int vis = hub_visible_count(d);
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
    children[idx].list.item_count = vis; children[idx].list.selected = d->item_idx;
    children[idx].list.items = arena_alloc(g_session_arena, vis * sizeof(ListItem)); int vi=0;
    for (int i=0;i<cat->item_count;i++){if(!hub_visible(d,&cat->items[i]))continue;HubItem*item=&cat->items[i];char*val=hub_get(d,item->id);char*disp=val?val:item->value;if(item->display&&strcmp(item->display,"set/not set")==0)disp=(val&&strlen(val)>0)?"set":"not set";if(!disp||strlen(disp)==0)disp="(none)";char l[512];snprintf(l,sizeof(l),"%s %s: %s",vi==d->item_idx?">":" ",item->label,disp);children[idx].list.items[vi].label=arena_strdup(g_session_arena,l);vi++;}
    children[idx].style_class = "list"; idx++;

    char footer[512]={0}; for(int i=0;i<d->action_count;i++){char b[32];snprintf(b,sizeof(b),"F%d:%s  ",i+1,d->actions[i]);strcat(footer,b);}
    strcat(footer,"Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit");
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, footer); children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult hub_handle_event(Widget *self, Event *ev, Backend *backend) {
    HubData *d = (HubData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == HUB_CONFIRM_QUIT) { if (ev->code==KEY_CHAR&&(ev->ch=='y'||ev->ch=='Y')) { d->base.accepts_text_input = false; return event_result_response((WidgetResponse){.result=NULL,.cancelled=true}); } d->mode=HUB_BROWSING;d->base.dirty=true;return event_result_handled(); }
    if (d->mode == HUB_CONFIRM_PROCEED) { if (ev->code==KEY_CHAR&&(ev->ch=='y'||ev->ch=='Y')){cJSON*r=cJSON_CreateObject();for(int i=0;i<d->val_count;i++)cJSON_AddStringToObject(r,d->keys[i],d->vals[i]); d->base.accepts_text_input = false; return event_result_response((WidgetResponse){.result=r,.cancelled=false});}d->mode=HUB_BROWSING;d->base.dirty=true;return event_result_handled(); }
    if (d->mode == HUB_EDITING && d->sub_widget) { EventResult r = d->sub_widget->vtable.handle_event(d->sub_widget, ev, backend); if (r.type==EVENT_RESULT_RESPONSE) { HubItem *item = hub_get_item(d); if (item && !r.response.cancelled && r.response.result) { if (r.response.result->valuestring) hub_set(d,item->id,r.response.result->valuestring); else if (r.response.result->type==cJSON_True) hub_set(d,item->id,"yes"); else if (r.response.result->type==cJSON_False) hub_set(d,item->id,"no"); else if (r.response.result->type==cJSON_Array||r.response.result->type==cJSON_Object){char*j=cJSON_PrintUnformatted(r.response.result);hub_set(d,item->id,j);free(j);}} widget_destroy(d->sub_widget); d->sub_widget=NULL; d->mode=HUB_BROWSING; d->base.accepts_text_input = false; d->base.dirty=true; } return event_result_handled(); }
    switch (ev->code) {
        case KEY_ESC: d->mode = HUB_CONFIRM_QUIT; d->base.dirty = true; return event_result_handled();
        case KEY_UP: if (d->item_idx > 0) d->item_idx--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->item_idx + 1 < hub_visible_count(d)) d->item_idx++; d->base.dirty = true; return event_result_handled();
        case KEY_LEFT: d->cat_idx = d->cat_idx>0?d->cat_idx-1:d->cat_count-1; d->item_idx=0; d->base.dirty=true; return event_result_handled();
        case KEY_RIGHT: case KEY_TAB: d->cat_idx = d->cat_idx+1<d->cat_count?d->cat_idx+1:0; d->item_idx=0; d->base.dirty=true; return event_result_handled();
        case KEY_ENTER: { HubItem *item = hub_get_item(d); if (!item) return event_result_handled(); char *cur = hub_get(d,item->id); if(!cur)cur=item->value; WidgetRequest req={.widget=item->widget,.params=cJSON_CreateObject()}; cJSON_AddStringToObject(req.params,"title",item->label); if(item->message&&strlen(item->message))cJSON_AddStringToObject(req.params,"message",item->message); if(cur&&strlen(cur))cJSON_AddStringToObject(req.params,"default",cur); if(item->placeholder&&strlen(item->placeholder))cJSON_AddStringToObject(req.params,"placeholder",item->placeholder); if(item->choice_count>0){cJSON*ch=cJSON_CreateArray();for(int i=0;i<item->choice_count;i++)cJSON_AddItemToArray(ch,cJSON_CreateString(item->choices[i]));cJSON_AddItemToObject(req.params,"choices",ch);} d->sub_widget = widget_registry_create(&req); cJSON_Delete(req.params); if (d->sub_widget) { d->mode = HUB_EDITING; d->base.accepts_text_input = true; d->base.dirty = true; } return event_result_handled(); }
        default: { if (ev->code >= KEY_F1 && ev->code <= KEY_F12) { int f = ev->code - KEY_F1; if (f < d->action_count) { if (strcmp(d->actions[f],"Proceed")==0) { d->mode = HUB_CONFIRM_PROCEED; d->base.dirty = true; return event_result_handled(); } return event_result_response((WidgetResponse){.result=cJSON_CreateString(d->actions[f]),.cancelled=false}); } } return event_result_unhandled(); }
    }
}

static void hub_destroy(Widget *self) { HubData *d = (HubData *)(self + 1); free(d->title); for(int i=0;i<d->cat_count;i++){free(d->categories[i].id);free(d->categories[i].label);free(d->categories[i].summary_template);for(int j=0;j<d->categories[i].item_count;j++){HubItem*item=&d->categories[i].items[j];free(item->id);free(item->label);free(item->value);free(item->widget);for(int k=0;k<item->choice_count;k++)free(item->choices[k]);free(item->choices);free(item->placeholder);free(item->message);free(item->display);for(int k=0;k<item->visible_if.count;k++){free(item->visible_if.keys[k]);free(item->visible_if.vals[k]);}free(item->visible_if.keys);free(item->visible_if.vals);}free(d->categories[i].items);} free(d->categories); for(int i=0;i<d->action_count;i++)free(d->actions[i]);free(d->actions); for(int i=0;i<d->val_count;i++){free(d->keys[i]);free(d->vals[i]);}free(d->keys);free(d->vals); if(d->sub_widget)widget_destroy(d->sub_widget); }

Widget *hub_widget_new(const char *title, cJSON *categories_json, cJSON *actions_json) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(HubData));
    HubData data = { .title = strdup(title), .cat_idx = 0, .item_idx = 0, .mode = HUB_BROWSING, .sub_widget = NULL };
    data.cat_count = 0; data.categories = NULL;
    if (categories_json && cJSON_IsArray(categories_json)) { data.cat_count = cJSON_GetArraySize(categories_json); data.categories = calloc(data.cat_count, sizeof(HubCategory)); int ci=0; cJSON *cv; cJSON_ArrayForEach(cv,categories_json){ cJSON*cid=cJSON_GetObjectItem(cv,"id");cJSON*cl=cJSON_GetObjectItem(cv,"label");cJSON*ct=cJSON_GetObjectItem(cv,"summary_template"); data.categories[ci].id=cid&&cid->valuestring?strdup(cid->valuestring):strdup(""); data.categories[ci].label=cl&&cl->valuestring?strdup(cl->valuestring):strdup(""); data.categories[ci].summary_template=ct&&ct->valuestring?strdup(ct->valuestring):strdup(""); cJSON*items=cJSON_GetObjectItem(cv,"items"); data.categories[ci].item_count=items?cJSON_GetArraySize(items):0; data.categories[ci].items=calloc(data.categories[ci].item_count,sizeof(HubItem)); int ii=0;cJSON*iv;cJSON_ArrayForEach(iv,items){ HubItem*item=&data.categories[ci].items[ii]; cJSON*iid=cJSON_GetObjectItem(iv,"id");item->id=iid&&iid->valuestring?strdup(iid->valuestring):strdup(""); cJSON*il=cJSON_GetObjectItem(iv,"label");item->label=il&&il->valuestring?strdup(il->valuestring):strdup(""); cJSON*ival=cJSON_GetObjectItem(iv,"value");item->value=ival&&ival->valuestring?strdup(ival->valuestring):strdup(""); cJSON*iw=cJSON_GetObjectItem(iv,"widget");item->widget=iw&&iw->valuestring?strdup(iw->valuestring):strdup("menu"); cJSON*ich=cJSON_GetObjectItem(iv,"choices");item->choice_count=ich?cJSON_GetArraySize(ich):0;item->choices=item->choice_count>0?malloc(item->choice_count*sizeof(char*)):NULL;for(int j=0;j<item->choice_count;j++)item->choices[j]=strdup(cJSON_GetArrayItem(ich,j)->valuestring); cJSON*ip=cJSON_GetObjectItem(iv,"placeholder");item->placeholder=ip&&ip->valuestring?strdup(ip->valuestring):strdup(""); cJSON*im=cJSON_GetObjectItem(iv,"message");item->message=im&&im->valuestring?strdup(im->valuestring):strdup(""); cJSON*idisp=cJSON_GetObjectItem(iv,"display");item->display=idisp&&idisp->valuestring?strdup(idisp->valuestring):NULL; cJSON*dp=cJSON_GetObjectItem(iv,"disk_picker");item->disk_picker=dp&&dp->valueint; cJSON*vi=cJSON_GetObjectItem(iv,"visible_if");item->visible_if.keys=NULL;item->visible_if.vals=NULL;item->visible_if.count=0; if(vi&&vi->type==cJSON_Object){item->visible_if.count=cJSON_GetArraySize(vi);item->visible_if.keys=malloc(item->visible_if.count*sizeof(char*));item->visible_if.vals=malloc(item->visible_if.count*sizeof(char*));cJSON*child=vi->child;int k=0;while(child){item->visible_if.keys[k]=strdup(child->string);item->visible_if.vals[k]=child->valuestring?strdup(child->valuestring):strdup("");k++;child=child->next;}} hub_set(&data,item->id,item->value);ii++;} ci++;} }
    data.action_count = 0; data.actions = NULL; if (actions_json && cJSON_IsArray(actions_json)) { data.action_count = cJSON_GetArraySize(actions_json); data.actions = malloc(data.action_count * sizeof(char *)); for (int i=0;i<data.action_count;i++) data.actions[i] = strdup(cJSON_GetArrayItem(actions_json,i)->valuestring); }
    widget_base_init(w, &data, sizeof(HubData), hub_render, hub_handle_event, hub_destroy);
    return w;
}