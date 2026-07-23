#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct HubItem_s {
    char *id, *label, *value, *widget, **choices, *placeholder, *message, *display;
    int choice_count;
    bool disk_picker;
    struct { char **keys, **vals; int count; } visible_if;
} HubItem;

typedef struct HubCategory_s { char *id, *label, *summary_template; HubItem *items; int item_count; } HubCategory;

typedef enum { HUB_BROWSING, HUB_EDITING_MENU, HUB_EDITING_INPUT, HUB_EDITING_PASSWORD,
               HUB_EDITING_YESNO, HUB_EDITING_FILTER, HUB_EDITING_MULTISELECT,
               HUB_EDITING_SUB_WIDGET, HUB_CONFIRM_PROCEED, HUB_CONFIRM_QUIT } HubMode;

typedef struct {
    WidgetBase base;
    char *title;
    HubCategory *categories; int cat_count;
    char **actions; int action_count;
    int cat_idx, item_idx;
    char **keys, **vals; int val_count;
    HubMode mode;
    int edit_selected, edit_cursor;
    char *edit_text, *edit_pass1, *edit_pass2;
    bool edit_pass_field, edit_yes;
    char *edit_query;
    int *edit_filtered, edit_filtered_count;
    bool *edit_selected_set;
    int edit_min_sel, edit_max_sel;
    Widget *sub_widget;
    WidgetRequest sub_req;
} HubData;

extern Arena *g_session_arena;

static char *hub_get(HubData *d, const char *key) {
    for (int i = 0; i < d->val_count; i++)
        if (strcmp(d->keys[i], key) == 0) return d->vals[i];
    return NULL;
}

static void hub_set(HubData *d, const char *key, const char *value) {
    for (int i = 0; i < d->val_count; i++) {
        if (strcmp(d->keys[i], key) == 0) { free(d->vals[i]); d->vals[i] = strdup(value); return; }
    }
    d->val_count++;
    d->keys = realloc(d->keys, d->val_count * sizeof(char *));
    d->vals = realloc(d->vals, d->val_count * sizeof(char *));
    d->keys[d->val_count-1] = strdup(key);
    d->vals[d->val_count-1] = strdup(value);
}

static bool hub_visible(HubData *d, HubItem *item) {
    if (item->visible_if.count == 0) return true;
    for (int i = 0; i < item->visible_if.count; i++) {
        char *v = hub_get(d, item->visible_if.keys[i]);
        if (!v) return false;
        char *cond = strdup(item->visible_if.vals[i]);
        char *tok = strtok(cond, ",");
        bool matched = false;
        while (tok) { while (*tok == ' ') tok++; if (strcmp(v, tok) == 0) { matched = true; break; } tok = strtok(NULL, ","); }
        free(cond);
        if (!matched) return false;
    }
    return true;
}

static HubItem *hub_get_item(HubData *d, int *out_vi) {
    HubCategory *cat = &d->categories[d->cat_idx];
    int vi = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!hub_visible(d, &cat->items[i])) continue;
        if (vi == d->item_idx) { if (out_vi) *out_vi = vi; return &cat->items[i]; }
        vi++;
    }
    return NULL;
}

static int hub_visible_count(HubData *d) {
    HubCategory *cat = &d->categories[d->cat_idx];
    int vc = 0;
    for (int i = 0; i < cat->item_count; i++) if (hub_visible(d, &cat->items[i])) vc++;
    return vc;
}

static char *get_disks(void) {
    FILE *fp = popen("lsblk -dpno NAME,SIZE,MODEL -e 7 2>/dev/null", "r");
    if (!fp) return strdup("[]");
    char buf[256];
    char *result = strdup("[");
    bool first = true;
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = 0;
        char label[512]; snprintf(label, sizeof(label), "%s", buf);
        char *escaped = malloc(strlen(label) * 2 + 3);
        char *p = escaped;
        *p++ = '"';
        for (char *c = label; *c; c++) { if (*c == '"' || *c == '\\') *p++ = '\\'; *p++ = *c; }
        *p++ = '"'; *p = '\0';
        char *tmp = malloc(strlen(result) + strlen(escaped) + (first ? 1 : 2));
        sprintf(tmp, first ? "%s%s" : "%s,%s", result, escaped);
        free(result); result = tmp;
        first = false;
        free(escaped);
    }
    pclose(fp);
    char *tmp = malloc(strlen(result) + 2);
    sprintf(tmp, "%s]", result);
    free(result);
    return tmp;
}

static void hub_update_filter(HubData *d, char **choices, int count) {
    free(d->edit_filtered);
    if (!d->edit_query || strlen(d->edit_query) == 0) {
        d->edit_filtered = malloc(count * sizeof(int));
        d->edit_filtered_count = count;
        for (int i = 0; i < count; i++) d->edit_filtered[i] = i;
    } else {
        d->edit_filtered = malloc(count * sizeof(int));
        d->edit_filtered_count = 0;
        char *ql = strdup(d->edit_query);
        for (int i = 0; ql[i]; i++) ql[i] = tolower(ql[i]);
        for (int i = 0; i < count; i++) {
            char *cl = strdup(choices[i]);
            for (int j = 0; cl[j]; j++) cl[j] = tolower(cl[j]);
            if (strstr(cl, ql)) d->edit_filtered[d->edit_filtered_count++] = i;
            free(cl);
        }
        free(ql);
    }
    if (d->edit_selected >= d->edit_filtered_count) d->edit_selected = d->edit_filtered_count > 0 ? d->edit_filtered_count - 1 : 0;
}

