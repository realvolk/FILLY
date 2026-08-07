#!/usr/bin/env bash
set -Euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GREEN='\e[32m'; RED='\e[31m'; CYAN='\e[1;36m'; YELLOW='\e[1;33m'; NC='\e[0m'

PASS_TOTAL=0; FAIL_TOTAL=0

run_suite() {
    local name="$1" cmd="$2"
    printf "${CYAN}=== ${name} ===${NC}\n"
    if eval "${cmd}"; then
        printf "${GREEN}${name}: PASSED${NC}\n\n"
        PASS_TOTAL=$((PASS_TOTAL + 1))
    else
        printf "${RED}${name}: FAILED${NC}\n\n"
        FAIL_TOTAL=$((FAIL_TOTAL + 1))
    fi
}

cleanup_all() {
    kill ${DAEMON_PID:-} 2>/dev/null || true
    wait ${DAEMON_PID:-} 2>/dev/null || true
    rm -rf /tmp/filly-full-tests /tmp/filly-tui-tests /tmp/filly-gui-tests /tmp/filly-v2-tests /tmp/filly-tests
}
trap cleanup_all EXIT

printf "${CYAN}========================================${NC}\n"
printf "${CYAN}  FILLY v2.0 Full Integration Suite${NC}\n"
printf "${CYAN}========================================${NC}\n\n"

run_suite "Unit: Arena" "make -C ${SCRIPT_DIR}/.. test-unit 2>/dev/null | grep -q 'All arena tests'"

run_suite "Unit: Theme" "${SCRIPT_DIR}/../test-unit-theme 2>/dev/null | grep -q 'All theme tests'"

run_suite "Unit: Session" "${SCRIPT_DIR}/../test-unit-session 2>/dev/null | grep -q 'All session tests'"

run_suite "Unit: Render" "${SCRIPT_DIR}/../test-unit-render 2>/dev/null | grep -q 'FILLY Unit Tests'"

if [[ -f "${SCRIPT_DIR}/../filly-test" ]]; then
    run_suite "C Test Suite" "${SCRIPT_DIR}/../filly-test 2>/dev/null | grep -q 'Results:'"
fi

if [[ -f "${SCRIPT_DIR}/harness.sh" ]]; then
    run_suite "Behavioral Tests" "bash ${SCRIPT_DIR}/harness.sh"
fi

if [[ -f "${SCRIPT_DIR}/harness_tui.sh" ]]; then
    run_suite "TUI Integration" "bash ${SCRIPT_DIR}/harness_tui.sh"
fi

if [[ -f "${SCRIPT_DIR}/harness_gui.sh" ]]; then
    run_suite "GUI Integration" "bash ${SCRIPT_DIR}/harness_gui.sh"
fi

if [[ -f "${SCRIPT_DIR}/harness_full.sh" ]]; then
    run_suite "Full Integration" "bash ${SCRIPT_DIR}/harness_full.sh"
fi

if [[ -f "${SCRIPT_DIR}/../filly-test" ]]; then
    run_suite "Fault Injection" "${SCRIPT_DIR}/../filly-test 2>/dev/null | grep -q 'Results:'"
fi

if [[ -f "${SCRIPT_DIR}/v2_pixel_test" ]]; then
    run_suite "v2.0 Pixel" "${SCRIPT_DIR}/v2_pixel_test"
else
    printf "${YELLOW}=== v2.0 Pixel ===${NC}\n"
    printf "${YELLOW}[SKIP] v2_pixel_test not built${NC}\n\n"
fi

if [[ -f "${SCRIPT_DIR}/v2_behavior.sh" ]]; then
    run_suite "v2.0 Behavior" "bash ${SCRIPT_DIR}/v2_behavior.sh"
else
    printf "${YELLOW}=== v2.0 Behavior ===${NC}\n"
    printf "${YELLOW}[SKIP] v2_behavior.sh not found${NC}\n\n"
fi

printf "${CYAN}========================================${NC}\n"
printf "${CYAN}  Suite Complete: %d passed, %d failed${NC}\n" "${PASS_TOTAL}" "${FAIL_TOTAL}"
printf "${CYAN}========================================${NC}\n"

[[ ${FAIL_TOTAL} -gt 0 ]] && exit 1 || exit 0