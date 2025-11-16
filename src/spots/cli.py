"""
Command-line interface for the SPOTS (Sprouts Parallel Outcome Tree Search) solver.

This module provides the main entry point for solving Sprouts game positions using
various algorithms including sequential solvers (DFS, PNS, DFPN) and parallel
solvers (Parallel DFPN, Parallel PNS-PDFPN).

Usage:
    python spots.py <position> --algorithm <algorithm> [options]

Supported algorithms:
    - dfs: Depth-First Search
    - pns: Proof-Number Search
    - dfpn: Depth-First Proof-Number Search
    - pdfpn: Parallel Depth-First Proof-Number Search (by Kaneko 2010)
    - pns-pdfpn: Parallel PNS with PDFPN workers (Ray-based)

Example:
    python spots.py "0*12" --algorithm dfpn --capacity 1000000 for solving a 12-spots Sprouts position using DFPN algorithm with a transposition table capacity of 1 million nodes (for Sprouts position encodings, see Applegate, Jacobson, Sleator 1991 or Lemoine and Viennot 2015).
"""

from functools import partial
import argparse

# Available solving algorithms
solvers = ["pns-pdfpn", "pns", "dfpn", "pdfpn", "dfs"]

parser = argparse.ArgumentParser(
    description="SPOTS: Sprouts Parallel Outcome Tree Search",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog="""
Algorithm descriptions:
  dfs        Depth-First Search
  pns        Proof-Number Search
  dfpn       Depth-First Proof-Number Search
  pdfpn      Parallel DFPN by Kaneko 2010 for shared-memory systems
  pns-pdfpn  Distributed PNS-PDFPN for distributed-memory systems (Ray-based)

For distributed solving (pns-pdfpn), Ray cluster can be specified via --address.
    """,
)

# General arguments
parser.add_argument("position", help="Sprouts position to be solved, e.g. '0*12' for a 12-spots game")
parser.add_argument("--algorithm", choices=solvers, required=True, help="Solving algorithm to use")
parser.add_argument(
    "--compute_nimber", dest="compute_nimber", action="store_true", help="Compute nimber value instead of just win/loss outcome"
)

parser.add_argument("--capacity", default=100_000, type=int, help="Maximum capacity of transposition table (default: 100k)")

parser.add_argument("--input_database", default="", type=str, help="Path to input nimber database file")
parser.add_argument(
    "--output_database", default="database.spr", type=str, help="Path to output nimber database file (default: database.spr)"
)
parser.add_argument(
    "--stats_path", default="stats.csv", type=str, help="Path to output CSV file for statistics (default: stats.csv)"
)

parser.add_argument("--verbose", dest="verbose", action="store_true", help="Enable verbose output during solving")
parser.add_argument("--seed", default=0, type=int, help="Random seed for solver (0 for no randomization)")

# PNS-PDFPN distributed solving arguments
parser.add_argument("--workers", default=1, type=int, help="Number of Ray workers for distributed solving")
parser.add_argument("--threads", default=0, type=int, help="Number of threads per worker (0 for DFPN, > 0 for PDFPN)")

parser.add_argument("--iterations", default=100, type=int, help="Maximum number of iterations per job")
parser.add_argument("--updates", default=100, type=int, help="Frequency of updates from workers")
parser.add_argument("--grouping", default=1, type=int, help="Worker group size for shared Grundy number (nimber) databases")

parser.add_argument(
    "--no_sharing", dest="no_sharing", action="store_true", help="Disable Grundy number (nimber) sharing between worker groups"
)
parser.add_argument(
    "--state_level",
    default=0,
    type=int,
    help="Worker state retention level: 0=full state, 1=Grundy number (nimber) only, 2=no state",
)

parser.add_argument("--address", default="", type=str, help="Address of existing Ray server to connect to")

parser.set_defaults(no_sharing=False, compute_nimber=False, verbose=False)


def solver_initializer(args):
    """
    Creates and configures the appropriate solver based on command-line arguments.

    Args:
        args: Parsed command-line arguments from argparse.

    Returns:
        callable: A partially configured solver constructor that can be called
                 to create solver instances.
    """
    game = "Sprouts"
    if args.algorithm == "pns-pdfpn":
        # Distributed solving with Ray framework
        from .solvers.parallel_solver import DistributedSolver
        import ray

        if args.address:
            ray.init(address=args.address)
        else:
            ray.init()

        solver_init = partial(
            DistributedSolver,
            game=game,
            workers=args.workers,
            threads=args.threads,
            iterations=args.iterations,
            updates=args.updates,
            depth=0,
            epsilon=1,
            grouping=args.grouping,
            heuristics=False,
            capacity=args.capacity,
            input_database_path=args.input_database,
            output_database_path=args.output_database,
            download_script_path="",
            upload_script_path="",
            verbose=args.verbose,
            no_vcpus=False,
            no_sharing=args.no_sharing,
            state_level=args.state_level,
            seed=args.seed,
        )

    elif args.algorithm == "pdfpn":
        # Parallel DFPN (multi-threaded single node)
        from .solvers.parallel_solver import ParallelSolver

        solver_init = partial(
            ParallelSolver,
            game=game,
            threads=args.threads,
            depth=0,
            epsilon=1,
            heuristics=False,
            capacity=args.capacity,
            input_database_path=args.input_database,
            output_database_path=args.output_database,
            seed=args.seed,
        )
    else:
        # Sequential solvers (dfs, pns, dfpn)
        from spots.solvers.sequential_solver import SequentialSolver

        solver_init = partial(
            SequentialSolver,
            game=game,
            algorithm=args.algorithm,
            input_database_path=args.input_database,
            output_database_path=args.output_database,
            verbose=args.verbose,
            heuristics=False,
            capacity=args.capacity,
            seed=args.seed,
        )

    return solver_init


def solve(args):
    """
    Main solving loop that handles nimber computation and result logging.

    When computing nimbers (--compute_nimber), this function iteratively
    solves for increasing nimber values until a losing position is found,
    which determines the position's nimber value.

    Args:
        args: Parsed command-line arguments.
    """
    solver_init = solver_initializer(args)

    nimber = 0
    while True:
        solver = solver_init()
        stats = solver.solve(args.position, nimber)
        solver.log_stats(stats, args)

        nimber += 1
        # Stop if not computing nimber, or if we found a losing position
        if not args.compute_nimber or stats["outcome"].is_loss():
            break


def main(argv=None):
    args = parser.parse_args(argv)
    solve(args)


if __name__ == "__main__":
    main()
