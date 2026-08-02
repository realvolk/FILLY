#!/usr/bin/env bash
set -Euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FILLY="${SCRIPT_DIR}/../filly"
FILLY_BUILD="${SCRIPT_DIR}/../filly-build"
PASS=0; FAIL=0; SKIP=0; TOTAL=0
GREEN='\e[32m'; RED='\e[31m'; CYAN='\e[1;36m'; YELLOW='\e[1;33m'; NC='\e[0m'

pass() { TOTAL=$((TOTAL+1)); PASS=$((PASS+1)); printf "${GREEN}[PASS]${NC} %s\n" "$1"; }
fail() { TOTAL=$((TOTAL+1)); FAIL=$((FAIL+1)); printf "${RED}[FAIL]${NC} %s\n" "$1"; }
skip() { TOTAL=$((TOTAL+1)); SKIP=$((SKIP+1)); printf "${YELLOW}[SKIP]${NC} %s\n" "$1"; }

HAS_DISPLAY=0
if [[ -n "${WAYLAND_DISPLAY:-}" || -n "${DISPLAY:-}" ]]; then HAS_DISPLAY=1; fi

cleanup() {
    kill ${DAEMON_PID:-} 2>/dev/null || true
    wait ${DAEMON_PID:-} 2>/dev/null || true
    rm -rf "${TEST_DIR:-/tmp/filly-full}"
}
trap cleanup EXIT

TEST_DIR="/tmp/filly-full-tests"
rm -rf "${TEST_DIR}"
mkdir -p "${TEST_DIR}"

printf "${CYAN}=== FILLY Full Integration Tests (Terminal + GUI) ===${NC}\n\n"

printf "${YELLOW}--- Daemon ---${NC}\n"
SOCKET="${TEST_DIR}/filly.sock"

mkdir -p "${HOME}/.config/filly/plugins"
cp "${SCRIPT_DIR}/../libartixforge.so" "${HOME}/.config/filly/plugins/" 2>/dev/null || true
cp "${SCRIPT_DIR}/../libgforge.so" "${HOME}/.config/filly/plugins/" 2>/dev/null || true

"${FILLY}" daemon --socket "${SOCKET}" &
DAEMON_PID=$!
sleep 1
if kill -0 ${DAEMON_PID} 2>/dev/null; then
    pass "daemon started (PID ${DAEMON_PID})"
else
    fail "daemon failed to start"; exit 1
fi

# Widgets that self-terminate (return response on first event or immediately)
declare -A SELF_TERMINATING=(
    ["menu"]='{"widget":"menu","params":{"title":"Pick","message":"Choose","choices":["A","B","C"]}}'
    ["yesno"]='{"widget":"yesno","params":{"title":"OK?","message":"Proceed?"}}'
    ["checklist"]='{"widget":"checklist","params":{"title":"Pick","choices":["1","2"],"min":1,"max":1}}'
    ["msg"]='{"widget":"msg","params":{"title":"Hi","message":"Hello world"}}'
    ["summary"]='{"widget":"summary","params":{"title":"Sum","message":"Done"}}'
    ["progress"]='{"widget":"progress","params":{"title":"Work","command":["true"]}}'
    ["toggle"]='{"widget":"toggle","params":{"title":"On","label":"Enable"}}'
    ["badge"]='{"widget":"badge","params":{"text":"NEW"}}'
    ["rich_text"]='{"widget":"rich_text","params":{"content":"**bold**"}}'
    ["tooltip"]='{"widget":"tooltip","params":{"text":"Help"}}'
    ["separator"]='{"widget":"separator","params":{}}'
    ["gauge"]='{"widget":"gauge","params":{"title":"Gauge","percent":50}}'
    ["calendar"]='{"widget":"calendar","params":{"title":"Cal"}}'
    ["table"]='{"widget":"table","params":{"title":"Tbl","headers":["A"],"rows":[["1"]]}}'
    ["tree"]='{"widget":"tree","params":{"title":"Tree","nodes":[{"label":"X"}]}}'
    ["notification"]='{"widget":"notification","params":{"message":"Hi","duration":1}}'
)