static void hub_apply_profile(HubData *d, const char *key, const char *variant) {
    hub_set(d, "FS_TYPE", "ext4"); hub_set(d, "BOOTLOADER", "grub");
    hub_set(d, "KERNEL_CHOICE", "linux"); hub_set(d, "INIT", "openrc");
    hub_set(d, "PRIV_ESCALATION", "sudo"); hub_set(d, "USE_LUKS", "no");
    hub_set(d, "USE_LVM", "no"); hub_set(d, "GENERATE_UKI", "no");
    hub_set(d, "ALLOW_OFFLINE", "no"); hub_set(d, "ENABLE_ARCH_REPOS", "no");
    hub_set(d, "MICROCODE_OVERRIDE", "auto"); hub_set(d, "KEEP_BINARY_KERNEL", "yes");
    hub_set(d, "COREUTILS", "gnu"); hub_set(d, "KERNEL_CONFIG_DEPTH", "auto");
    hub_set(d, "POWER_USER", "no"); hub_set(d, "USER_SHELL", "bash");
    hub_set(d, "SWAP_ENABLED", "none"); hub_set(d, "SWAP_SIZE", "0");
    hub_set(d, "ZRAM_PERCENT", "50"); hub_set(d, "BTRFS_LAYOUT", "standard");
    hub_set(d, "NETWORK_STACK", "networkmanager"); hub_set(d, "AUDIO_STACK", "pipewire");
    hub_set(d, "DISPLAY_MANAGER", "none"); hub_set(d, "X_STACK", "xorg");

    typedef struct { const char *profile_key, *wm_de, *display_manager, *x_stack, *enable_arch_repos, *extras, *extras_full, *quick_profile_name, *init, *priv, *keep_binary, *coreutils, *fs_type, *bootloader, *kernel, *net, *audio, *shell, *use_luks, *use_lvm, *generate_uki, *microcode, *power_user, *swap_enabled, *kde_profile; } ProfileDef;
    ProfileDef profiles[] = {
        {.profile_key="QUICK_PROFILE_KDE",.wm_de="kde",.display_manager="sddm",.x_stack="xlibre",.enable_arch_repos="yes",.extras="git flatpak fastfetch firewalld bluez zram-tools firefox neovim alacritty fzf zoxide starship eza btop htop tmux mpv",.quick_profile_name="KDE"},
        {.profile_key="QUICK_PROFILE_XFCE",.wm_de="xfce4",.display_manager="lightdm",.x_stack="xlibre",.extras="git neovim tmux",.extras_full="git firefox neovim alacritty fzf zoxide starship eza btop tmux mpv",.quick_profile_name="XFCE"},
        {.profile_key="QUICK_PROFILE_MANGO",.wm_de="mango",.display_manager="lightdm",.x_stack="none",.enable_arch_repos="yes",.extras="git firefox alacritty waybar wofi swaybg swaylock fzf zoxide starship eza btop tmux",.quick_profile_name="MangoWM"},
        {.profile_key="QUICK_PROFILE_HYPRLAND",.wm_de="hyprland",.x_stack="none",.enable_arch_repos="yes",.extras="git firefox alacritty waybar wofi hyprpaper hyprlock fzf zoxide starship eza btop tmux",.quick_profile_name="Hyprland"},
        {.profile_key="QUICK_PROFILE_SWAY",.wm_de="sway",.x_stack="none",.extras="git firefox alacritty waybar wofi swaybg swaylock fzf zoxide starship eza btop tmux",.quick_profile_name="Sway"},
        {.profile_key="QUICK_PROFILE_NIRI",.wm_de="niri",.x_stack="none",.extras="git firefox alacritty waybar fuzzel swaybg swaylock fzf zoxide starship eza btop tmux",.quick_profile_name="Niri"},
        {.profile_key="QUICK_PROFILE_I3",.wm_de="i3wm",.display_manager="lightdm",.x_stack="xlibre",.enable_arch_repos="yes",.extras="git firefox alacritty fzf zoxide starship eza btop tmux",.quick_profile_name="i3wm"},
        {.profile_key="QUICK_PROFILE_DWM",.wm_de="dwm",.display_manager="lightdm",.x_stack="xlibre",.enable_arch_repos="yes",.extras="git firefox st fzf zoxide starship eza tmux",.quick_profile_name="dwm"},
        {.profile_key="QUICK_PROFILE_LXQT",.wm_de="lxqt",.display_manager="sddm",.x_stack="xlibre",.quick_profile_name="LXQt"},
        {.profile_key="QUICK_PROFILE_LXDE",.wm_de="lxde",.display_manager="lightdm",.x_stack="xlibre",.quick_profile_name="LXDE"},
        {.profile_key="QUICK_PROFILE_CINNAMON",.wm_de="cinnamon",.display_manager="lightdm",.x_stack="xlibre",.quick_profile_name="Cinnamon"},
        {.profile_key="QUICK_PROFILE_BUDGIE",.wm_de="budgie",.display_manager="lightdm",.x_stack="xlibre",.quick_profile_name="Budgie"},
        {.profile_key="QUICK_PROFILE_MOKSHA",.wm_de="moksha",.display_manager="lightdm",.x_stack="xlibre",.extras="git firefox terminology fzf zoxide starship eza tmux",.quick_profile_name="Moksha"},
        {.profile_key="QUICK_PROFILE_COSMIC",.wm_de="cosmic",.display_manager="lightdm",.x_stack="none",.quick_profile_name="COSMIC"},
        {.profile_key="QUICK_PROFILE_SERVER",.wm_de="none",.display_manager="none",.x_stack="none",.priv="doas",.net="dhcpcd+iwd",.audio="none",.extras="git tmux",.extras_full="git firewalld zram-tools tmux",.quick_profile_name="Server"},
        {.profile_key="QUICK_PROFILE_EMBEDDED",.wm_de="none",.display_manager="none",.x_stack="none",.init="busybox",.priv="none",.keep_binary="no",.coreutils="busybox",.kernel="linux-lts",.net="none",.audio="none",.power_user="yes",.swap_enabled="none",.extras="",.quick_profile_name="Embedded"},
        {.profile_key="QUICK_PROFILE_VOLK",.wm_de="kde",.display_manager="lightdm",.x_stack="xlibre",.enable_arch_repos="yes",.init="dinit",.priv="doas",.keep_binary="no",.net="dhcpcd+iwd",.power_user="yes",.extras="git fastfetch tmux htop kitty firewalld flatpak",.quick_profile_name="Volk",.kde_profile="minimal"},
        {.profile_key="QUICK_PROFILE_TESTING",.wm_de="mango",.display_manager="lightdm",.x_stack="none",.enable_arch_repos="yes",.init="s6",.priv="doas",.coreutils="busybox",.fs_type="xfs",.bootloader="limine",.kernel="linux-cachyos-bore",.net="dhcpcd+iwd",.audio="pipewire",.shell="fish",.use_luks="yes",.use_lvm="yes",.generate_uki="yes",.microcode="none",.extras="git fastfetch tmux htop kitty firewalld flatpak",.quick_profile_name="TestingQP"},
    };
    int n = (int)(sizeof(profiles)/sizeof(profiles[0]));
    ProfileDef *pf = NULL;
    for (int i = 0; i < n; i++) if (strcmp(profiles[i].profile_key, key) == 0) { pf = &profiles[i]; break; }
    if (!pf) return;
    if (pf->wm_de) hub_set(d, "WM_DE", pf->wm_de);
    if (pf->display_manager) hub_set(d, "DISPLAY_MANAGER", pf->display_manager);
    if (pf->x_stack) hub_set(d, "X_STACK", pf->x_stack);
    if (pf->enable_arch_repos) hub_set(d, "ENABLE_ARCH_REPOS", pf->enable_arch_repos);
    if (pf->quick_profile_name) hub_set(d, "QUICK_PROFILE", pf->quick_profile_name);
    if (pf->init) hub_set(d, "INIT", pf->init);
    if (pf->priv) hub_set(d, "PRIV_ESCALATION", pf->priv);
    if (pf->keep_binary) hub_set(d, "KEEP_BINARY_KERNEL", pf->keep_binary);
    if (pf->coreutils) hub_set(d, "COREUTILS", pf->coreutils);
    if (pf->fs_type) hub_set(d, "FS_TYPE", pf->fs_type);
    if (pf->bootloader) hub_set(d, "BOOTLOADER", pf->bootloader);
    if (pf->kernel) hub_set(d, "KERNEL_CHOICE", pf->kernel);
    if (pf->net) hub_set(d, "NETWORK_STACK", pf->net);
    if (pf->audio) hub_set(d, "AUDIO_STACK", pf->audio);
    if (pf->shell) hub_set(d, "USER_SHELL", pf->shell);
    if (pf->use_luks) hub_set(d, "USE_LUKS", pf->use_luks);
    if (pf->use_lvm) hub_set(d, "USE_LVM", pf->use_lvm);
    if (pf->generate_uki) hub_set(d, "GENERATE_UKI", pf->generate_uki);
    if (pf->microcode) hub_set(d, "MICROCODE_OVERRIDE", pf->microcode);
    if (pf->power_user) hub_set(d, "POWER_USER", pf->power_user);
    if (pf->swap_enabled) hub_set(d, "SWAP_ENABLED", pf->swap_enabled);
    bool is_full = (variant && strcmp(variant, "Full") == 0);
    if (is_full && pf->extras_full) hub_set(d, "EXTRAS", pf->extras_full);
    else if (pf->extras) hub_set(d, "EXTRAS", pf->extras);
    if (pf->kde_profile) hub_set(d, "KDE_PROFILE", pf->kde_profile);
    else if (strcmp(key, "QUICK_PROFILE_KDE") == 0) hub_set(d, "KDE_PROFILE", variant ? (strcmp(variant,"Full")==0?"full":strcmp(variant,"Desktop")==0?"desktop":"minimal") : "minimal");
}

