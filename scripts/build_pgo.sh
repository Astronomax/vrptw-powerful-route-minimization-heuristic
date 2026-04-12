#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEN_BUILD_DIR="${GEN_BUILD_DIR:-$ROOT_DIR/pgo-generate-build}"
USE_BUILD_DIR="${USE_BUILD_DIR:-$ROOT_DIR/pgo-build}"
PGO_PROFILE_DIR="${PGO_PROFILE_DIR:-$ROOT_DIR/pgo-data}"
BENCHMARK_DIR="${BENCHMARK_DIR:-$ROOT_DIR/GehringHomberger1000}"
DEFAULT_T_MAX="${DEFAULT_T_MAX:-60}"
RUN_SPECS=()

usage() {
    cat <<EOF
Usage: $(basename "$0") [--run INSTANCE:LOWER_BOUND[:T_MAX]]...

Builds the solver in two phases:
1. a profile-generation build;
2. a profile-use build that consumes the generated data.

Options:
  --run SPEC            Training run in the form INSTANCE:LOWER_BOUND[:T_MAX].
                        May be specified multiple times.
  --gen-build-dir DIR   Directory for the profile-generation build.
  --use-build-dir DIR   Directory for the final optimized PGO build.
  --profile-dir DIR     Directory where GCC writes profile data.
  --benchmark-dir DIR   Directory with benchmark instances.
  --default-t-max SEC   Default time limit for runs without explicit T_MAX.
  -h, --help            Show this help.

Environment overrides:
  GEN_BUILD_DIR, USE_BUILD_DIR, PGO_PROFILE_DIR, BENCHMARK_DIR, DEFAULT_T_MAX

Examples:
  $(basename "$0")
  $(basename "$0") --run C1_10_1:100:60
  $(basename "$0") --run C1_10_1:100:60 --run RC2_10_1:20:60
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --run)
            [[ $# -ge 2 ]] || { echo "missing value for --run" >&2; exit 1; }
            RUN_SPECS+=("$2")
            shift 2
            ;;
        --gen-build-dir)
            [[ $# -ge 2 ]] || { echo "missing value for --gen-build-dir" >&2; exit 1; }
            GEN_BUILD_DIR="$2"
            shift 2
            ;;
        --use-build-dir)
            [[ $# -ge 2 ]] || { echo "missing value for --use-build-dir" >&2; exit 1; }
            USE_BUILD_DIR="$2"
            shift 2
            ;;
        --profile-dir)
            [[ $# -ge 2 ]] || { echo "missing value for --profile-dir" >&2; exit 1; }
            PGO_PROFILE_DIR="$2"
            shift 2
            ;;
        --benchmark-dir)
            [[ $# -ge 2 ]] || { echo "missing value for --benchmark-dir" >&2; exit 1; }
            BENCHMARK_DIR="$2"
            shift 2
            ;;
        --default-t-max)
            [[ $# -ge 2 ]] || { echo "missing value for --default-t-max" >&2; exit 1; }
            DEFAULT_T_MAX="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ ${#RUN_SPECS[@]} -eq 0 ]]; then
    RUN_SPECS=("C1_10_1:100:${DEFAULT_T_MAX}")
fi

require_command() {
    local command_name="$1"
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "$command_name" >&2
        exit 1
    fi
}

require_file() {
    local path="$1"
    local description="$2"
    if [[ ! -f "$path" ]]; then
        printf 'Missing %s: %s\n' "$description" "$path" >&2
        exit 1
    fi
}

require_command cmake

printf 'PGO profile dir: %s\n' "$PGO_PROFILE_DIR"
rm -rf "$PGO_PROFILE_DIR"

cmake -S "$ROOT_DIR" -B "$GEN_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPGO_MODE=GENERATE \
    -DPGO_PROFILE_DIR="$PGO_PROFILE_DIR"
cmake --build "$GEN_BUILD_DIR" --target routes -j

for run_spec in "${RUN_SPECS[@]}"; do
    IFS=':' read -r instance lower_bound t_max <<<"$run_spec"
    if [[ -z "${instance:-}" || -z "${lower_bound:-}" ]]; then
        printf 'Invalid --run value: %s\n' "$run_spec" >&2
        exit 1
    fi
    if [[ -z "${t_max:-}" ]]; then
        t_max="$DEFAULT_T_MAX"
    fi

    problem_file="$BENCHMARK_DIR/${instance}.TXT"
    solution_file="$GEN_BUILD_DIR/${instance}.pgo.sol"
    require_file "$problem_file" "benchmark instance"

    printf '\nTraining on %s (lower_bound=%s, t_max=%s)\n' \
        "$instance" "$lower_bound" "$t_max"
    "$GEN_BUILD_DIR/routes" \
        "$problem_file" \
        "$solution_file" \
        --lower_bound "$lower_bound" \
        --t_max "$t_max" \
        --beta_correction \
        --log_level normal
    rm -f "$solution_file"
done

cmake -S "$ROOT_DIR" -B "$USE_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPGO_MODE=USE \
    -DPGO_PROFILE_DIR="$PGO_PROFILE_DIR"
cmake --build "$USE_BUILD_DIR" --target routes -j

printf '\nPGO build completed.\n'
printf 'Final binary: %s\n' "$USE_BUILD_DIR/routes"