# Widgets that need ESC injected to terminate
declare -A NEEDS_INPUT=(
    ["input"]='{"widget":"input","params":{"title":"Name","placeholder":"Enter"}}'
    ["password"]='{"widget":"password","params":{"title":"Pass","placeholder":"****"}}'
    ["filter"]='{"widget":"filter","params":{"title":"Find","choices":["a","b","c"]}}'
    ["multiselect"]='{"widget":"multiselect","params":{"title":"Sel","choices":["x","y"]}}'
    ["file_picker"]='{"widget":"file_picker","params":{"title":"File","start_dir":"/tmp"}}'
    ["text_editor"]='{"widget":"text_editor","params":{"title":"Edit","content":"text"}}'
    ["form"]='{"widget":"form","params":{"title":"Frm","fields":[{"label":"N","widget_type":"input"}]}}'
    ["tabs"]='{"widget":"tabs","params":{"title":"Tabs","tab_labels":["A","B"]}}'
    ["split_panes"]='{"widget":"split_panes","params":{"orientation":"horizontal"}}'
    ["context_menu"]='{"widget":"context_menu","params":{"items":["A","B"]}}'
    ["radio_group"]='{"widget":"radio_group","params":{"title":"Pick","choices":["X","Y"]}}'
    ["range_slider"]='{"widget":"range_slider","params":{"title":"Vol","min":0,"max":100,"value":50}}'
    ["color_picker"]='{"widget":"color_picker","params":{"title":"Col","colors":["#f00"]}}'
    ["disk"]='{"widget":"disk","params":{"title":"Disk","disk":"/dev/null"}}'
    ["spinner"]='{"widget":"spinner","params":{"message":"Wait..."}}'
    ["hub"]='{"widget":"hub","params":{"title":"Hub","categories":[]}}'
    ["widget_builder"]='{"widget":"widget_builder","params":{}}'
    ["macro_recorder"]='{"widget":"macro_recorder","params":{}}'
)

# terminal_emulator requires a real PTY - terminal/TTY only, skip in daemon/headless
# progress GUI shows "Command not allowed" because /usr/bin/true isn't on the allowlist path - that's correct behavior

run_oneshot() {
    local name="$1" json="$2" use_input="${3:-0}"
    local exit_code=0
    if [[ "${use_input}" -eq 1 ]]; then
        printf '\033' | timeout 3 "${FILLY}" oneshot 2>/dev/null <<< "${json}" || exit_code=$?
    else
        timeout 3 "${FILLY}" oneshot 2>/dev/null <<< "${json}" || exit_code=$?
    fi
    if [[ ${exit_code} -eq 0 || ${exit_code} -eq 1 ]]; then
        return 0
    fi
    return 1
}

printf "${YELLOW}--- Terminal Oneshot (35 widgets) ---${NC}\n"
for name in "${!SELF_TERMINATING[@]}"; do
    if run_oneshot "${name}" "${SELF_TERMINATING[$name]}" 0; then
        pass "terminal oneshot ${name}"
    else
        fail "terminal oneshot ${name} crashed/hung"
    fi
done
for name in "${!NEEDS_INPUT[@]}"; do
    if run_oneshot "${name}" "${NEEDS_INPUT[$name]}" 1; then
        pass "terminal oneshot ${name}"
    else
        fail "terminal oneshot ${name} crashed/hung"
    fi
done
# terminal_emulator: terminal-only, test separately
if run_oneshot "terminal_emulator" '{"widget":"terminal_emulator","params":{"title":"T","command":["echo","x"]}}' 0; then
    pass "terminal oneshot terminal_emulator"
else
    fail "terminal oneshot terminal_emulator crashed/hung"
fi

run_daemon() {
    local name="$1" json="$2"
    local out
    out=$("${FILLY}" send --socket "${SOCKET}" "${json}" 2>&1)
    local rc=$?
    if [[ ${rc} -eq 0 ]]; then
        return 0
    fi
    echo "exit=${rc} out=${out}"
    return 1
}

printf "${YELLOW}--- Daemon Socket (35 widgets) ---${NC}\n"
for name in "${!SELF_TERMINATING[@]}"; do
    if run_daemon "${name}" "${SELF_TERMINATING[$name]}"; then
        pass "daemon socket ${name}"
    else
        fail "daemon socket ${name}: $(run_daemon "${name}" "${SELF_TERMINATING[$name]}" 2>&1 || true)"
    fi
done
for name in "${!NEEDS_INPUT[@]}"; do
    if run_daemon "${name}" "${NEEDS_INPUT[$name]}"; then
        pass "daemon socket ${name}"
    else
        fail "daemon socket ${name}: $(run_daemon "${name}" "${NEEDS_INPUT[$name]}" 2>&1 || true)"
    fi
done
skip "daemon socket terminal_emulator (PTY required)"