static void hub_enter_edit(HubData *d, HubItem *item) {
    char *current = hub_get(d, item->id); if (!current) current = item->value;
    if (item->disk_picker) {
        free(item->widget); item->widget = strdup("menu");
        char *disks_json = get_disks();
        cJSON *disks_arr = cJSON_Parse(disks_json); free(disks_json);
        if (disks_arr && disks_arr->type == cJSON_Array) {
            free(item->choices); item->choice_count = cJSON_GetArraySize(disks_arr);
            item->choices = malloc(item->choice_count * sizeof(char *));
            for (int i = 0; i < item->choice_count; i++) item->choices[i] = strdup(cJSON_GetArrayItem(disks_arr, i)->valuestring);
        }
        if (disks_arr) cJSON_Delete(disks_arr);
        current = item->choices && item->choice_count > 0 ? item->choices[0] : "";
    }
    if (strcmp(item->widget, "menu") == 0 || strcmp(item->widget, "filter") == 0) {
        d->edit_selected = 0;
        for (int i = 0; i < item->choice_count; i++) if (strcmp(item->choices[i], current) == 0) { d->edit_selected = i; break; }
        if (strcmp(item->widget, "filter") == 0) { free(d->edit_query); d->edit_query = strdup(""); hub_update_filter(d, item->choices, item->choice_count); d->mode = HUB_EDITING_FILTER; }
        else d->mode = HUB_EDITING_MENU;
    } else if (strcmp(item->widget, "input") == 0) {
        free(d->edit_text); d->edit_text = strdup(current); d->edit_cursor = strlen(d->edit_text); d->mode = HUB_EDITING_INPUT;
    } else if (strcmp(item->widget, "password") == 0 || strcmp(item->widget, "password_confirm") == 0) {
        free(d->edit_pass1); free(d->edit_pass2); d->edit_pass1 = strdup(""); d->edit_pass2 = strdup(""); d->edit_pass_field = false; d->mode = HUB_EDITING_PASSWORD;
    } else if (strcmp(item->widget, "yesno") == 0) {
        d->edit_yes = (strcmp(current, "yes") == 0); d->mode = HUB_EDITING_YESNO;
    } else if (strcmp(item->widget, "multiselect") == 0) {
        free(d->edit_query); d->edit_query = strdup(""); free(d->edit_selected_set);
        d->edit_selected_set = calloc(item->choice_count, sizeof(bool)); d->edit_min_sel = 0; d->edit_max_sel = item->choice_count;
        hub_update_filter(d, item->choices, item->choice_count); d->edit_selected = 0; d->mode = HUB_EDITING_MULTISELECT;
    } else {
        memset(&d->sub_req, 0, sizeof(d->sub_req)); d->sub_req.widget = item->widget; d->sub_req.params = cJSON_CreateObject();
        cJSON_AddStringToObject(d->sub_req.params, "title", item->label);
        if (item->message && strlen(item->message)) cJSON_AddStringToObject(d->sub_req.params, "message", item->message);
        if (current && strlen(current)) cJSON_AddStringToObject(d->sub_req.params, "default", current);
        if (item->placeholder && strlen(item->placeholder)) cJSON_AddStringToObject(d->sub_req.params, "placeholder", item->placeholder);
        if (item->choice_count > 0) { cJSON *ch = cJSON_CreateArray(); for (int i = 0; i < item->choice_count; i++) cJSON_AddItemToArray(ch, cJSON_CreateString(item->choices[i])); cJSON_AddItemToObject(d->sub_req.params, "choices", ch); }
        d->sub_widget = widget_registry_create(&d->sub_req); d->mode = HUB_EDITING_SUB_WIDGET;
    }
    d->base.dirty = true;
}

