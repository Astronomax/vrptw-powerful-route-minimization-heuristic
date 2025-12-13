#!/usr/bin/env python3
"""
Benchmark script for GehringHomberger1000 benchmark set.
Runs tests according to the original paper methodology.
"""

import json
import os
import re
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple, Optional


# Best known results (optimum)
BEST_RESULTS = {
    "c1_10_1": 100, "c1_10_2": 90, "c1_10_3": 90, "c1_10_4": 90, "c1_10_5": 100,
    "c1_10_6": 99, "c1_10_7": 97, "c1_10_8": 92, "c1_10_9": 90, "c1_10_10": 90,
    "c2_10_1": 30, "c2_10_2": 29, "c2_10_3": 28, "c2_10_4": 28, "c2_10_5": 30,
    "c2_10_6": 29, "c2_10_7": 29, "c2_10_8": 28, "c2_10_9": 28, "c2_10_10": 28,
    "r1_10_1": 100, "r1_10_2": 91, "r1_10_3": 91, "r1_10_4": 91, "r1_10_5": 91,
    "r1_10_6": 91, "r1_10_7": 91, "r1_10_8": 91, "r1_10_9": 91, "r1_10_10": 91,
    "r2_10_1": 19, "r2_10_2": 19, "r2_10_3": 19, "r2_10_4": 19, "r2_10_5": 19,
    "r2_10_6": 19, "r2_10_7": 19, "r2_10_8": 19, "r2_10_9": 19, "r2_10_10": 19,
    "rc1_10_1": 90, "rc1_10_2": 90, "rc1_10_3": 90, "rc1_10_4": 90, "rc1_10_5": 90,
    "rc1_10_6": 90, "rc1_10_7": 90, "rc1_10_8": 90, "rc1_10_9": 90, "rc1_10_10": 90,
    "rc2_10_1": 20, "rc2_10_2": 18, "rc2_10_3": 18, "rc2_10_4": 18, "rc2_10_5": 18,
    "rc2_10_6": 18, "rc2_10_7": 18, "rc2_10_8": 18, "rc2_10_9": 18, "rc2_10_10": 18,
}

# Time limits in minutes (as per paper: 10 min, 60 min, n/200 hours = 5 hours = 300 min)
TIME_LIMITS = [10, 60, 300]

# Time limits in seconds for t_max
TIME_LIMITS_SEC = [10 * 60, 60 * 60, 300 * 60]


def get_test_instances() -> List[str]:
    """Generate list of all test instance names."""
    instances = []
    for prefix in ["c1", "c2", "r1", "r2", "rc1", "rc2"]:
        for i in range(1, 11):
            instances.append(f"{prefix}_10_{i}")
    return instances


def parse_routes_from_output(line: str) -> Optional[int]:
    """Parse number of routes from log output line."""
    # Look for "routes number: X" pattern from log_level=normal (may have ANSI color codes)
    match = re.search(r'routes number:\s*(\d+)', line, re.IGNORECASE)
    if match:
        return int(match.group(1))
    
    # Also check for "n_routes: X" from final output
    match = re.search(r'n_routes:\s*(\d+)', line, re.IGNORECASE)
    if match:
        return int(match.group(1))
    
    return None


def run_test(instance: str, problem_dir: Path, build_dir: Path, 
              lower_bound: int, time_limit_sec: int) -> Tuple[Dict[int, int], bool]:
    """
    Run a single test and capture route counts at different time points.
    
    Returns:
        (route_counts, success) where route_counts maps time_limit -> route_count
        and success indicates if the run completed successfully
    """
    # Convert instance name to file name (e.g., "c1_10_1" -> "C1_10_1.TXT")
    # The instance name format is already correct, just uppercase it
    file_name = instance.upper() + ".TXT"
    problem_file = problem_dir / file_name
    
    if not problem_file.exists():
        print(f"Warning: Problem file not found: {problem_file}", file=sys.stderr)
        return {}, False
    
    solution_file = Path(f"{instance}.sol")
    routes_exe = build_dir / "routes"
    
    if not routes_exe.exists():
        print(f"Error: Executable not found: {routes_exe}", file=sys.stderr)
        return {}, False
    
    # Track route counts at different time points
    route_counts = {}
    start_time = time.time()
    
    # Build command
    cmd = [
        str(routes_exe),
        str(problem_file),
        str(solution_file),
        "--lower_bound", str(lower_bound),
        "--log_level=normal",
        "--t_max", str(time_limit_sec),
        "--beta_correction"
    ]
    
    print(f"Running {instance} with t_max={time_limit_sec}s, lower_bound={lower_bound}...", 
          file=sys.stderr, flush=True)
    
    try:
        # Run the process and capture output in real-time
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        # Monitor output for route counts
        last_route_count = None
        last_route_time = start_time
        
        # Also set up periodic checks for time limits
        check_times = [start_time + sec for sec in TIME_LIMITS_SEC]
        next_check_idx = 0
        
        for line in process.stdout:
            elapsed = time.time() - start_time
            
            # Check for route count updates
            route_count = parse_routes_from_output(line)
            if route_count is not None:
                last_route_count = route_count
                last_route_time = time.time()
                # Check if we've hit any of our time limits (elapsed is in seconds)
                for i, (limit_min, limit_sec) in enumerate(zip(TIME_LIMITS, TIME_LIMITS_SEC)):
                    if elapsed >= limit_sec and limit_min not in route_counts:
                        route_counts[limit_min] = route_count
                        print(f"  [{instance}] At {limit_min} min: {route_count} routes", 
                              file=sys.stderr, flush=True)
            
            # Periodic check: if we've passed a time limit without seeing an update, use last known
            while next_check_idx < len(check_times):
                if time.time() >= check_times[next_check_idx]:
                    limit_min = TIME_LIMITS[next_check_idx]
                    if limit_min not in route_counts and last_route_count is not None:
                        route_counts[limit_min] = last_route_count
                        print(f"  [{instance}] At {limit_min} min (no update): {last_route_count} routes", 
                              file=sys.stderr, flush=True)
                    next_check_idx += 1
                else:
                    break
            
            # Also print to stderr for monitoring
            print(line, end='', file=sys.stderr, flush=True)
        
        # Wait for process to complete
        return_code = process.wait()
        
        success = (return_code == 0)
        
        # Clean up solution file
        if solution_file.exists():
            solution_file.unlink()
        
        return route_counts, success
        
    except Exception as e:
        print(f"Error running {instance}: {e}", file=sys.stderr)
        return {}, False


