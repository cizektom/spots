"""
Statistics logging utilities for SPOTS solver performance analysis.

This module provides functions for logging solver statistics to both stdout
and CSV files. It handles both sequential and parallel solver metrics with
comprehensive performance breakdowns and worker-level details.

The logging system supports:
- Real-time stdout progress reporting
- CSV export for data analysis
- Thread-safe file operations with locking
- Extensible metric collection
"""

import csv
import os
import fcntl


def log_stats_csv(stats, stats_path, args):
    """
    Logs solver statistics and arguments to a CSV file in a thread-safe manner.

    This function appends results to an existing CSV file or creates a new one
    with appropriate headers. File locking ensures thread-safety for concurrent
    writes from multiple workers or processes.

    Args:
        stats (dict): Statistics dictionary as returned by solver.get_stats().
                     Contains metrics like tree size, solving time, iterations, etc.
        stats_path (str): Path to the CSV file where statistics will be written.
        args (argparse.Namespace): Command-line arguments to include as metadata.
                                  These are logged alongside performance metrics.
    """
    args_row = vars(args)

    # Construct comprehensive row with all available metrics
    row = {
        "datetime": stats.get("datetime", ""),
        "position": stats.get("position", ""),
        "nimber": stats.get("nimber", ""),
        "outcome": (
            stats.get("outcome", "").to_string() if hasattr(stats.get("outcome", ""), "to_string") else stats.get("outcome", "")
        ),
        # Master/sequential solver metrics
        "master_tree_nodes": stats.get("master_tree_nodes", ""),
        "master_nimbers": stats.get("master_nimbers", ""),
        "master_computed_nimbers": stats.get("master_computed_nimbers", ""),
        # Distributed solver job metrics
        "jobs_assigned": stats.get("jobs_assigned", ""),
        "jobs_done": stats.get("jobs_done", ""),
        "jobs_closed": stats.get("jobs_closed", ""),
        "jobs_open": stats.get("jobs_open", ""),
        "jobs_updated": stats.get("jobs_updated", ""),
        # Master timing breakdown
        "master_time": stats.get("master_time", ""),
        "master_time_init": stats.get("master_time_breakdown", {}).get("init", ""),
        "master_time_assign": stats.get("master_time_breakdown", {}).get("assign", ""),
        "master_time_submit": stats.get("master_time_breakdown", {}).get("submit", ""),
        "master_time_backup": stats.get("master_time_breakdown", {}).get("backup", ""),
        "master_time_prune": stats.get("master_time_breakdown", {}).get("prune", ""),
        # Worker aggregate metrics
        "workers_nodes_mean": stats.get("workers_nodes_mean", ""),
        "workers_nimbers_mean": stats.get("workers_nimbers_mean", ""),
        "jobs_num_sum": stats.get("jobs_num_sum", ""),
        "mini_jobs_num_sum": stats.get("mini_jobs_num_sum", ""),
        "workers_iterations_mean": stats.get("workers_iterations_mean", ""),
        "workers_times_mean": stats.get("workers_times_mean", ""),
        "workers_utils_mean": stats.get("workers_utils_mean", ""),
        # Overall timing and efficiency
        "solving_time": stats.get("solving_time", ""),
        "total_iterations": stats.get("total_iterations", ""),
        "master_time_percent": stats.get("master_time_percent", ""),
        "workers_time_percent": stats.get("workers_time_percent", ""),
        # Per-worker detailed metrics (as stringified lists for CSV compatibility)
        "workers_nodes": str(stats.get("workers_nodes", "")),
        "workers_nimbers": str(stats.get("workers_nimbers", "")),
        "workers_computed_nimbers": str(stats.get("workers_computed_nimbers", "")),
        "jobs_num": str(stats.get("jobs_num", "")),
        "mini_jobs_num": str(stats.get("mini_jobs_num", "")),
        "workers_iterations": str(stats.get("workers_iterations", "")),
        "workers_times": str(stats.get("workers_times", "")),
        "workers_utils": str(stats.get("workers_utils", "")),
    }

    # Merge command-line args with statistics, prioritizing args in header order
    full_row = {**args_row, **row}
    file_exists = os.path.isfile(stats_path)
    fieldnames = None

    # Thread-safe file operations with exclusive locking
    with open(stats_path, "a+", newline="", encoding="utf-8") as csvfile:
        fcntl.flock(csvfile.fileno(), fcntl.LOCK_EX)
        csvfile.seek(0)
        first_line = csvfile.readline()

        if file_exists and first_line:
            # File exists with content - preserve existing header order
            reader = csv.reader([first_line])
            existing_header = next(reader)
            # Add any new fields not in existing header to the end
            fieldnames = existing_header + [k for k in full_row.keys() if k not in existing_header]
            has_header = first_line.startswith(fieldnames[0])
        else:
            # New file or empty file - create header with args first, then stats
            fieldnames = list(args_row.keys()) + [k for k in row.keys() if k not in args_row]
            has_header = False

        csvfile.seek(0, os.SEEK_END)
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

        if not file_exists or not has_header:
            writer.writeheader()
        writer.writerow(full_row)
        csvfile.flush()
        fcntl.flock(csvfile.fileno(), fcntl.LOCK_UN)


