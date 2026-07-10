#!/usr/bin/env bash
set -Eeuo pipefail

# FILLY graphical backend launcher
# Uses the same JSON protocol as filly-terminal.
# Requires: python, pygobject, gtk4, libadwaita

FILLY_GRAPHICAL="${FILLY_GRAPHICAL:-filly-graphical}"

filly_graphical_menu() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"menu","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}' | jq -r '.result // empty'
}

filly_graphical_yesno() {
    local title="$1" message="$2"
    "$FILLY_GRAPHICAL" <<< '{"widget":"yesno","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_input() {
    local title="$1" message="$2" default="${3:-}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"input","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","default":"'"${default//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_password() {
    local title="$1" message="$2"
    "$FILLY_GRAPHICAL" <<< '{"widget":"password","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_checklist() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"checklist","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}' | jq -r '.result // empty'
}

filly_graphical_msg() {
    local title="$1" message="$2"
    "$FILLY_GRAPHICAL" <<< '{"widget":"msg","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}' >/dev/null
}

filly_graphical_progress() {
    local title="$1"
    shift
    local cmd_json
    cmd_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"progress","params":{"title":"'"${title//\"/\\\"}"'","command":'"$cmd_json"'}}' | jq -r '.result // empty'
}

filly_graphical_file_picker() {
    local title="$1" start_dir="${2:-/}" filter="${3:-}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"file_picker","params":{"title":"'"${title//\"/\\\"}"'","start_dir":"'"${start_dir}"'","filter":"'"${filter}"'"}}' | jq -r '.result // empty'
}

filly_graphical_filter() {
    local title="$1" message="$2" placeholder="${3:-}"
    shift 3
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"filter","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","placeholder":"'"${placeholder}"'","choices":'"$choices_json"'}}' | jq -r '.result // empty'
}

filly_graphical_multiselect() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"multiselect","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}' | jq -r '.result // empty'
}

filly_graphical_summary() {
    local title="$1" file="${2:-}" message="${3:-}"
    local params='"title":"'"${title//\"/\\\"}"'"'
    [[ -n "$file" ]] && params+=',"file":"'"${file}"'"'
    [[ -n "$message" ]] && params+=',"message":"'"${message//\"/\\\"}"'"'
    "$FILLY_GRAPHICAL" <<< '{"widget":"summary","params":{'"$params"'}}' >/dev/null
}

filly_graphical_text_editor() {
    local title="$1" file="${2:-}" content="${3:-}"
    local params='"title":"'"${title//\"/\\\"}"'"'
    [[ -n "$file" ]] && params+=',"file":"'"${file}"'"'
    [[ -n "$content" ]] && params+=',"content":"'"${content//\"/\\\"}"'"'
    "$FILLY_GRAPHICAL" <<< '{"widget":"text","params":{'"$params"'}}' | jq -r '.result // empty'
}

filly_graphical_hub() {
    local title="$1" categories_json="$2" actions_json="$3"
    "$FILLY_GRAPHICAL" <<< '{"widget":"hub","params":{"title":"'"${title//\"/\\\"}"'","categories":'"$categories_json"',"actions":'"$actions_json"'}}' | jq -r '.result // empty'
}

filly_graphical_toggle() {
    local title="$1" label="$2" default="${3:-false}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"toggle","params":{"title":"'"${title//\"/\\\"}"'","label":"'"${label//\"/\\\"}"'","default":'"$default"'}}' | jq -r '.result // empty'
}

filly_graphical_spinner() {
    local message="$1"
    "$FILLY_GRAPHICAL" <<< '{"widget":"spinner","params":{"message":"'"${message//\"/\\\"}"'"}}' >/dev/null
}

filly_graphical_table() {
    local title="$1"
    shift
    local headers_json rows_json
    headers_json=$(printf '%s\n' "$1" | jq -R . | jq -s .); shift
    rows_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"table","params":{"title":"'"${title//\"/\\\"}"'","headers":'"$headers_json"',"rows":'"$rows_json"'}}' | jq -r '.result // empty'
}

filly_graphical_tree() {
    local title="$1" nodes_json="$2"
    "$FILLY_GRAPHICAL" <<< '{"widget":"tree","params":{"title":"'"${title//\"/\\\"}"'","nodes":'"$nodes_json"'}}' | jq -r '.result // empty'
}

filly_graphical_gauge() {
    local title="$1" percent="$2" label="$3"
    "$FILLY_GRAPHICAL" <<< '{"widget":"gauge","params":{"title":"'"${title//\"/\\\"}"'","percent":'"$percent"',"label":"'"${label//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_calendar() {
    local title="$1"
    "$FILLY_GRAPHICAL" <<< '{"widget":"calendar","params":{"title":"'"${title//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_form() {
    local title="$1" fields_json="$2" submit_label="${3:-Submit}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"form","params":{"title":"'"${title//\"/\\\"}"'","fields":'"$fields_json"',"submit_label":"'"${submit_label//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_tabs() {
    local title="$1"
    shift
    local tabs_json
    tabs_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"tabs","params":{"title":"'"${title//\"/\\\"}"'","tabs":'"$tabs_json"'}}' >/dev/null
}

filly_graphical_split_panes() {
    local orientation="${1:-horizontal}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"split_panes","params":{"orientation":"'"${orientation}"'"}}' >/dev/null
}

filly_graphical_context_menu() {
    local title="$1"
    shift
    local items_json
    items_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"context_menu","params":{"items":'"$items_json"'}}' | jq -r '.result // empty'
}

filly_graphical_notification() {
    local message="$1" duration="${2:-3}"
    "$FILLY_GRAPHICAL" <<< '{"widget":"notification","params":{"message":"'"${message//\"/\\\"}"'","duration":'"$duration"'}}' >/dev/null
}

filly_graphical_radio_group() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"radio_group","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}' | jq -r '.result // empty'
}

filly_graphical_range_slider() {
    local title="$1" min="$2" max="$3" value="$4" label="$5"
    "$FILLY_GRAPHICAL" <<< '{"widget":"range_slider","params":{"title":"'"${title//\"/\\\"}"'","min":'"$min"',"max":'"$max"',"value":'"$value"',"label":"'"${label//\"/\\\"}"'"}}' | jq -r '.result // empty'
}

filly_graphical_color_picker() {
    local title="$1"
    shift
    local colors_json
    colors_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    "$FILLY_GRAPHICAL" <<< '{"widget":"color_picker","params":{"title":"'"${title//\"/\\\"}"'","colors":'"$colors_json"'}}' | jq -r '.result // empty'
}

filly_graphical_badge() {
    local text="$1"
    "$FILLY_GRAPHICAL" <<< '{"widget":"badge","params":{"text":"'"${text//\"/\\\"}"'"}}' >/dev/null
}

filly_graphical_rich_text() {
    local content="$1"
    "$FILLY_GRAPHICAL" <<< '{"widget":"rich_text","params":{"content":"'"${content//\"/\\\"}"'"}}' >/dev/null
}

filly_graphical_tooltip() {
    local text="$1"
    "$FILLY_GRAPHICAL" <<< '{"widget":"tooltip","params":{"text":"'"${text//\"/\\\"}"'"}}' >/dev/null
}