static void hub_render_overlay(HubData *d, Rect area, RenderTree *out) {
    if (d->mode == HUB_EDITING_SUB_WIDGET && d->sub_widget) { d->sub_widget->vtable.render(d->sub_widget, area, out); return; }
    int ow = (int)(area.w * 0.55f), oh = (int)(area.h * 0.60f);
    if (ow > area.w - 4) ow = area.w - 4; if (oh > area.h - 4) oh = area.h - 4;
    int ox = (area.w - ow) / 2, oy = (area.h - oh) / 2;
    RenderTree *children = NULL; int child_count = 0;
    HubItem *item = hub_get_item(d, NULL);
    const char *msg = item && item->message ? item->message : "";
    int msg_lines = 0; for (const char *p = msg; *p; p++) if (*p == '\n') msg_lines++;
    int msg_h = strlen(msg) > 0 ? msg_lines + 1 : 0;

    switch (d->mode) {
    case HUB_EDITING_MENU: {
        child_count = 3 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int list_y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 1, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; list_y = 1 + msg_h; }
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, list_y, ow - 2, oh - list_y - 2);
        children[ci].list.item_count = item ? item->choice_count : 0; children[ci].list.selected = d->edit_selected;
        children[ci].list.items = arena_alloc(g_session_arena, (item ? item->choice_count : 0) * sizeof(ListItem));
        if (item) for (int i = 0; i < item->choice_count; i++) children[ci].list.items[i].label = arena_strdup(g_session_arena, item->choices[i]);
        children[ci].style_class = "list"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = "Up/Down:move  Enter:select  Esc:cancel"; children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case HUB_EDITING_INPUT: {
        child_count = 3 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int input_y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 1, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; input_y = 1 + msg_h; }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, input_y, ow - 2, 1);
        children[ci].input.text = arena_strdup(g_session_arena, d->edit_text ? d->edit_text : ""); children[ci].input.cursor = d->edit_cursor;
        children[ci].input.placeholder = arena_strdup(g_session_arena, item && item->placeholder ? item->placeholder : ""); children[ci].style_class = "input"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = "Enter:confirm  Esc:cancel"; children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case HUB_EDITING_PASSWORD: {
        child_count = 4 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; y += msg_h; }
        char mask1[128] = {0}; for (int j = 0; j < (int)strlen(d->edit_pass1) && j < 127; j++) mask1[j] = '*';
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char buf[256]; snprintf(buf, sizeof(buf), "Password: %s", mask1); children[ci].text.content = arena_strdup(g_session_arena, buf);
        children[ci].style_class = "text"; ci++; y++;
        char mask2[128] = {0}; for (int j = 0; j < (int)strlen(d->edit_pass2) && j < 127; j++) mask2[j] = '*';
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        snprintf(buf, sizeof(buf), "Confirm:  %s", mask2); children[ci].text.content = arena_strdup(g_session_arena, buf);
        children[ci].style_class = "text"; ci++; y++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = "Tab:next  Enter:submit  Esc:cancel"; children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case HUB_EDITING_YESNO: {
        child_count = 3 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; y += msg_h; }
        char yesno_text[64]; snprintf(yesno_text, sizeof(yesno_text), "[ %s ]  [ %s ]", d->edit_yes ? "Yes" : "yes", d->edit_yes ? "no" : "No");
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y + 1, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, yesno_text); children[ci].style_class = "text"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = "Left/Right:choose  Enter:confirm  y/n:quick  Esc:cancel"; children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case HUB_EDITING_FILTER: {
        child_count = 4 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; y += msg_h; }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char qd[256]; snprintf(qd, sizeof(qd), "> %s", d->edit_query ? d->edit_query : "");
        children[ci].input.text = arena_strdup(g_session_arena, qd); children[ci].input.cursor = strlen(qd);
        children[ci].input.placeholder = "Type to filter..."; children[ci].style_class = "input"; ci++; y++;
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, y, ow - 2, oh - y - 2);
        children[ci].list.item_count = d->edit_filtered_count; children[ci].list.selected = d->edit_selected;
        children[ci].list.items = arena_alloc(g_session_arena, d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++) children[ci].list.items[i].label = arena_strdup(g_session_arena, item->choices[d->edit_filtered[i]]);
        children[ci].style_class = "list"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = "Type:filter  Up/Down:move  Enter:select  Esc:cancel"; children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case HUB_EDITING_MULTISELECT: {
        child_count = 4 + (msg_h > 0 ? 1 : 0); children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree)); int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, item ? item->label : ""); children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) { children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h); children[ci].text.content = arena_strdup(g_session_arena, msg); children[ci].style_class = "text"; ci++; y += msg_h; }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char qd2[256]; snprintf(qd2, sizeof(qd2), "> %s", d->edit_query ? d->edit_query : "");
        children[ci].input.text = arena_strdup(g_session_arena, qd2); children[ci].input.cursor = strlen(qd2);
        children[ci].input.placeholder = "Type to filter..."; children[ci].style_class = "input"; ci++; y++;
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, y, ow - 2, oh - y - 2);
        children[ci].list.item_count = d->edit_filtered_count; children[ci].list.selected = d->edit_selected;
        children[ci].list.items = arena_alloc(g_session_arena, d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++) {
            int orig = d->edit_filtered[i]; char label[512];
            snprintf(label, sizeof(label), "%s %s", d->edit_selected_set[orig] ? "[x]" : "[ ]", item->choices[orig]);
            children[ci].list.items[i].label = arena_strdup(g_session_arena, label);
        }
        children[ci].style_class = "list"; ci++;
        int sel_count = 0; for (int i = 0; i < item->choice_count; i++) if (d->edit_selected_set[i]) sel_count++;
        char footer[256]; snprintf(footer, sizeof(footer), "%d selected  Space:toggle  Enter:confirm  Esc:cancel", sel_count);
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].text.content = arena_strdup(g_session_arena, footer); children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    default: break;
    }
    out->type = RNODE_CONTAINER; out->rect = rect_new(ox, oy, ow, oh);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = child_count;
}

