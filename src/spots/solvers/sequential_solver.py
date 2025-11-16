"""
Sequential solver implementations for Sprouts game positions.

This module provides Python wrappers for various sequential solving algorithms
including depth-first search (DFS), proof-number search (PNS), and
depth-first proof-number search (DFPN).
"""

import time
import datetime

from .config import games
from .stats_logger import log_stats_csv, log_sequential_stats_stdout


class SequentialSolver:
    """
    A sequential solver for Sprouts game positions.

    This class provides a unified interface for various sequential solving algorithms.
    It wraps the underlying C++ implementations and provides statistics collection
    and logging functionality.

    Attributes:
        _solver: The underlying C++ solver implementation (varies by algorithm).
        _output_database_path (str): Path where the solved nimber database will be stored.
        _verbose (bool): Flag indicating whether to enable verbose output during solving.
    """

    def __init__(
        self,
        game,
        algorithm,
        input_database_path="",
        output_database_path="",
        heuristics=False,
        capacity=100_000,
        verbose=False,
        seed=0,
    ):
        """
        Initializes the sequential solver with the specified algorithm.

        Args:
            game (str): The game type (currently supports "Sprouts").
            algorithm (str): The solving algorithm to use ("dfs", "pns", or "dfpn").
            input_database_path (str): Path to pre-existing nimber database to load.
            output_database_path (str): Path where solved database will be saved.
            heuristics (bool): Whether to enable heuristic evaluations.
            capacity (int): Maximum number of nodes in transposition table.
            verbose (bool): Flag indicating whether to print verbose output during solving.
            seed (int): Random seed for reproducible behavior (0 for no randomization).
        """
        self._solver = (
            games[game][algorithm](input_database_path, verbose, heuristics, capacity, seed)
            if input_database_path
            else games[game][algorithm](verbose, heuristics, capacity, seed)
        )
        self._output_database_path = output_database_path
        self._verbose = verbose

    def clear(self):
        """
        Clears the internal state of the solver.

        This resets the transposition table and other internal data structures,
        allowing the solver to be reused for different positions.
        """
        self._solver.clear()

    def solve(self, position, nimber=0):
        """
        Solves the given Sprouts position using the configured algorithm.

        Args:
            position: The Sprouts position to be solved (game-specific format).
            nimber (int): The Nim heap size.

        Returns:
            dict: Statistics dictionary containing solving metrics and results.
                  Includes timing, tree size, nimber count, and outcome information.
        """
        start = time.time()
        outcome = self._solver.solve(position, nimber)
        stats = self.get_stats(position, nimber, outcome, start)

        if self._output_database_path:
            self._solver.store_database(self._output_database_path)

        return stats

    def get_stats(self, position, nimber, outcome, start_time):
        """
        Collects and returns comprehensive statistics from the solving process.

        Args:
            position: The solved position.
            nimber (int): The Nim heap size.
            outcome: The computed outcome/result.
            start_time (float): Timestamp when solving began.

        Returns:
            dict: Statistics including position, outcome, timing, tree metrics,
                  and iteration counts for performance analysis.
        """
        solving_time = time.time() - start_time
        stats = {
            "position": position,
            "nimber": nimber,
            "outcome": outcome,
            "datetime": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "master_tree_nodes": self._solver.tree_size(),
            "master_nimbers": self._solver.nimbers(),
            "solving_time": solving_time,
            "total_iterations": self._solver.iterations(),
        }
        return stats

    def log_stats(self, stats, args):
        """
        Logs solver statistics to both stdout and CSV file.

        Args:
            stats (dict): Statistics dictionary from get_stats().
            args: Command-line arguments containing output configuration.
        """
        log_sequential_stats_stdout(stats, args)
        log_stats_csv(stats, args.stats_path, args)
