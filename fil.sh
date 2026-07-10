#!/usr/bin/env bash
set -Eeuo pipefail

FILLY_BIN="${FILLY_BIN:-./target/release/filly}"

_filly_oneshot() {
    local tmp
    tmp=$(mktemp)
    printf '%s\n' "$1" > "$tmp"
    local result
    result=$($FILLY_BIN oneshot --input "$tmp" 2>/dev/null) || true
    rm -f "$tmp"
    printf '%s\n' "$result"
}

_filly_result() {
    local resp
    resp=$(_filly_oneshot "$1")
    [[ -z "$resp" ]] && return 1
    [[ "$(jq -r '.cancelled' <<< "$resp")" == "true" ]] && return 1
    jq -r '.result // empty' <<< "$resp"
}

filly_menu() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"menu","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}'
}

filly_yesno() {
    local title="$1" message="$2"
    _filly_result '{"widget":"yesno","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}'
}

filly_input() {
    local title="$1" message="$2" default="${3:-}"
    _filly_result '{"widget":"input","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","default":"'"${default//\"/\\\"}"'"}}'
}

filly_password() {
    local title="$1" message="$2"
    _filly_result '{"widget":"password","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}'
}

filly_checklist() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"checklist","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}'
}

filly_msg() {
    local title="$1" message="$2"
    _filly_oneshot '{"widget":"msg","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'"}}' >/dev/null
}

filly_progress() {
    local title="$1"
    shift
    local cmd_json
    cmd_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"progress","params":{"title":"'"${title//\"/\\\"}"'","command":'"$cmd_json"'}}'
}

filly_file_picker() {
    local title="$1" start_dir="${2:-/}" filter="${3:-}"
    _filly_result '{"widget":"file_picker","params":{"title":"'"${title//\"/\\\"}"'","start_dir":"'"${start_dir}"'","filter":"'"${filter}"'"}}'
}

filly_filter() {
    local title="$1" message="$2" placeholder="${3:-}"
    shift 3
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"filter","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","placeholder":"'"${placeholder}"'","choices":'"$choices_json"'}}'
}

filly_multiselect() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"multiselect","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}'
}

filly_summary() {
    local title="$1" file="${2:-}" message="${3:-}"
    local params='"title":"'"${title//\"/\\\"}"'"'
    [[ -n "$file" ]] && params+=',"file":"'"${file}"'"'
    [[ -n "$message" ]] && params+=',"message":"'"${message//\"/\\\"}"'"'
    _filly_oneshot '{"widget":"summary","params":{'"$params"'}}' >/dev/null
}

filly_text_editor() {
    local title="$1" file="${2:-}" content="${3:-}"
    local params='"title":"'"${title//\"/\\\"}"'"'
    [[ -n "$file" ]] && params+=',"file":"'"${file}"'"'
    [[ -n "$content" ]] && params+=',"content":"'"${content//\"/\\\"}"'"'
    _filly_result '{"widget":"text","params":{'"$params"'}}'
}

filly_hub() {
    local title="$1" categories_json="$2" actions_json="$3"
    _filly_result '{"widget":"hub","params":{"title":"'"${title//\"/\\\"}"'","categories":'"$categories_json"',"actions":'"$actions_json"'}}'
}

filly_toggle() {
    local title="$1" label="$2" default="${3:-false}"
    _filly_result '{"widget":"toggle","params":{"title":"'"${title//\"/\\\"}"'","label":"'"${label//\"/\\\"}"'","default":'"$default"'}}'
}

filly_spinner() {
    local message="$1"
    _filly_oneshot '{"widget":"spinner","params":{"message":"'"${message//\"/\\\"}"'"}}' >/dev/null
}

filly_separator() {
    local orientation="${1:-horizontal}"
    _filly_oneshot '{"widget":"separator","params":{"orientation":"'"${orientation}"'"}}' >/dev/null
}

filly_table() {
    local title="$1"
    shift
    local headers_json rows_json
    headers_json=$(printf '%s\n' "$1" | jq -R . | jq -s .); shift
    rows_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"table","params":{"title":"'"${title//\"/\\\"}"'","headers":'"$headers_json"',"rows":'"$rows_json"'}}'
}

filly_tree() {
    local title="$1" nodes_json="$2"
    _filly_result '{"widget":"tree","params":{"title":"'"${title//\"/\\\"}"'","nodes":'"$nodes_json"'}}'
}

filly_gauge() {
    local title="$1" percent="$2" label="$3"
    _filly_result '{"widget":"gauge","params":{"title":"'"${title//\"/\\\"}"'","percent":'"$percent"',"label":"'"${label//\"/\\\"}"'"}}'
}

filly_calendar() {
    local title="$1"
    _filly_result '{"widget":"calendar","params":{"title":"'"${title//\"/\\\"}"'"}}'
}

filly_form() {
    local title="$1" fields_json="$2" submit_label="${3:-Submit}"
    _filly_result '{"widget":"form","params":{"title":"'"${title//\"/\\\"}"'","fields":'"$fields_json"',"submit_label":"'"${submit_label//\"/\\\"}"'"}}'
}

filly_tabs() {
    local title="$1"
    shift
    local tabs_json
    tabs_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_oneshot '{"widget":"tabs","params":{"title":"'"${title//\"/\\\"}"'","tabs":'"$tabs_json"'}}' >/dev/null
}

filly_split_panes() {
    local orientation="${1:-horizontal}"
    _filly_oneshot '{"widget":"split_panes","params":{"orientation":"'"${orientation}"'"}}' >/dev/null
}

filly_context_menu() {
    local title="$1"
    shift
    local items_json
    items_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"context_menu","params":{"items":'"$items_json"'}}'
}

filly_notification() {
    local message="$1" duration="${2:-3}"
    _filly_oneshot '{"widget":"notification","params":{"message":"'"${message//\"/\\\"}"'","duration":'"$duration"'}}' >/dev/null
}

filly_radio_group() {
    local title="$1" message="$2"
    shift 2
    local choices_json
    choices_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"radio_group","params":{"title":"'"${title//\"/\\\"}"'","message":"'"${message//\"/\\\"}"'","choices":'"$choices_json"'}}'
}

filly_range_slider() {
    local title="$1" min="$2" max="$3" value="$4" label="$5"
    _filly_result '{"widget":"range_slider","params":{"title":"'"${title//\"/\\\"}"'","min":'"$min"',"max":'"$max"',"value":'"$value"',"label":"'"${label//\"/\\\"}"'"}}'
}

filly_color_picker() {
    local title="$1"
    shift
    local colors_json
    colors_json=$(printf '%s\n' "$@" | jq -R . | jq -s .)
    _filly_result '{"widget":"color_picker","params":{"title":"'"${title//\"/\\\"}"'","colors":'"$colors_json"'}}'
}

filly_badge() {
    local text="$1"
    _filly_oneshot '{"widget":"badge","params":{"text":"'"${text//\"/\\\"}"'"}}' >/dev/null
}

filly_rich_text() {
    local content="$1"
    _filly_oneshot '{"widget":"rich_text","params":{"content":"'"${content//\"/\\\"}"'"}}' >/dev/null
}

filly_tooltip() {
    local text="$1"
    _filly_oneshot '{"widget":"tooltip","params":{"text":"'"${text//\"/\\\"}"'"}}' >/dev/null
}