def print_args(args):
    """
    Prints command-line arguments in a formatted manner.

    Args:
        args: Parsed command-line arguments from argparse.
    """
    print("Args: ", end="")
    for i, arg in enumerate(vars(args)):
        print(f'{arg}="{getattr(args, arg)}"', end=(", " if i != (len(vars(args)) - 1) else "\n"))


def log_parallel_stats_stdout(stats, args=None):
    """
    Logs comprehensive statistics for parallel/distributed solvers to stdout.

    This function provides a detailed breakdown of master and worker performance
    including timing analysis, job distribution, and efficiency metrics.

    Args:
        stats (dict): Statistics dictionary containing parallel solver metrics.
        args (argparse.Namespace, optional): Command-line arguments to display.
    """
    if args is not None:
        print_args(args)

    print("-" * 110, flush=True)
    print(f'{stats["position"]}+*{stats["nimber"]} is {stats["outcome"].to_string()} ({stats["datetime"]}):')
    print()

    # Master node statistics and timing breakdown
    print("Master:")
    print(f"\tNodes:      {stats['master_tree_nodes']:-10}")
    print(f"\tNimbers:    {stats['master_nimbers']:-10}  \t[COMP={stats['master_computed_nimbers']:.2f}%]")
    print(
        f"\tJobs:       {stats['jobs_assigned']:-10}  \t[DONE={stats['jobs_done']}, CLSD={stats['jobs_closed']}, OPEN={stats['jobs_open']}, UPDT={stats['jobs_updated']}]"
    )
    print(f"\tTime:       {stats['master_time']:-10.2f} s", end="")
    mtb = stats["master_time_breakdown"]
    mt = stats["master_time"]
    if mt > 0:
        print(
            f"\t[INIT={100*mtb['init']/mt:.2f}% ASGN={100*mtb['assign']/mt:.2f}%, SBMT={100*mtb['submit']/mt:.2f}%, BCKUP={100*mtb['backup']/mt:.2f}%, PRUN={100*mtb['prune']/mt:.2f}%]"
        )
    else:
        print()
    print()

    # Worker statistics with per-worker details
    print("Workers:")
    print(f"\tNodes:      {stats['workers_nodes_mean']:-10.0f}  \t{stats['workers_nodes']}")
    print(f"\tNimbers:    {stats['workers_nimbers_mean']:-10.0f}  \t[", end="")
    print(
        *[
            f"{n} ({100*s:.0f}%)" if n is not None else n
            for n, s in zip(stats["workers_nimbers"], stats["workers_computed_nimbers"])
        ],
        sep=", ",
        end="",
    )
    print("]")
    print(f"\tJobs:       {stats['jobs_num_sum']:-10.0f}  \t[", end="")
    print(
        *[
            f"{jobs} ({mini_jobs})" if jobs is not None else jobs
            for jobs, mini_jobs in zip(stats["jobs_num"], stats["mini_jobs_num"])
        ],
        sep=", ",
        end="",
    )
    print(f"] (mini: {stats['mini_jobs_num_sum']})")
    print(f"\tIterations: {stats['workers_iterations_mean']:-10.0f}  \t{stats['workers_iterations']}")
    print(f"\tTime:       {stats['workers_times_mean']:-10.2f} s \t[", end="")
    print(*[f"{v:.2f}" if v is not None else v for v in stats["workers_times"]], sep=", ", end="")
    print("]")
    print(f"\tUtil:       {stats['workers_utils_mean']:-10.2f}% \t[", end="")
    print(*[f"{v:.2f}%" if v is not None else v for v in stats["workers_utils"]], sep=", ", end="")
    print("]")
    print()

    # Overall performance summary
    print("Summary:")
    print(f"\tIterations: {stats['total_iterations']:-10.0f}")
    print(
        f"\tTime:       {stats['solving_time']:-10.2f} s\t[MSTR={stats['master_time_percent']:0.2f}%, WRKS={stats['workers_time_percent']:0.2f}%]"
    )
    print("-" * 110, flush=True)


def log_sequential_stats_stdout(stats, args=None):
    """
    Logs concise statistics for sequential solvers to stdout.

    Provides a clean summary of solving metrics for single-threaded algorithms
    including tree size, iterations, and timing information.

    Args:
        stats (dict): Statistics dictionary containing sequential solver metrics.
        args (argparse.Namespace, optional): Command-line arguments to display.
    """
    if args is not None:
        print_args(args)

    print("--------------------------------------")
    print(f'{stats["position"]}+*{stats["nimber"]} is {stats["outcome"].to_string()}:')
    print()
    print(f'\tNodes:      {stats["master_tree_nodes"]:-10}')
    print(f'\tNimbers:    {stats["master_nimbers"]:-10}')
    print(f'\tIterations: {stats["total_iterations"]:-10.0f}')
    print(f'\tTime:       {stats["solving_time"]:-10.2f} s')
    print("--------------------------------------")