static void hub_render(Widget *self, Rect area, RenderTree *out) {
    HubData *d = (HubData *)(self + 1);
    memset(out, 0, sizeof(*out));
    if (d->mode == HUB_EDITING_SUB_WIDGET && d->sub_widget) { d->sub_widget->vtable.render(d->sub_widget, area, out); return; }
    if (d->mode >= HUB_EDITING_MENU && d->mode <= HUB_EDITING_MULTISELECT) { hub_render_overlay(d, area, out); return; }
    out->style_class = "container";
    int box_w = (int)(area.w * 0.95f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.95f); if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    if (d->mode == HUB_CONFIRM_PROCEED || d->mode == HUB_CONFIRM_QUIT) {
        RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
        children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
        children[0].text.content = arena_strdup(g_session_arena, d->title); children[0].style_class = "text"; children[0].state = "title";
        children[1].type = RNODE_TEXT; children[1].rect = rect_new(1, 2, box_w - 2, 1);
        children[1].text.content = d->mode == HUB_CONFIRM_PROCEED ? "Proceed with installation?" : "Quit without saving?";
        children[1].style_class = "text";
        children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, 4, box_w - 2, 1);
        children[2].text.content = "[Y]es  [N]o"; children[2].style_class = "text";
        out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, 7);
        out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
        out->container.children = children; out->container.child_count = 3;
        return;
    }

    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, d->title); children[idx].style_class = "text"; children[idx].state = "title"; idx++;
    int left_w = box_w * 30 / 100, right_x = left_w + 2, right_w = box_w - right_x - 1;
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, 1, left_w, box_h - 3);
    children[idx].list.item_count = d->cat_count; children[idx].list.selected = d->cat_idx;
    children[idx].list.items = arena_alloc(g_session_arena, d->cat_count * sizeof(ListItem));
    for (int i = 0; i < d->cat_count; i++) {
        char label[256]; snprintf(label, sizeof(label), "%s %s", i == d->cat_idx ? ">" : " ", d->categories[i].label);
        children[idx].list.items[i].label = arena_strdup(g_session_arena, label);
    }
    children[idx].style_class = "list"; idx++;
    HubCategory *cat = &d->categories[d->cat_idx]; int vis_count = hub_visible_count(d);
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
    children[idx].list.item_count = vis_count; children[idx].list.selected = d->item_idx;
    children[idx].list.items = arena_alloc(g_session_arena, vis_count * sizeof(ListItem));
    int vi = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!hub_visible(d, &cat->items[i])) continue;
        HubItem *item = &cat->items[i]; char *val = hub_get(d, item->id);
        char *disp = val ? val : item->value;
        if (item->display && strcmp(item->display, "set/not set") == 0) disp = (val && strlen(val) > 0) ? "set" : "not set";
        if (!disp || strlen(disp) == 0) disp = "(none)";
        char label[512]; snprintf(label, sizeof(label), "%s %s: %s", vi == d->item_idx ? ">" : " ", item->label, disp);
        children[idx].list.items[vi].label = arena_strdup(g_session_arena, label); vi++;
    }
    children[idx].style_class = "list"; idx++;
    char footer[512] = {0};
    for (int i = 0; i < d->action_count; i++) { char buf[32]; snprintf(buf, sizeof(buf), "F%d:%s  ", i + 1, d->actions[i]); strcat(footer, buf); }
    strcat(footer, "Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit");
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, footer); children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult hub_handle_edit_event(HubData *d, Event *ev) {
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == HUB_EDITING_SUB_WIDGET && d->sub_widget) {
        EventResult r = d->sub_widget->vtable.handle_event(d->sub_widget, ev, NULL);
        if (r.type == EVENT_RESULT_RESPONSE) {
            HubItem *item = hub_get_item(d, NULL);
            if (item && !r.response.cancelled && r.response.result) {
                if (r.response.result->valuestring) hub_set(d, item->id, r.response.result->valuestring);
                else if (r.response.result->type == cJSON_True) hub_set(d, item->id, "yes");
                else if (r.response.result->type == cJSON_False) hub_set(d, item->id, "no");
                else if (r.response.result->type == cJSON_Array || r.response.result->type == cJSON_Object) { char *j = cJSON_PrintUnformatted(r.response.result); hub_set(d, item->id, j); free(j); }
            }
            widget_destroy(d->sub_widget); d->sub_widget = NULL; cJSON_Delete(d->sub_req.params);
            d->mode = HUB_BROWSING; d->base.dirty = true;
        }
        return event_result_handled();
    }
    switch (d->mode) {
    case HUB_EDITING_MENU: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < item->choice_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: hub_set(d, item->id, item->choices[d->edit_selected]); if (strncmp(item->id, "QUICK_PROFILE_", 14) == 0) hub_apply_profile(d, item->id, item->choices[d->edit_selected]); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case HUB_EDITING_INPUT: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: hub_set(d, item->id, d->edit_text); if (strncmp(item->id, "QUICK_PROFILE_", 14) == 0) hub_apply_profile(d, item->id, d->edit_text); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = strlen(d->edit_text); d->edit_text = realloc(d->edit_text, len + 2); memmove(d->edit_text + d->edit_cursor + 1, d->edit_text + d->edit_cursor, len - d->edit_cursor + 1); d->edit_text[d->edit_cursor] = ev->ch; d->edit_cursor++; d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: if (d->edit_cursor > 0) { memmove(d->edit_text + d->edit_cursor - 1, d->edit_text + d->edit_cursor, strlen(d->edit_text + d->edit_cursor) + 1); d->edit_cursor--; d->base.dirty = true; } return event_result_handled();
            case KEY_LEFT: if (d->edit_cursor > 0) d->edit_cursor--; return event_result_handled();
            case KEY_RIGHT: if (d->edit_cursor < (int)strlen(d->edit_text)) d->edit_cursor++; return event_result_handled();
            case KEY_HOME: d->edit_cursor = 0; return event_result_handled();
            case KEY_END: d->edit_cursor = strlen(d->edit_text); return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case HUB_EDITING_PASSWORD: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_TAB: d->edit_pass_field = !d->edit_pass_field; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: if (strlen(d->edit_pass1) > 0 && strcmp(d->edit_pass1, d->edit_pass2) == 0) { hub_set(d, item->id, d->edit_pass1); d->mode = HUB_BROWSING; d->base.dirty = true; } return event_result_handled();
            case KEY_CHAR: { char **t = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1; int len = strlen(*t); *t = realloc(*t, len + 2); (*t)[len] = ev->ch; (*t)[len + 1] = '\0'; d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: { char **t = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1; if (strlen(*t) > 0) (*t)[strlen(*t) - 1] = '\0'; d->base.dirty = true; return event_result_handled(); }
            default: return event_result_unhandled();
        }
        break; }
    case HUB_EDITING_YESNO: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_LEFT: case KEY_RIGHT: case KEY_TAB: d->edit_yes = !d->edit_yes; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: hub_set(d, item->id, d->edit_yes ? "yes" : "no"); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: if (ev->ch == 'y' || ev->ch == 'Y') { hub_set(d, item->id, "yes"); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled(); } if (ev->ch == 'n' || ev->ch == 'N') { hub_set(d, item->id, "no"); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled(); } return event_result_unhandled();
            default: return event_result_unhandled();
        }
        break; }
    case HUB_EDITING_FILTER: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) { int orig = d->edit_filtered[d->edit_selected]; hub_set(d, item->id, item->choices[orig]); } d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = strlen(d->edit_query); d->edit_query = realloc(d->edit_query, len + 2); d->edit_query[len] = ev->ch; d->edit_query[len + 1] = '\0'; d->edit_selected = 0; hub_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: if (strlen(d->edit_query) > 0) { d->edit_query[strlen(d->edit_query) - 1] = '\0'; d->edit_selected = 0; hub_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case HUB_EDITING_MULTISELECT: { HubItem *item = hub_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: { cJSON *arr = cJSON_CreateArray(); for (int i = 0; i < item->choice_count; i++) if (d->edit_selected_set[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(item->choices[i])); char *joined = cJSON_PrintUnformatted(arr); hub_set(d, item->id, joined); free(joined); cJSON_Delete(arr); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled(); }
            case KEY_CHAR: if (ev->ch == ' ') { if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) { int orig = d->edit_filtered[d->edit_selected]; d->edit_selected_set[orig] = !d->edit_selected_set[orig]; d->base.dirty = true; } } else { int len = strlen(d->edit_query); d->edit_query = realloc(d->edit_query, len + 2); d->edit_query[len] = ev->ch; d->edit_query[len + 1] = '\0'; d->edit_selected = 0; hub_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            case KEY_BACKSPACE: if (strlen(d->edit_query) > 0) { d->edit_query[strlen(d->edit_query) - 1] = '\0'; d->edit_selected = 0; hub_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    default: break;
    }
    return event_result_unhandled();
}

static EventResult hub_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    HubData *d = (HubData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == HUB_EDITING_SUB_WIDGET && d->sub_widget) return hub_handle_edit_event(d, ev);
    if (d->mode >= HUB_EDITING_MENU && d->mode <= HUB_EDITING_MULTISELECT) return hub_handle_edit_event(d, ev);
    if (d->mode == HUB_CONFIRM_QUIT) { if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true }); d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled(); }
    if (d->mode == HUB_CONFIRM_PROCEED) { if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) { cJSON *result = cJSON_CreateObject(); for (int i = 0; i < d->val_count; i++) cJSON_AddStringToObject(result, d->keys[i], d->vals[i]); return event_result_response((WidgetResponse){ .result = result, .cancelled = false }); } d->mode = HUB_BROWSING; d->base.dirty = true; return event_result_handled(); }
    switch (ev->code) {
        case KEY_ESC: d->mode = HUB_CONFIRM_QUIT; d->base.dirty = true; return event_result_handled();
        case KEY_UP: if (d->item_idx > 0) d->item_idx--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: { int vc = hub_visible_count(d); if (d->item_idx + 1 < vc) d->item_idx++; d->base.dirty = true; return event_result_handled(); }
        case KEY_LEFT: d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : d->cat_count - 1; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: case KEY_TAB: d->cat_idx = d->cat_idx + 1 < d->cat_count ? d->cat_idx + 1 : 0; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: { HubItem *item = hub_get_item(d, NULL); if (!item) return event_result_handled(); hub_enter_edit(d, item); return event_result_handled(); }
        default: { if (ev->code >= KEY_F1 && ev->code <= KEY_F12) { int f = ev->code - KEY_F1; if (f < d->action_count) { if (strcmp(d->actions[f], "Proceed") == 0) { d->mode = HUB_CONFIRM_PROCEED; d->base.dirty = true; return event_result_handled(); } return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->actions[f]), .cancelled = false }); } } return event_result_unhandled(); }
    }
}

