#!/usr/bin/env bash
set -Euo pipefail

FILLY="./filly"
PASS=0
FAIL=0
TOTAL=0

TEST_DIR="/tmp/filly-tests"
mkdir -p "${TEST_DIR}"

GREEN='\e[32m'
RED='\e[31m'
CYAN='\e[1;36m'
YELLOW='\e[1;33m'
NC='\e[0m'

assert_response() {
    local name="$1" response="$2" check="$3"
    TOTAL=$((TOTAL + 1))
    if echo "${response}" | jq -e "${check}" >/dev/null 2>&1; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"
        PASS=$((PASS + 1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"
        printf '       Expected: %s\n' "${check}"
        printf '       Got:      %s\n' "${response}"
        FAIL=$((FAIL + 1))
    fi
}

assert_eq() {
    local name="$1" response="$2" expected="$3"
    TOTAL=$((TOTAL + 1))
    local actual
    actual=$(echo "${response}" | jq -c '.result')
    if [[ "${actual}" == "${expected}" ]]; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"
        PASS=$((PASS + 1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"
        printf '       Expected: %s\n' "${expected}"
        printf '       Got:      %s\n' "${actual}"
        FAIL=$((FAIL + 1))
    fi
}

run_headless() {
    local widget="$1" params="$2" events="${3:-}"
    local json
    json=$(printf '{"widget":"%s","params":%s}' "${widget}" "${params}")
    local events_file="${TEST_DIR}/events.txt"
    if [[ -n "${events}" ]]; then
        printf '%s\n' "${events}" > "${events_file}"
    else
        printf 'KEY:ESC\n' > "${events_file}"
    fi
    echo "${json}" | "${FILLY}" oneshot --headless --insecure-plugins --events "${events_file}" 2>/dev/null || true
}

printf "${CYAN}=== FILLY Comprehensive Behavioral Test Suite ===${NC}\n\n"

printf "${YELLOW}--- Menu Widget ---${NC}\n"
resp=$(run_headless "menu" '{"title":"Menu","choices":["Alpha","Beta","Gamma"]}' "KEY:ENTER")
assert_eq "menu selects first by default" "${resp}" '"Alpha"'
resp=$(run_headless "menu" '{"title":"Menu","choices":["Alpha","Beta","Gamma"],"default":"Beta"}' "KEY:ENTER")
assert_eq "menu respects default index" "${resp}" '"Beta"'
resp=$(run_headless "menu" '{"title":"Menu","choices":["Alpha","Beta","Gamma"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "menu selects third item" "${resp}" '"Gamma"'
resp=$(run_headless "menu" '{"title":"Menu","choices":["Alpha","Beta","Gamma"]}' $'KEY:UP\nKEY:ENTER')
assert_eq "menu stays at first on UP from top" "${resp}" '"Alpha"'
resp=$(run_headless "menu" '{"title":"Menu","choices":["Alpha","Beta","Gamma"]}' "KEY:ESC")
assert_response "menu cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- YesNo Widget ---${NC}\n"
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":true}' "KEY:ENTER")
assert_eq "yesno confirms default yes" "${resp}" 'true'
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":false}' "KEY:ENTER")
assert_eq "yesno confirms default no" "${resp}" 'false'
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":false}' $'KEY:LEFT\nKEY:ENTER')
assert_eq "yesno switches to yes with left" "${resp}" 'true'
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":true}' $'TEXT:y')
assert_eq "yesno quick key y" "${resp}" 'true'
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":true}' $'TEXT:n')
assert_eq "yesno quick key n" "${resp}" 'false'
resp=$(run_headless "yesno" '{"title":"Confirm","message":"Proceed?","default":true}' "KEY:ESC")
assert_response "yesno cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Input Widget ---${NC}\n"
resp=$(run_headless "input" '{"title":"Name","default":"Artix"}' "KEY:ENTER")
assert_eq "input returns default text" "${resp}" '"Artix"'
resp=$(run_headless "input" '{"title":"Name","placeholder":"enter name"}' $'TEXT:hello\nKEY:ENTER')
assert_eq "input captures typed text" "${resp}" '"hello"'
resp=$(run_headless "input" '{"title":"Name","default":"hello"}' $'KEY:BACKSPACE\nKEY:BACKSPACE\nKEY:ENTER')
assert_eq "input backspace removes chars" "${resp}" '"hel"'
resp=$(run_headless "input" '{"title":"Name","default":"abc"}' $'KEY:HOME\nTEXT:X\nKEY:ENTER')
assert_eq "input HOME and insert at start" "${resp}" '"Xabc"'
resp=$(run_headless "input" '{"title":"Name","default":"abc"}' $'KEY:END\nTEXT:X\nKEY:ENTER')
assert_eq "input END and append" "${resp}" '"abcX"'
resp=$(run_headless "input" '{"title":"Name","validation":"^[0-9]+$","default":"abc"}' "KEY:ENTER")
assert_response "input rejects invalid validation" "${resp}" '.cancelled == true'
resp=$(run_headless "input" '{"title":"Name","validation":"^[0-9]+$","default":"123"}' "KEY:ENTER")
assert_eq "input accepts valid validation" "${resp}" '"123"'
resp=$(run_headless "input" '{"title":"Name"}' "KEY:ESC")
assert_response "input cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Password Widget ---${NC}\n"
resp=$(run_headless "password" '{"title":"Secret"}' $'TEXT:mypass\nKEY:ENTER')
assert_eq "password captures input" "${resp}" '"mypass"'
resp=$(run_headless "password" '{"title":"Secret","placeholder":"hint"}' $'TEXT:abc\nKEY:BACKSPACE\nKEY:ENTER')
assert_eq "password backspace works" "${resp}" '"ab"'
resp=$(run_headless "password" '{"title":"Secret"}' "KEY:ESC")
assert_response "password cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Checklist Widget ---${NC}\n"
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim","gcc"]}' "KEY:ENTER")
assert_eq "checklist empty submit returns empty array" "${resp}" '[]'
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim","gcc"],"default":["git"]}' "KEY:ENTER")
assert_eq "checklist respects defaults" "${resp}" '["git"]'
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim","gcc"]}' $'KEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER')
assert_eq "checklist toggles two items" "${resp}" '["git","vim"]'
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim","gcc"],"min":2,"max":2}' $'KEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER')
assert_eq "checklist min/max satisfied" "${resp}" '["git","vim"]'
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim"],"min":1}' "KEY:ENTER")
assert_response "checklist rejects below min" "${resp}" '.cancelled == true'
resp=$(run_headless "checklist" '{"title":"Packages","choices":["git","vim"]}' "KEY:ESC")
assert_response "checklist cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Filter Widget ---${NC}\n"
resp=$(run_headless "filter" '{"title":"TZ","choices":["London","Paris","Tokyo"]}' $'TEXT:Lon\nKEY:ENTER')
assert_eq "filter narrows and selects" "${resp}" '"London"'
resp=$(run_headless "filter" '{"title":"TZ","choices":["London","Paris","Tokyo"]}' $'TEXT:Par\nKEY:ENTER')
assert_eq "filter selects Paris" "${resp}" '"Paris"'
resp=$(run_headless "filter" '{"title":"TZ","choices":["London","Paris","Tokyo"]}' $'TEXT:xyz\nKEY:ENTER')
assert_response "filter no match stays" "${resp}" '.cancelled == true'
resp=$(run_headless "filter" '{"title":"TZ","choices":["London","Paris","Tokyo"]}' $'TEXT:Lon\nKEY:BACKSPACE\nKEY:BACKSPACE\nKEY:BACKSPACE\nKEY:DOWN\nKEY:ENTER')
assert_eq "filter clears and selects Paris" "${resp}" '"Paris"'
resp=$(run_headless "filter" '{"title":"TZ","choices":["London","Paris","Tokyo"]}' "KEY:ESC")
assert_response "filter cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Multiselect Widget ---${NC}\n"
resp=$(run_headless "multiselect" '{"title":"Pkgs","choices":["a","b","c"]}' "KEY:ENTER")
assert_eq "multiselect empty submit" "${resp}" '[]'
resp=$(run_headless "multiselect" '{"title":"Pkgs","choices":["a","b","c"]}' $'KEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER')
assert_eq "multiselect toggles two" "${resp}" '["a","b"]'
resp=$(run_headless "multiselect" '{"title":"Pkgs","choices":["a","b","c"]}' $'TEXT:b\nKEY:SPACE\nKEY:ENTER')
assert_eq "multiselect filter and toggle" "${resp}" '["b"]'
resp=$(run_headless "multiselect" '{"title":"Pkgs","choices":["a","b","c"]}' "KEY:ESC")
assert_response "multiselect cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Toggle Widget ---${NC}\n"
resp=$(run_headless "toggle" '{"title":"Enable","label":"Feature","default":false}' "KEY:ENTER")
assert_eq "toggle default off confirms false" "${resp}" 'false'
resp=$(run_headless "toggle" '{"title":"Enable","label":"Feature","default":true}' "KEY:ENTER")
assert_eq "toggle default on confirms true" "${resp}" 'true'
resp=$(run_headless "toggle" '{"title":"Enable","label":"Feature","default":false}' $'KEY:SPACE\nKEY:ENTER')
assert_eq "toggle space toggles on" "${resp}" 'true'
resp=$(run_headless "toggle" '{"title":"Enable","label":"Feature","default":true}' $'KEY:SPACE\nKEY:ENTER')
assert_eq "toggle space toggles off" "${resp}" 'false'
resp=$(run_headless "toggle" '{"title":"Enable","label":"Feature","default":false}' "KEY:ESC")
assert_response "toggle cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Radio Group Widget ---${NC}\n"
resp=$(run_headless "radio_group" '{"title":"Pick","choices":["A","B","C"],"default":0}' "KEY:ENTER")
assert_eq "radio selects default" "${resp}" '"A"'
resp=$(run_headless "radio_group" '{"title":"Pick","choices":["A","B","C"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "radio selects third" "${resp}" '"C"'
resp=$(run_headless "radio_group" '{"title":"Pick","choices":["A","B","C"]}' $'KEY:UP\nKEY:ENTER')
assert_eq "radio UP from top stays" "${resp}" '"A"'
resp=$(run_headless "radio_group" '{"title":"Pick","choices":["A","B","C"]}' "KEY:ESC")
assert_response "radio cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Range Slider Widget ---${NC}\n"
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":50}' "KEY:ENTER")
assert_eq "range slider returns initial value" "${resp}" '50'
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":50}' $'KEY:RIGHT\nKEY:RIGHT\nKEY:RIGHT\nKEY:ENTER')
assert_eq "range slider increments" "${resp}" '53'
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":50}' $'KEY:LEFT\nKEY:LEFT\nKEY:ENTER')
assert_eq "range slider decrements" "${resp}" '48'
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":0}' $'KEY:LEFT\nKEY:ENTER')
assert_eq "range slider clamps at min" "${resp}" '0'
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":100}' $'KEY:RIGHT\nKEY:ENTER')
assert_eq "range slider clamps at max" "${resp}" '100'
resp=$(run_headless "range_slider" '{"title":"Volume","min":0,"max":100,"value":50}' "KEY:ESC")
assert_response "range slider cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Form Widget ---${NC}\n"
resp=$(run_headless "form" '{"title":"User","fields":[{"label":"Name","widget_type":"input","value":""},{"label":"Age","widget_type":"input","value":""}],"submit_label":"OK"}' $'TEXT:John\nKEY:TAB\nTEXT:30\nKEY:TAB\nKEY:ENTER')
assert_eq "form submits two fields" "${resp}" '{"Name":"John","Age":"30"}'
resp=$(run_headless "form" '{"title":"User","fields":[{"label":"Name","widget_type":"input","value":"default"}]}' "KEY:ENTER")
assert_response "form does not submit on field ENTER" "${resp}" '.result == null'
resp=$(run_headless "form" '{"title":"User","fields":[{"label":"Name","widget_type":"input","value":""}]}' "KEY:ESC")
assert_response "form cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Calendar Widget ---${NC}\n"
resp=$(run_headless "calendar" '{"title":"Date"}' "KEY:ENTER")
assert_response "calendar returns ISO date" "${resp}" '.result | test("^[0-9]{4}-[0-9]{2}-[0-9]{2}$")'
resp=$(run_headless "calendar" '{"title":"Date"}' $'KEY:LEFT\nKEY:ENTER')
assert_response "calendar previous day returns date" "${resp}" '.result | test("^[0-9]{4}-[0-9]{2}-[0-9]{2}$")'
resp=$(run_headless "calendar" '{"title":"Date"}' "KEY:ESC")
assert_response "calendar cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Table Widget ---${NC}\n"
resp=$(run_headless "table" '{"title":"Tbl","headers":["Name","Size"],"rows":[["a","1"],["b","2"]]}' "KEY:ENTER")
assert_eq "table selects first cell" "${resp}" '"a"'
resp=$(run_headless "table" '{"title":"Tbl","headers":["Name","Size"],"rows":[["a","1"],["b","2"]]}' $'KEY:DOWN\nKEY:RIGHT\nKEY:ENTER')
assert_eq "table selects cell by navigation" "${resp}" '"2"'
resp=$(run_headless "table" '{"title":"Tbl","headers":["Name","Size"],"rows":[["a","1"]]}' "KEY:ESC")
assert_response "table cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Tree Widget ---${NC}\n"
resp=$(run_headless "tree" '{"title":"Files"}' $'KEY:SPACE\nKEY:ESC')
assert_response "tree expands and dismisses" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Context Menu Widget ---${NC}\n"
resp=$(run_headless "context_menu" '{"items":["Copy","Paste","Delete"]}' "KEY:ENTER")
assert_eq "context menu selects first" "${resp}" '"Copy"'
resp=$(run_headless "context_menu" '{"items":["Copy","Paste","Delete"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "context menu selects third" "${resp}" '"Delete"'
resp=$(run_headless "context_menu" '{"items":["Copy","Paste","Delete"]}' "KEY:ESC")
assert_response "context menu cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Hub Widget ---${NC}\n"
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat","items":[{"id":"k","label":"Key","value":"v","widget":"input"}]}],"actions":["Proceed"]}' $'KEY:ENTER\nTEXT:newval\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "hub inline edit and proceed" "${resp}" '{"k":"vnewval"}'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat1","items":[{"id":"a","label":"A","value":"1","widget":"input"}]},{"label":"Cat2","items":[{"id":"b","label":"B","value":"2","widget":"input"}]}],"actions":["Proceed"]}' $'KEY:RIGHT\nKEY:ENTER\nTEXT:changed\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "hub cross-category edit" "${resp}" '{"a":"1","b":"2changed"}'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat","items":[{"id":"k","label":"Key","value":"yes","widget":"yesno"}]}],"actions":["Proceed"]}' $'KEY:ENTER\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "hub yesno inline edit confirms" "${resp}" '{"k":"no"}'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat","items":[{"id":"k","label":"Key","value":"opt1","widget":"menu","choices":["opt1","opt2","opt3"]}]}],"actions":["Proceed"]}' $'KEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "hub menu inline edit" "${resp}" '{"k":"opt2"}'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Empty","items":[]}],"actions":["Proceed"]}' $'KEY:F1\nTEXT:y')
assert_eq "hub proceed with empty items" "${resp}" '{}'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat","items":[{"id":"k","label":"Key","value":"v","widget":"input"}]}],"actions":["Proceed"]}' $'KEY:ESC\nTEXT:n')
assert_response "hub quit confirmation no" "${resp}" '.cancelled == false'
resp=$(run_headless "hub" '{"title":"H","categories":[{"label":"Cat","items":[{"id":"k","label":"Key","value":"v","widget":"input"}]}],"actions":["Proceed"]}' $'KEY:ESC\nTEXT:y')
assert_response "hub quit confirmation yes" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Display Widgets ---${NC}\n"
resp=$(run_headless "msg" '{"title":"Msg","message":"Hello"}' "KEY:ENTER")
assert_response "msg dismisses on key" "${resp}" '.cancelled == false'
resp=$(run_headless "badge" '{"text":"NEW"}' "KEY:ENTER")
assert_response "badge dismisses on key" "${resp}" '.cancelled == false'
resp=$(run_headless "notification" '{"message":"Alert","duration":10}' "KEY:ENTER")
assert_response "notification dismisses on key" "${resp}" '.cancelled == false'
resp=$(run_headless "gauge" '{"title":"Gauge","percent":75,"label":"Loading"}' "KEY:ENTER")
assert_response "gauge dismisses on key" "${resp}" '.cancelled == false'
resp=$(run_headless "spinner" '{"message":"Wait..."}' "KEY:ESC")
assert_response "spinner cancels on ESC" "${resp}" '.cancelled == true'
resp=$(run_headless "summary" '{"title":"Summary","message":"Line1\nLine2"}' "KEY:ENTER")
assert_response "summary dismisses" "${resp}" '.cancelled == false'
resp=$(run_headless "separator" '{"orientation":"horizontal"}' "KEY:ENTER")
assert_response "separator dismisses" "${resp}" '.cancelled == false'
resp=$(run_headless "tooltip" '{"text":"Hint"}' "KEY:ENTER")
assert_response "tooltip dismisses on key" "${resp}" '.cancelled == false'
resp=$(run_headless "rich_text" '{"content":"**bold**"}' "KEY:ENTER")
assert_response "rich_text dismisses on key" "${resp}" '.cancelled == false'

printf "${YELLOW}--- File Picker Widget ---${NC}\n"
resp=$(run_headless "file_picker" '{"title":"Files","start_dir":"/"}' "KEY:ESC")
assert_response "file_picker cancels" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Text Editor Widget ---${NC}\n"
resp=$(run_headless "text" '{"title":"Edit","content":"hello"}' "KEY:ESC")
assert_eq "text_editor returns content on ESC" "${resp}" '"hello\n"'
resp=$(run_headless "text" '{"title":"Edit","content":"line1\nline2"}' $'KEY:ENTER\nTEXT:new\nKEY:ESC')
assert_response "text_editor insert and return" "${resp}" '.result | contains("line1")'
resp=$(run_headless "text" '{"title":"Edit","content":"hello"}' $'KEY:BACKSPACE\nKEY:BACKSPACE\nKEY:ESC')
assert_eq "text_editor backspace" "${resp}" '"hel\n"'

printf "${YELLOW}--- Disk Widget ---${NC}\n"
resp=$(run_headless "disk" '{"title":"Disk","disk":"/dev/sda","partitions":[],"free_space":[],"readonly":true}' "KEY:ESC")
assert_response "disk read-only cancels" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Color Picker Widget ---${NC}\n"
resp=$(run_headless "color_picker" '{"title":"Color","colors":[]}' "KEY:ENTER")
assert_response "color_picker returns hex on ENTER" "${resp}" '.result | test("^#[0-9A-F]{6}$")'
resp=$(run_headless "color_picker" '{"title":"Color","colors":[]}' $'KEY:RIGHT\nKEY:RIGHT\nKEY:RIGHT\nKEY:ENTER')
assert_response "color_picker adjusts red and returns" "${resp}" '.result | test("^#[0-9A-F]{6}$")'
resp=$(run_headless "color_picker" '{"title":"Color","colors":[]}' "KEY:ESC")
assert_response "color_picker cancels on ESC" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Progress Widget ---${NC}\n"
resp=$(run_headless "progress" '{"title":"Progress","command":["echo","task done"]}' "KEY:ENTER")
assert_response "progress runs command and returns output" "${resp}" '.cancelled == false'
resp=$(run_headless "progress" '{"title":"Progress","command":["sleep","10"]}' "KEY:ESC")
assert_response "progress cancels long command" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Tabs Widget ---${NC}\n"
resp=$(run_headless "tabs" '{"title":"Tabs","tabs":[{"label":"Tab1"},{"label":"Tab2"}]}' $'KEY:RIGHT\nKEY:ESC')
assert_response "tabs switch and dismiss" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Split Panes Widget ---${NC}\n"
resp=$(run_headless "split_panes" '{"title":"Split"}' $'KEY:F2\nKEY:ESC')
assert_response "split_panes switch pane and dismiss" "${resp}" '.cancelled == false'

printf "${YELLOW}--- ArtixForge Plugin Widgets ---${NC}\n"

resp=$(run_headless "install_hub" '{"title":"Artix Configuration","categories":[{"label":"Disk","items":[{"id":"DISK","label":"Target disk","value":"/dev/sda - 256G","widget":"menu","choices":["/dev/sda - 256G","/dev/nvme0n1 - 512G"]},{"id":"FS_TYPE","label":"Filesystem","value":"ext4","widget":"menu","choices":["ext4","btrfs","xfs","f2fs"]}]}],"actions":["Proceed"],"boot_mode":"uefi"}' $'KEY:F1\nTEXT:y')
assert_eq "install_hub proceed with defaults" "${resp}" '{"DISK":"/dev/sda - 256G","FS_TYPE":"ext4"}'

resp=$(run_headless "install_hub" '{"title":"Disk Setup","categories":[{"label":"Storage","items":[{"id":"FS_TYPE","label":"Filesystem","value":"ext4","widget":"menu","choices":["ext4","btrfs"]},{"id":"USE_LUKS","label":"LUKS Encryption","value":"no","widget":"yesno","visible_if":{"FS_TYPE":"btrfs"}},{"id":"USE_LVM","label":"LVM","value":"no","widget":"yesno","visible_if":{"FS_TYPE":"btrfs"}}]}],"actions":["Proceed"],"boot_mode":"uefi"}' $'KEY:F1\nTEXT:y')
assert_eq "install_hub visibility hides LUKS+LVM when FS=ext4" "${resp}" '{"FS_TYPE":"ext4","USE_LUKS":"no","USE_LVM":"no"}'

resp=$(run_headless "install_hub" '{"title":"Disk Setup","categories":[{"label":"Storage","items":[{"id":"FS_TYPE","label":"Filesystem","value":"ext4","widget":"menu","choices":["ext4","btrfs"]},{"id":"USE_LUKS","label":"LUKS Encryption","value":"no","widget":"yesno","visible_if":{"FS_TYPE":"btrfs"}},{"id":"USE_LVM","label":"LVM","value":"no","widget":"yesno","visible_if":{"FS_TYPE":"btrfs"}}]}],"actions":["Proceed"],"boot_mode":"uefi"}' $'KEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:LEFT\nKEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:LEFT\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "install_hub visibility shows LUKS+LVM when FS=btrfs" "${resp}" '{"FS_TYPE":"btrfs","USE_LUKS":"yes","USE_LVM":"yes"}'

resp=$(run_headless "anvil" '{"title":"System Maintenance","categories":[{"category":"Package Management","actions":[{"key":"pacman-sync","description":"Sync package databases"},{"key":"pacman-upgrade","description":"Upgrade all packages"}]}]}' $'KEY:DOWN\nKEY:ENTER\nTEXT:y')
assert_eq "anvil run package upgrade" "${resp}" '"pacman-upgrade"'

resp=$(run_headless "anvil" '{"title":"System Maintenance","categories":[{"category":"Package Management","actions":[{"key":"pacman-sync","description":"Sync package databases"}]},{"category":"Services","actions":[{"key":"svc-status","description":"Show service status"}]}]}' $'KEY:RIGHT\nKEY:ENTER\nTEXT:y')
assert_eq "anvil switch to Services and run status" "${resp}" '"svc-status"'

resp=$(run_headless "anvil" '{"title":"System Maintenance","categories":[{"category":"Package Management","actions":[{"key":"pacman-sync","description":"Sync package databases"}]}]}' $'KEY:ENTER\nTEXT:n')
assert_response "anvil cancel confirmation with n" "${resp}" '.cancelled == true'

resp=$(run_headless "recovery" '{"title":"System Recovery","status":[{"key":"disk","label":"Disk health","status":"ok"},{"key":"boot","label":"Bootloader integrity","status":"warn"},{"key":"fstab","label":"Fstab entries","status":"fail"},{"key":"pacman","label":"Package database","status":"ok"}],"repairs":[{"key":"fix_boot","label":"Reinstall Bootloader"},{"key":"fix_fstab","label":"Regenerate Fstab"},{"key":"fix_pacman","label":"Rebuild Package DB"},{"key":"fix_all","label":"Repair All Issues"}]}' $'KEY:DOWN\nKEY:DOWN\nKEY:F3\nTEXT:y')
assert_eq "recovery repair third issue with F3" "${resp}" '"fix_pacman"'

resp=$(run_headless "recovery" '{"title":"System Recovery","status":[{"key":"disk","label":"Disk health","status":"ok"},{"key":"boot","label":"Bootloader integrity","status":"fail"}],"repairs":[{"key":"fix_boot","label":"Reinstall Bootloader"},{"key":"fix_all","label":"Repair All Issues"}]}' $'KEY:DOWN\nKEY:F2\nTEXT:n')
assert_response "recovery cancel fix-all with n" "${resp}" '.cancelled == true'

resp=$(run_headless "iso" '{"title":"Artix ISO Builder","categories":[{"label":"ISO Type","items":[{"id":"ISO_MODE","label":"Boot mode","value":"live","widget":"menu","choices":["live","installer"]}]},{"label":"Desktop","items":[{"id":"WM_DE","label":"Desktop Environment","value":"none","widget":"menu","choices":["none","kde","xfce4","hyprland","sway"]},{"id":"DISPLAY_MANAGER","label":"Display Manager","value":"none","widget":"menu","choices":["none","sddm","lightdm"]}]}]}' $'KEY:F1\nTEXT:y')
assert_eq "iso build live ISO with defaults" "${resp}" '{"ISO_MODE":"live","WM_DE":"none","DISPLAY_MANAGER":"none"}'

resp=$(run_headless "iso" '{"title":"Artix ISO Builder","categories":[{"label":"ISO Type","items":[{"id":"ISO_MODE","label":"Boot mode","value":"live","widget":"menu","choices":["live","installer"]}]}]}' $'KEY:F1\nTEXT:n')
assert_response "iso cancel build with n" "${resp}" '.cancelled == true'

resp=$(run_headless "migration_init" '{"title":"Init System Migration","current_init":"openrc"}' $'KEY:DOWN\nKEY:RIGHT\nKEY:ENTER')
assert_eq "migration_init select runit" "${resp}" '{"source":"openrc","target":"runit"}'

resp=$(run_headless "migration_init" '{"title":"Init System Migration","current_init":"openrc"}' $'KEY:DOWN\nKEY:RIGHT\nKEY:RIGHT\nKEY:RIGHT\nKEY:ENTER')
assert_eq "migration_init select s6" "${resp}" '{"source":"openrc","target":"s6"}'

resp=$(run_headless "migration_desktop" '{"title":"Desktop Environment Migration","current_de":"none"}' $'KEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:RIGHT\nKEY:RIGHT\nKEY:DOWN\nKEY:ENTER')
assert_eq "migration_desktop full config" "${resp}" '{"source":"none","target":"kde","dm":"current","x_stack":"xorg","audio":"current","network":"current"}'

resp=$(run_headless "password_confirm" '{"title":"Set Root Password"}' $'TEXT:secret123\nKEY:TAB\nTEXT:secret123\nKEY:ENTER')
assert_response "password_confirm matching passwords" "${resp}" '.result == "SHA512:secret123"'

resp=$(run_headless "password_confirm" '{"title":"Set Root Password"}' $'TEXT:abc\nKEY:TAB\nTEXT:xyz\nKEY:ENTER')
assert_response "password_confirm mismatch stays in form" "${resp}" '.result == null'

resp=$(run_headless "user_manager" '{"title":"User Accounts"}' $'TEXT:a\nTEXT:john\nKEY:TAB\nTEXT:secret\nKEY:TAB\nTEXT:secret\nKEY:ENTER\nKEY:ESC')
assert_response "user_manager add user john" "${resp}" '.result[0].name == "john" and .result[0].sudo == true and .result[0].pass != "SHA512:secret"'

resp=$(run_headless "user_manager" '{"title":"User Accounts"}' "KEY:ESC")
assert_eq "user_manager exits with empty list" "${resp}" '[]'

printf "${YELLOW}--- GForge Plugin Widgets ---${NC}\n"

resp=$(run_headless "gforge_hub" '{"title":"Gentoo Installation","categories":[{"label":"Disk","items":[{"id":"DISK","label":"Target disk","value":"","widget":"input"},{"id":"FS_TYPE","label":"Filesystem","value":"ext4","widget":"menu","choices":["ext4","xfs"]}]},{"label":"Stage3","items":[{"id":"STAGE3","label":"Stage3 variant","value":"openrc","widget":"menu","choices":["openrc","systemd","musl"]}]}],"actions":["Proceed"]}' $'KEY:ENTER\nTEXT:/dev/nvme0n1\nKEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:DOWN\nKEY:ENTER\nKEY:RIGHT\nKEY:ENTER\nKEY:DOWN\nKEY:DOWN\nKEY:ENTER\nKEY:F1\nTEXT:y')
assert_eq "gforge_hub config" "${resp}" '{"DISK":"/dev/nvme0n1","FS_TYPE":"xfs","STAGE3":"musl"}'

resp=$(run_headless "gforge_hub" '{"title":"Gentoo Installation","categories":[{"label":"Empty","items":[]}],"actions":["Proceed"]}' $'KEY:F1\nTEXT:y')
assert_eq "gforge_hub proceed with empty category" "${resp}" '{}'

resp=$(run_headless "stage3_picker" '{"title":"Stage3 Tarball Selection","choices":["openrc","systemd","musl","musl-hardened"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "stage3_picker select musl" "${resp}" '"musl"'

resp=$(run_headless "profile_picker" '{"title":"Portage Profile Selection","choices":["default/linux/amd64/23.0","default/linux/amd64/23.0/musl","default/linux/amd64/23.0/split-usr","default/linux/amd64/23.0/musl/split-usr"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "profile_picker select musl split-usr" "${resp}" '"default/linux/amd64/23.0/musl/split-usr"'

resp=$(run_headless "kernel_picker" '{"title":"Kernel Selection","choices":["gentoo-kernel","gentoo-kernel-bin","gentoo-sources","gentoo-sources-genkernel","linux-cachyos-bore"]}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "kernel_picker select gentoo-sources" "${resp}" '"gentoo-sources"'

resp=$(run_headless "use_flags" '{"title":"Global USE Flags","choices":["X","wayland","pulseaudio","pipewire","lto","pgo"]}' $'KEY:SPACE\nKEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER')
assert_eq "use_flags toggle X pipewire lto pgo" "${resp}" '"X pipewire lto pgo"'

resp=$(run_headless "use_flags" '{"title":"Global USE Flags","choices":["X","wayland"]}' $'KEY:SPACE\nKEY:SPACE\nKEY:ENTER')
assert_eq "use_flags toggle on then off returns empty" "${resp}" '""'

resp=$(run_headless "cflags" '{"title":"Compiler Flags","CFLAGS":"-march=native -O2 -pipe","CXXFLAGS":"-march=native -O2 -pipe","MAKEOPTS":"-j$(nproc)","RUSTFLAGS":"-C target-cpu=native"}' $'KEY:F1')
assert_eq "cflags F1 submit all Gentoo-style" "${resp}" '{"CFLAGS":"-march=native -O2 -pipe","CXXFLAGS":"-march=native -O2 -pipe","MAKEOPTS":"-j$(nproc)","RUSTFLAGS":"-C target-cpu=native"}'

resp=$(run_headless "cflags" '{"title":"Compiler Flags","CFLAGS":"","CXXFLAGS":"","MAKEOPTS":"","RUSTFLAGS":""}' $'KEY:DOWN\nKEY:DOWN\nKEY:ENTER')
assert_eq "cflags navigate to MAKEOPTS and select" "${resp}" '"MAKEOPTS"'

printf "\n${CYAN}=== Results: %d/%d passed ===${NC}\n" "${PASS}" "${TOTAL}"
if [[ ${FAIL} -gt 0 ]]; then
    printf "${RED}%d tests FAILED${NC}\n" "${FAIL}"
    exit 1
else
    printf "${GREEN}All tests passed${NC}\n"
    exit 0
fi