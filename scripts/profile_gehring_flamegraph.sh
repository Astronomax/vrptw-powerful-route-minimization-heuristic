#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/perf-build}"
SOLVER_BIN="${SOLVER_BIN:-$BUILD_DIR/routes}"
BENCHMARK_DIR="${BENCHMARK_DIR:-$ROOT_DIR/GehringHomberger1000}"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-$ROOT_DIR/FlameGraph}"
INSTANCE="${INSTANCE:-C1_10_1}"
LOWER_BOUND="${LOWER_BOUND:-100}"
T_MAX="${T_MAX:-60}"
FREQ="${FREQ:-99}"
OUTPUT_BASE_DIR="${OUTPUT_BASE_DIR:-$ROOT_DIR/perf-results}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [INSTANCE] [LOWER_BOUND] [T_MAX]

Profiles the VRPTW solver on one GehringHomberger1000 instance and generates
a flame graph using Linux perf and the scripts from FlameGraph/.

Arguments:
  INSTANCE      Benchmark instance name without extension (default: ${INSTANCE})
  LOWER_BOUND   Solver lower bound (default: ${LOWER_BOUND})
  T_MAX         Solver time limit in seconds (default: ${T_MAX})

Environment overrides:
  BUILD_DIR, SOLVER_BIN, BENCHMARK_DIR, FLAMEGRAPH_DIR, OUTPUT_BASE_DIR, FREQ

Example:
  $(basename "$0")
  $(basename "$0") C1_10_1 100 120
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -ge 1 ]]; then
    INSTANCE="$1"
fi

if [[ $# -ge 2 ]]; then
    LOWER_BOUND="$2"
fi

if [[ $# -ge 3 ]]; then
    T_MAX="$3"
fi

if [[ $# -gt 3 ]]; then
    usage >&2
    exit 1
fi

PROBLEM_FILE="$BENCHMARK_DIR/${INSTANCE}.TXT"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RUN_DIR="$OUTPUT_BASE_DIR/${INSTANCE}_${TIMESTAMP}"
SOLUTION_FILE="$RUN_DIR/${INSTANCE}.sol"
PERF_DATA_FILE="$RUN_DIR/perf.data"
PERF_SCRIPT_FILE="$RUN_DIR/perf.script"
FOLDED_FILE="$RUN_DIR/perf.folded"
SVG_FILE="$RUN_DIR/${INSTANCE}.svg"

require_file() {
    local path="$1"
    local description="$2"
    if [[ ! -f "$path" ]]; then
        printf 'Missing %s: %s\n' "$description" "$path" >&2
        exit 1
    fi
}

require_command() {
    local command_name="$1"
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "$command_name" >&2
        exit 1
    fi
}

require_command perf
require_command perl
require_file "$SOLVER_BIN" "solver binary"
require_file "$PROBLEM_FILE" "benchmark instance"
require_file "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" "FlameGraph stackcollapse script"
require_file "$FLAMEGRAPH_DIR/flamegraph.pl" "FlameGraph renderer"

mkdir -p "$RUN_DIR"

printf 'Profiling %s with %s\n' "$INSTANCE" "$SOLVER_BIN"
printf 'Problem: %s\n' "$PROBLEM_FILE"
printf 'Output directory: %s\n' "$RUN_DIR"

if ! perf record \
    -F "$FREQ" \
    --call-graph dwarf \
    -o "$PERF_DATA_FILE" \
    -- \
    "$SOLVER_BIN" \
    "$PROBLEM_FILE" \
    "$SOLUTION_FILE" \
    --lower_bound "$LOWER_BOUND" \
    --t_max "$T_MAX" \
    --beta_correction \
    --log_level normal; then
    cat <<EOF >&2
perf record failed.
If this is a permissions issue, check:
  cat /proc/sys/kernel/perf_event_paranoid
and lower the value if needed, or run the script with sufficient privileges.
EOF
    exit 1
fi

perf script -i "$PERF_DATA_FILE" > "$PERF_SCRIPT_FILE"
perl "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" --all "$PERF_SCRIPT_FILE" > "$FOLDED_FILE"
perl "$FLAMEGRAPH_DIR/flamegraph.pl" \
    --title "routes ${INSTANCE}" \
    --countname samples \
    "$FOLDED_FILE" > "$SVG_FILE"

printf '\nGenerated files:\n'
printf '  perf data:    %s\n' "$PERF_DATA_FILE"
printf '  perf script:  %s\n' "$PERF_SCRIPT_FILE"
printf '  folded stacks:%s\n' " $FOLDED_FILE"
printf '  flame graph:  %s\n' "$SVG_FILE"
