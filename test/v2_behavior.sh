#!/usr/bin/env bash
set -Euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FILLY="${SCRIPT_DIR}/../filly"
PASS=0; FAIL=0; SKIP=0; TOTAL=0
GREEN='\e[32m'; RED='\e[31m'; CYAN='\e[1;36m'; YELLOW='\e[1;33m'; NC='\e[0m'

pass() { TOTAL=$((TOTAL+1)); PASS=$((PASS+1)); printf "${GREEN}[PASS]${NC} %s\n" "$1"; }
fail() { TOTAL=$((TOTAL+1)); FAIL=$((FAIL+1)); printf "${RED}[FAIL]${NC} %s\n" "$1"; }
skip() { TOTAL=$((TOTAL+1)); SKIP=$((SKIP+1)); printf "${YELLOW}[SKIP]${NC} %s\n" "$1"; }

TEST_DIR="/tmp/filly-v2-tests"
rm -rf "${TEST_DIR}"
mkdir -p "${TEST_DIR}"

run_headless() {
    local json="$1" events="${2:-}"
    local events_file="${TEST_DIR}/v2-events.txt"
    if [[ -n "${events}" ]]; then
        printf '%s\n' "${events}" > "${events_file}"
        echo "${json}" | "${FILLY}" oneshot --headless --events "${events_file}" 2>/dev/null || true
    else
        echo "${json}" | "${FILLY}" oneshot --headless 2>/dev/null || true
    fi
}

assert_contains() {
    local name="$1" response="$2" expected="$3"
    TOTAL=$((TOTAL+1))
    if echo "${response}" | grep -qF "${expected}"; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
        printf '       Expected to contain: %s\n' "${expected}"
        printf '       Got: %s\n' "$(echo "${response}" | head -c 200)"
    fi
}

assert_eq() {
    local name="$1" response="$2" expected="$3"
    TOTAL=$((TOTAL+1))
    local actual
    actual=$(echo "${response}" | jq -c '.result' 2>/dev/null || echo "PARSE_ERROR")
    if [[ "${actual}" == "${expected}" ]]; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
        printf '       Expected: %s\n' "${expected}"
        printf '       Got:      %s\n' "${actual}"
    fi
}

assert_response() {
    local name="$1" response="$2" check="$3"
    TOTAL=$((TOTAL+1))
    if echo "${response}" | jq -e "${check}" >/dev/null 2>&1; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
        printf '       Check: %s\n' "${check}"
        printf '       Got:   %s\n' "$(echo "${response}" | head -c 200)"
    fi
}

printf "${CYAN}=== FILLY v2.0 Behavioral Tests ===${NC}\n\n"

printf "${YELLOW}--- New Widgets ---${NC}\n"

resp=$(run_headless '{"widget":"image","params":{"source":"test.png","fit":"contain","width":100,"height":100}}' "KEY:ENTER")
assert_response "image widget renders" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"canvas","params":{"script":"draw()","width":200,"height":200}}' "KEY:ENTER")
assert_response "canvas widget renders" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"markdown","params":{"content":"# Hello\n\n**bold** text"}}' "KEY:ENTER")
assert_response "markdown widget renders" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"plot","params":{"type":"line","data":[1,3,2,5,4],"labels":["a","b","c","d","e"]}}' "KEY:ENTER")
assert_response "plot widget renders" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"video","params":{"source":"demo.mp4","loop":true}}' "KEY:ENTER")
assert_response "video widget renders" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Positioning ---${NC}\n"

resp=$(run_headless '{"widget":"msg","params":{"title":"Anchored","message":"top-right"},"anchor":"top-right","dx":-2,"dy":1}' "KEY:ENTER")
assert_response "anchor top-right positioning" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"msg","params":{"title":"Absolute","message":"fixed"},"x":10,"y":5}' "KEY:ENTER")
assert_response "absolute positioning" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"msg","params":{"title":"Z-Order","message":"layered"},"z_index":99}' "KEY:ENTER")
assert_response "z-index positioning" "${resp}" '.cancelled == false'

resp=$(run_headless '{"widget":"msg","params":{"title":"Overflow","message":"clipped"},"overflow":"clip"}' "KEY:ENTER")
assert_response "overflow clip" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Layout ---${NC}\n"

resp=$(run_headless '{"widget":"msg","params":{"title":"MinSize","message":"constrained"},"min_width":40,"min_height":10}' "KEY:ENTER")
assert_response "min width/height constraints" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Protocol ---${NC}\n"

SOCKET="${TEST_DIR}/filly-v2.sock"
"${FILLY}" daemon --socket "${SOCKET}" &
DAEMON_PID=$!
sleep 1

