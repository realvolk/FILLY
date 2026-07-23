#!/usr/bin/env bash
set -Euo pipefail
FILLY="../filly"
PASS=0; FAIL=0; SKIP=0
TOTAL=0
TEST_DIR="/tmp/filly-tui-tests"
mkdir -p "${TEST_DIR}"

GREEN='\e[32m'; RED='\e[31m'; CYAN='\e[1;36m'; YELLOW='\e[1;33m'; NC='\e[0m'

run_widget() {
    local json="$1" events="${2:-}"
    local events_file="${TEST_DIR}/tui-events.txt"
    printf '%s\n' "${events}" > "${events_file}"
    echo "${json}" | "${FILLY}" oneshot --headless --insecure-plugins --events "${events_file}" 2>/dev/null || true
}

assert_contains() {
    local name="$1" response="$2" expected="$3"
    TOTAL=$((TOTAL+1))
    if echo "${response}" | grep -qF "${expected}"; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
        printf '       Expected to contain: %s\n' "${expected}"
        printf '       Got: %s\n' "${response}"
    fi
}

assert_eq() {
    local name="$1" response="$2" expected="$3"
    TOTAL=$((TOTAL+1))
    local actual
    actual=$(echo "${response}" | jq -c '.result' 2>/dev/null || echo "")
    if [[ "${actual}" == "${expected}" ]]; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
        printf '       Expected: %s\n' "${expected}"
        printf '       Got:      %s\n' "${actual}"
    fi
}

printf "${CYAN}=== FILLY TUI Integration Tests ===${NC}\n\n"

printf "${YELLOW}--- Widget Logic (Headless) ---${NC}\n"
resp=$(run_widget '{"widget":"msg","params":{"title":"TUI","message":"works"}}' "KEY:ENTER")
assert_contains "msg produces response" "${resp}" '"cancelled":false'

resp=$(run_widget '{"widget":"menu","params":{"title":"Pick","choices":["A","B"]}}' $'KEY:DOWN\nKEY:ENTER')
assert_eq "menu navigation" "${resp}" '"B"'

resp=$(run_widget '{"widget":"input","params":{"title":"Name","default":"Artix"}}' "KEY:ENTER")
assert_eq "input returns default" "${resp}" '"Artix"'

resp=$(run_widget '{"widget":"yesno","params":{"title":"Go?","default":true}}' "KEY:ENTER")
assert_eq "yesno works" "${resp}" 'true'

resp=$(run_widget '{"widget":"checklist","params":{"title":"P","choices":["a","b"]}}' $'KEY:SPACE\nKEY:ENTER')
assert_eq "checklist toggles" "${resp}" '["a"]'

resp=$(run_widget '{"widget":"filter","params":{"title":"F","choices":["X","Y"]}}' $'TEXT:X\nKEY:ENTER')
assert_eq "filter narrows" "${resp}" '"X"'

printf "${YELLOW}--- Daemon Mode ---${NC}\n"
"${FILLY}" daemon --socket /tmp/filly-tui-daemon.sock --insecure-plugins &
DAEMON_PID=$!
sleep 2
if kill -0 ${DAEMON_PID} 2>/dev/null; then
    printf "${GREEN}[PASS]${NC} daemon starts\n"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1))
    resp=$(timeout 3 "${FILLY}" send --socket /tmp/filly-tui-daemon.sock '{"widget":"msg","params":{"title":"D","message":"daemon works"}}' 2>/dev/null || true)
    if echo "${resp}" | grep -qF '"cancelled":false'; then
        printf "${GREEN}[PASS]${NC} daemon processes request\n"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1))
    else
        printf "${RED}[FAIL]${NC} daemon processes request (timeout or error)\n"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1))
    fi
    timeout 2 "${FILLY}" send --socket /tmp/filly-tui-daemon.sock --quit 2>/dev/null || true
    sleep 1
    if kill -0 ${DAEMON_PID} 2>/dev/null; then
        kill -9 ${DAEMON_PID} 2>/dev/null
        printf "${RED}[FAIL]${NC} daemon quits (forced kill)\n"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1))
    else
        printf "${GREEN}[PASS]${NC} daemon quits\n"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1))
    fi
else
    printf "${RED}[FAIL]${NC} daemon starts\n"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1))
fi
kill ${DAEMON_PID} 2>/dev/null; wait ${DAEMON_PID} 2>/dev/null
rm -f /tmp/filly-tui-daemon.sock

printf "${YELLOW}--- filly send / filly build CLI ---${NC}\n"
resp=$("${FILLY}" build menu --title "CLITest" --choice A --choice B 2>/dev/null || true)
if echo "${resp}" | grep -qF '"widget":"menu"'; then
    printf "${GREEN}[PASS]${NC} filly build produces JSON\n"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1))
else
    printf "${RED}[FAIL]${NC} filly build produces JSON\n"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1))
fi

resp=$(echo '{"widget":"msg","params":{"title":"T","message":"M"}}' | "${FILLY}" oneshot --headless --insecure-plugins 2>/dev/null || true)
if echo "${resp}" | grep -qF '"cancelled":false'; then
    printf "${GREEN}[PASS]${NC} filly oneshot stdin\n"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1))
else
    printf "${RED}[FAIL]${NC} filly oneshot stdin\n"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1))
fi

printf "\n${CYAN}=== TUI Results: %d/%d passed ===${NC}\n" "${PASS}" "${TOTAL}"
[[ ${FAIL} -gt 0 ]] && exit 1 || exit 0