static void hub_destroy(Widget *self) {
    HubData *d = (HubData *)(self + 1);
    free(d->title);
    for (int i = 0; i < d->cat_count; i++) {
        free(d->categories[i].id); free(d->categories[i].label); free(d->categories[i].summary_template);
        for (int j = 0; j < d->categories[i].item_count; j++) {
            HubItem *item = &d->categories[i].items[j];
            free(item->id); free(item->label); free(item->value); free(item->widget);
            for (int k = 0; k < item->choice_count; k++) free(item->choices[k]); free(item->choices);
            free(item->placeholder); free(item->message); free(item->display);
            for (int k = 0; k < item->visible_if.count; k++) { free(item->visible_if.keys[k]); free(item->visible_if.vals[k]); }
            free(item->visible_if.keys); free(item->visible_if.vals);
        }
        free(d->categories[i].items);
    }
    free(d->categories);
    for (int i = 0; i < d->action_count; i++) free(d->actions[i]); free(d->actions);
    for (int i = 0; i < d->val_count; i++) { free(d->keys[i]); free(d->vals[i]); }
    free(d->keys); free(d->vals);
    free(d->edit_text); free(d->edit_pass1); free(d->edit_pass2);
    free(d->edit_query); free(d->edit_filtered); free(d->edit_selected_set);
    if (d->sub_widget) { widget_destroy(d->sub_widget); cJSON_Delete(d->sub_req.params); }
}