def main():
    """Main benchmarking function."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Benchmark GehringHomberger1000 instances"
    )
    parser.add_argument(
        "--problem-dir",
        type=Path,
        default=Path("GehringHomberger1000"),
        help="Directory containing problem files"
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build"),
        help="Directory containing the 'routes' executable"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("benchmark_results.json"),
        help="Output JSON file"
    )
    parser.add_argument(
        "--instance",
        type=str,
        help="Run only a specific instance (for testing)"
    )
    
    args = parser.parse_args()
    
    # Get all test instances
    instances = [args.instance] if args.instance else get_test_instances()
    
    # Results structure
    results = {
        "time_limits_minutes": TIME_LIMITS,
        "results": {},
        "sums": {},
        "not_optimal": {}
    }
    
    # Run all tests
    for instance in instances:
        if instance not in BEST_RESULTS:
            print(f"Warning: No best result for {instance}, skipping", file=sys.stderr)
            continue
        
        lower_bound = BEST_RESULTS[instance]
        # Run with longest timeout (300 min = 18000 sec)
        route_counts, success = run_test(
            instance,
            args.problem_dir,
            args.build_dir,
            lower_bound,
            TIME_LIMITS_SEC[-1]  # 300 minutes
        )
        
        if success:
            results["results"][instance] = {
                "best_known": BEST_RESULTS[instance],
                "route_counts": route_counts
            }
        else:
            print(f"Warning: {instance} failed to run", file=sys.stderr)
    
    # Calculate sums for each time limit
    for limit_min in TIME_LIMITS:
        total = 0
        for instance, data in results["results"].items():
            if limit_min in data["route_counts"]:
                total += data["route_counts"][limit_min]
        results["sums"][f"{limit_min}_min"] = total
    
    # Find instances where optimum was not achieved, sorted by difference
    for limit_min in TIME_LIMITS:
        not_optimal = []
        for instance, data in results["results"].items():
            best_known = data["best_known"]
            if limit_min in data["route_counts"]:
                route_count = data["route_counts"][limit_min]
                if route_count > best_known:
                    diff = route_count - best_known
                    not_optimal.append({
                        "instance": instance,
                        "best_known": best_known,
                        "route_count": route_count,
                        "difference": diff
                    })
        
        # Sort by difference (descending)
        not_optimal.sort(key=lambda x: x["difference"], reverse=True)
        results["not_optimal"][f"{limit_min}_min"] = not_optimal
    
    # Output results as JSON
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\nResults written to {args.output}", file=sys.stderr)
    print(f"\nSummary:", file=sys.stderr)
    print(f"  Total routes (10 min):  {results['sums']['10_min']}", file=sys.stderr)
    print(f"  Total routes (60 min):  {results['sums']['60_min']}", file=sys.stderr)
    print(f"  Total routes (300 min): {results['sums']['300_min']}", file=sys.stderr)
    
    # Print not optimal instances
    for limit_min in TIME_LIMITS:
        key = f"{limit_min}_min"
        not_opt = results["not_optimal"][key]
        if not_opt:
            print(f"\nNot optimal at {limit_min} min ({len(not_opt)} instances):", 
                  file=sys.stderr)
            for item in not_opt[:10]:  # Show top 10
                print(f"  {item['instance']}: {item['route_count']} (best: {item['best_known']}, diff: +{item['difference']})", 
                      file=sys.stderr)


if __name__ == "__main__":
    main()