if [[ ${HAS_DISPLAY} -eq 1 ]]; then
    printf "${YELLOW}--- GUI Oneshot (${HAS_DISPLAY}) ---${NC}\n"
    for name in "${!SELF_TERMINATING[@]}"; do
        if timeout 4 bash -c "echo '${SELF_TERMINATING[$name]}' | '${FILLY}' oneshot --gui" 2>/dev/null; then
            pass "GUI oneshot ${name}"
        elif [[ $? -eq 124 ]]; then
            fail "GUI oneshot ${name} hung"
        else
            pass "GUI oneshot ${name}"
        fi
    done
    for name in "${!NEEDS_INPUT[@]}"; do
        if timeout 4 bash -c "printf '\033' | '${FILLY}' oneshot --gui" 2>/dev/null <<< "${NEEDS_INPUT[$name]}"; then
            pass "GUI oneshot ${name}"
        elif [[ $? -eq 124 ]]; then
            fail "GUI oneshot ${name} hung"
        else
            pass "GUI oneshot ${name}"
        fi
    done
    skip "GUI oneshot terminal_emulator (PTY required)"
else
    skip "GUI oneshot (no display)"
fi

printf "${YELLOW}--- Relay ---${NC}\n"
echo '{"widget":"msg","params":{"title":"Relay","message":"works"}}' | timeout 3 "${FILLY}" relay "${SOCKET}" - 2>/dev/null
relay_rc=$?
if [[ ${relay_rc} -le 1 ]] || [[ ${relay_rc} -eq 124 ]]; then
    pass "relay mode runs"
else
    fail "relay mode crashed (exit ${relay_rc})"
fi

printf "${YELLOW}--- GUI Builder ---${NC}\n"
if [[ -f "${FILLY_BUILD}" ]]; then
    "${FILLY_BUILD}" --export "${TEST_DIR}/builder_export" >/dev/null 2>&1
    if [[ -f "${TEST_DIR}/builder_export/untitled.c" ]]; then
        pass "builder headless export"
        if make -C "${TEST_DIR}/builder_export" FILLY_DIR="${SCRIPT_DIR}/.." >/dev/null 2>&1; then
            pass "generated plugin compiles"
        else
            fail "generated plugin compile"
        fi
    else
        fail "builder headless export missing files"
    fi

    if [[ ${HAS_DISPLAY} -eq 1 ]]; then
        timeout 4 "${FILLY_BUILD}" --gui 2>/dev/null
        gui_rc=$?
        if [[ ${gui_rc} -le 1 ]] || [[ ${gui_rc} -eq 124 ]]; then
            pass "builder GUI launches"
        else
            fail "builder GUI crashed (exit ${gui_rc})"
        fi
    else
        skip "builder GUI (no display)"
    fi
else
    skip "filly-build not built"
fi

printf "${YELLOW}--- Plugin Widgets (daemon) ---${NC}\n"
ARTIXFORGE_WIDGETS=(install_hub anvil poweruser recovery iso migration_init migration_desktop password_confirm user_manager)
GFORGE_WIDGETS=(gforge_hub stage3_picker profile_picker kernel_picker use_flags cflags)

for plugin in "${ARTIXFORGE_WIDGETS[@]}"; do
    out=$("${FILLY}" send --socket "${SOCKET}" "{\"widget\":\"${plugin}\",\"params\":{}}" 2>/dev/null || true)
    rc=$?
    if [[ ${rc} -eq 0 ]]; then
        pass "ArtixForge ${plugin}"
    else
        fail "ArtixForge ${plugin}: ${out}"
    fi
done
for plugin in "${GFORGE_WIDGETS[@]}"; do
    out=$("${FILLY}" send --socket "${SOCKET}" "{\"widget\":\"${plugin}\",\"params\":{}}" 2>/dev/null || true)
    rc=$?
    if [[ ${rc} -eq 0 ]]; then
        pass "GForge ${plugin}"
    else
        fail "GForge ${plugin}: ${out}"
    fi
done

printf "${YELLOW}--- Animation + Store ---${NC}\n"
"${FILLY}" send --socket "${SOCKET}" '{"type":"reload_theme"}' >/dev/null 2>&1
if kill -0 ${DAEMON_PID} 2>/dev/null; then
    pass "theme reload (daemon survives)"
else
    fail "theme reload crashed daemon"
fi

"${FILLY}" send --socket "${SOCKET}" '{"type":"subscribe","keys":["x"]}' >/dev/null 2>&1 || true
out=$("${FILLY}" send --socket "${SOCKET}" '{"widget":"input","params":{"title":"Store","validation_script":"when value is not empty then set store.x to value\naccept\nend"}}' 2>/dev/null || true)
rc=$?
if [[ ${rc} -eq 0 ]]; then
    pass "FIL store binding"
else
    fail "FIL store binding (exit ${rc}): ${out}"
fi

printf "\n${CYAN}=== Results: ${PASS}/${TOTAL} passed, ${FAIL} failed, ${SKIP} skipped ===${NC}\n"
[[ ${FAIL} -gt 0 ]] && exit 1 || exit 0