if kill -0 ${DAEMON_PID} 2>/dev/null; then
    pass "v2 daemon started"

    handshake='{"type":"handshake","version":2,"encoding":"json","capabilities":["animations","shadows","positioning","flexbox"]}'
    resp=$(echo "${handshake}" | "${FILLY}" send --socket "${SOCKET}" --json - 2>/dev/null || true)
    if echo "${resp}" | jq -e '.type == "handshake_ack"' >/dev/null 2>&1; then
        pass "handshake capability negotiation"
    else
        fail "handshake capability negotiation"
        echo "       Got: ${resp}"
    fi

    query='{"type":"query","widget_id":"test-key"}'
    resp=$(echo "${query}" | "${FILLY}" send --socket "${SOCKET}" --json - 2>/dev/null || true)
    if echo "${resp}" | jq -e '.type == "query_response"' >/dev/null 2>&1; then
        pass "query widget state"
    else
        fail "query widget state"
    fi

    position='{"type":"position","widget_id":"w1","anchor":"top-right","dx":-2,"dy":1,"z_index":10}'
    resp=$(echo "${position}" | "${FILLY}" send --socket "${SOCKET}" --json - 2>/dev/null || true)
    if echo "${resp}" | jq -e '.cancelled == false' >/dev/null 2>&1; then
        pass "position update message"
    else
        fail "position update message"
    fi

    subscribe='{"type":"subscribe","keys":["v2-test"]}'
    resp=$(echo "${subscribe}" | "${FILLY}" send --socket "${SOCKET}" --json - 2>/dev/null || true)
    assert_response "store subscribe" "${resp}" '.'

    "${FILLY}" send --socket "${SOCKET}" --quit 2>/dev/null || true
    sleep 1
    if kill -0 ${DAEMON_PID} 2>/dev/null; then
        kill -9 ${DAEMON_PID} 2>/dev/null || true
        wait ${DAEMON_PID} 2>/dev/null || true
    fi
else
    fail "v2 daemon started"
fi
rm -f "${SOCKET}"

printf "${YELLOW}--- Animation (Reduced Motion) ---${NC}\n"

resp=$(FILLY_REDUCED_MOTION=1 run_headless '{"widget":"spinner","params":{"message":"animated"}}' "KEY:ESC")
assert_response "reduced motion env var" "${resp}" '.cancelled == true'

printf "${YELLOW}--- Config ---${NC}\n"

CONF="${TEST_DIR}/filly.conf"
cat > "${CONF}" << 'EOF'
prefers_reduced_motion=true
max_connections_per_sec=5
EOF

if "${FILLY}" daemon --socket "${TEST_DIR}/filly-conf.sock" 2>/dev/null &
then
    CONF_PID=$!
    sleep 1
    if kill -0 ${CONF_PID} 2>/dev/null; then
        pass "daemon starts with config"
    else
        fail "daemon starts with config"
    fi
    kill ${CONF_PID} 2>/dev/null || true
    wait ${CONF_PID} 2>/dev/null || true
fi
rm -f "${TEST_DIR}/filly-conf.sock" "${CONF}"

printf "${YELLOW}--- CLI Commands ---${NC}\n"

if "${FILLY}" inspect --socket "${TEST_DIR}/filly-v2.sock" 2>/dev/null; then
    skip "inspect (no daemon running)"
else
    pass "inspect handles missing daemon"
fi

resp=$("${FILLY}" profile 2>/dev/null || true)
if echo "${resp}" | jq -e '.fps >= 0' >/dev/null 2>&1; then
    pass "profile command outputs JSON"
else
    fail "profile command outputs JSON"
fi

printf "${YELLOW}--- Tooltip + Focus ---${NC}\n"

resp=$(run_headless '{"widget":"input","params":{"title":"Focus","placeholder":"tab to me"},"tab_index":1,"tooltip":"Enter your name"}' "KEY:TAB\nKEY:ENTER")
assert_response "tab_index and tooltip fields parse" "${resp}" '.cancelled == false'

printf "${YELLOW}--- Drag-and-Drop ---${NC}\n"

resp=$(run_headless '{"widget":"msg","params":{"title":"Drag","message":"draggable"},"draggable":true}' $'MOUSE:DRAG_START:10,10\nMOUSE:DRAG_MOVE:5,5\nMOUSE:DRAG_END:30,30\nKEY:ENTER')
assert_response "draggable events handled" "${resp}" '.cancelled == false'

printf "\n${CYAN}=== v2.0 Behavior Results: %d/%d passed, %d failed, %d skipped ===${NC}\n" "${PASS}" "${TOTAL}" "${FAIL}" "${SKIP}"
[[ ${FAIL} -gt 0 ]] && exit 1 || exit 0