Widget *install_hub_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(HubData));
    HubData data;
    memset(&data, 0, sizeof(data));
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(t && t->valuestring ? t->valuestring : "Configuration");

    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        data.cat_count = cJSON_GetArraySize(cats);
        data.categories = calloc(data.cat_count, sizeof(HubCategory));
        int ci = 0; cJSON *cat_val;
        cJSON_ArrayForEach(cat_val, cats) {
            cJSON *cid = cJSON_GetObjectItem(cat_val, "id");
            cJSON *clabel = cJSON_GetObjectItem(cat_val, "label");
            cJSON *ctmpl = cJSON_GetObjectItem(cat_val, "summary_template");
            data.categories[ci].id = cid && cid->valuestring ? strdup(cid->valuestring) : strdup("");
            data.categories[ci].label = clabel && clabel->valuestring ? strdup(clabel->valuestring) : strdup("");
            data.categories[ci].summary_template = ctmpl && ctmpl->valuestring ? strdup(ctmpl->valuestring) : strdup("");
            cJSON *items_arr = cJSON_GetObjectItem(cat_val, "items");
            data.categories[ci].item_count = items_arr ? cJSON_GetArraySize(items_arr) : 0;
            data.categories[ci].items = calloc(data.categories[ci].item_count, sizeof(HubItem));
            int ii = 0; cJSON *item_val;
            cJSON_ArrayForEach(item_val, items_arr) {
                HubItem *item = &data.categories[ci].items[ii];
                cJSON *iid = cJSON_GetObjectItem(item_val, "id"); item->id = iid && iid->valuestring ? strdup(iid->valuestring) : strdup("");
                cJSON *il = cJSON_GetObjectItem(item_val, "label"); item->label = il && il->valuestring ? strdup(il->valuestring) : strdup("");
                cJSON *iv = cJSON_GetObjectItem(item_val, "value"); item->value = iv && iv->valuestring ? strdup(iv->valuestring) : strdup("");
                cJSON *iw = cJSON_GetObjectItem(item_val, "widget"); item->widget = iw && iw->valuestring ? strdup(iw->valuestring) : strdup("menu");
                cJSON *ich = cJSON_GetObjectItem(item_val, "choices");
                cJSON *ichf = cJSON_GetObjectItem(item_val, "choices_file");
                item->choice_count = 0; item->choices = NULL;
                if (ich && ich->type == cJSON_Array) { item->choice_count = cJSON_GetArraySize(ich); item->choices = item->choice_count > 0 ? malloc(item->choice_count * sizeof(char *)) : NULL; for (int j = 0; j < item->choice_count; j++) item->choices[j] = strdup(cJSON_GetArrayItem(ich, j)->valuestring); }
                else if (ichf && ichf->valuestring) { char filepath[1024]; snprintf(filepath, sizeof(filepath), "/tmp/artix-installer/filly-data/%s", ichf->valuestring); FILE *fp = fopen(filepath, "r"); if (fp) { char line[4096]; int cap = 64; item->choices = malloc(cap * sizeof(char *)); item->choice_count = 0; while (fgets(line, sizeof(line), fp)) { line[strcspn(line, "\n")] = 0; if (strlen(line) == 0) continue; if (item->choice_count >= cap) { cap *= 2; item->choices = realloc(item->choices, cap * sizeof(char *)); } item->choices[item->choice_count++] = strdup(line); } fclose(fp); } if (item->choice_count == 0) { item->choices = malloc(sizeof(char *)); item->choices[0] = strdup("(no data)"); item->choice_count = 1; } }
                cJSON *ip = cJSON_GetObjectItem(item_val, "placeholder"); item->placeholder = ip && ip->valuestring ? strdup(ip->valuestring) : strdup("");
                cJSON *im = cJSON_GetObjectItem(item_val, "message"); item->message = im && im->valuestring ? strdup(im->valuestring) : strdup("");
                cJSON *idisp = cJSON_GetObjectItem(item_val, "display"); item->display = idisp && idisp->valuestring ? strdup(idisp->valuestring) : NULL;
                cJSON *dp = cJSON_GetObjectItem(item_val, "disk_picker"); item->disk_picker = dp && dp->valueint;
                cJSON *vi = cJSON_GetObjectItem(item_val, "visible_if"); item->visible_if.keys = NULL; item->visible_if.vals = NULL; item->visible_if.count = 0;
                if (vi && vi->type == cJSON_Object) { item->visible_if.count = cJSON_GetArraySize(vi); item->visible_if.keys = malloc(item->visible_if.count * sizeof(char *)); item->visible_if.vals = malloc(item->visible_if.count * sizeof(char *)); cJSON *child = vi->child; int k = 0; while (child) { item->visible_if.keys[k] = strdup(child->string); item->visible_if.vals[k] = child->valuestring ? strdup(child->valuestring) : strdup(""); k++; child = child->next; } }
                hub_set(&data, item->id, item->value);
                ii++;
            }
            ci++;
        }
    }
    cJSON *acts = cJSON_GetObjectItem(req->params, "actions");
    if (acts && acts->type == cJSON_Array) { data.action_count = cJSON_GetArraySize(acts); data.actions = malloc(data.action_count * sizeof(char *)); for (int i = 0; i < data.action_count; i++) data.actions[i] = strdup(cJSON_GetArrayItem(acts, i)->valuestring); }
    widget_base_init(w, &data, sizeof(HubData), hub_render, hub_handle_event, hub_destroy);
    return w;
}