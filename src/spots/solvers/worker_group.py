"""
Ray-based worker group implementation for distributed Sprouts solving.

This module provides the WorkerGroup class, which coordinates groups of workers
that share nimber databases for efficient distributed computation. Workers are
managed using the Ray distributed computing framework.

Key features:
- Shared nimber databases within worker groups
- Automatic load balancing and job distribution
- State management for different solving phases
- Inter-group nimber sharing capabilities
"""

import subprocess
import ray

from .config import games


@ray.remote
class WorkerGroup:
    """
    Represents a group of workers that share a single nimber database for distributed solving.

    This class encapsulates a group of C++ solver workers that collaborate on solving
    subtasks while sharing a common nimber database. The group can be configured with
    different state retention levels and sharing strategies.

    Attributes:
        _group: The underlying C++ implementation of the worker group.
        _group_id (int): Unique identifier for this worker group.
        _received_nimbers (int): Count of nimbers received from other groups.
        _cycles (dict): Maps currently processing jobs to their cycle numbers.
        _database_path (str): Path to nimber database file for state persistence.
        _download_script_path (str): Path to script for downloading shared state.
    """

    class Parameters:
        """
        Configuration parameters for a worker group.

        This nested class encapsulates all the configuration options needed to
        initialize and configure a worker group, including threading, database
        settings, and coordination parameters.

        Attributes:
            game (str): The game type ("Sprouts").
            grouping (int): Number of workers in the group.
            threads (int): Number of threads per worker.
            branching_depth (int): Fixed depth for synchronization tree growth.
            epsilon (int): Epsilon parameter for proof-number thresholds.
            heuristics (bool): Whether to enable heuristic evaluations.
            capacity (int): Maximum nodes in transposition tables.
            database_path (str): Path to nimber database for loading group state.
            download_script_path (str): Path to download script for shared state.
            verbose (bool): Whether to enable verbose logging output.
            state_level (int): State retention level (0=full, 1=nimbers, 2=none).
            share_nimbers (bool): Whether to enable inter-group nimber sharing.
            seed (int): Random seed for reproducible behavior.
        """

        def __init__(
            self,
            game,
            grouping,
            threads,
            branching_depth,
            epsilon,
            heuristics,
            capacity,
            database_path,
            download_script_path,
            verbose,
            state_level,
            share_nimbers,
            seed,
        ):
            """
            Initializes worker group parameters.

            Args:
                game (str): Game type identifier.
                grouping (int): Size of the worker group.
                threads (int): Number of threads per worker.
                branching_depth (int): Synchronization tree depth limit.
                epsilon (int): Proof-number search epsilon parameter.
                heuristics (bool): Enable heuristic position evaluation.
                capacity (int): Transposition table capacity per worker.
                database_path (str): Path to shared nimber database.
                download_script_path (str): Script for downloading shared state.
                verbose (bool): Enable detailed logging output.
                state_level (int): Worker state retention (0-2 scale).
                share_nimbers (bool): Enable nimber sharing between groups.
                seed (int): Random seed for deterministic behavior.
            """
            self.game = game
            self.grouping = grouping
            self.threads = threads
            self.branching_depth = branching_depth
            self.epsilon = epsilon
            self.heuristics = heuristics
            self.capacity = capacity
            self.database_path = database_path
            self.download_script_path = download_script_path
            self.verbose = verbose
            self.state_level = state_level
            self.share_nimbers = share_nimbers
            self.seed = seed

        def get_params(self):
            """
            Returns all parameters as a tuple for C++ constructor compatibility.

            Returns:
                tuple: Complete parameter set for initializing C++ worker group,
                       including game type, threading, database paths, and all
                       configuration flags in the correct order.
            """
            return (
                self.game,
                self.grouping,
                self.threads,
                self.branching_depth,
                self.epsilon,
                self.heuristics,
                self.capacity,
                self.database_path,
                self.download_script_path,
                self.verbose,
                self.state_level,
                self.share_nimbers,
                self.seed,
            )

    class Stats:
        """
        Statistics container for worker group performance metrics.

        This class collects and organizes performance data from a worker group
        including tree sizes, nimber counts, timing information, and job statistics.
        """

        def __init__(
            self, tree_sizes, nimbers, received_nimbers, iterations, jobs_num, mini_jobs_num, working_times, waiting_times
        ):
            """
            Initializes worker group statistics.

            Args:
                tree_sizes (list): Number of nodes in each worker's transposition table.
                nimbers (int): Total nimbers stored in the group's shared database.
                received_nimbers (int): New nimbers received from other groups.
                iterations (list): Iteration counts performed by each worker.
                jobs_num (int): Total number of jobs processed by the group.
                mini_jobs_num (int): Number of mini-jobs (sub-tasks) processed.
                working_times (list): Computation time for each worker in seconds.
                waiting_times (list): Idle time for each worker in seconds.
            """
            self.tree_sizes = tree_sizes
            self.nimbers = nimbers
            self.received_nimbers = received_nimbers
            self.iterations = iterations
            self.jobs_num = jobs_num
            self.mini_jobs_num = mini_jobs_num
            self.working_times = working_times
            self.waiting_times = waiting_times

    def __init__(self, parameters, group_id):
        """
        Args:
            parameters (Parameters): A group configuration.
            group_id (int): The ID of the worker group.
        """

        (
            game,
            grouping,
            threads,
            branching_depth,
            epsilon,
            heuristics,
            capacity,
            database_path,
            download_script_path,
            verbose,
            state_level,
            share_nimbers,
            seed,
        ) = parameters.get_params()
        self._group = games[game]["worker_group"](
            grouping, threads, branching_depth, epsilon, heuristics, capacity, state_level, share_nimbers, seed
        )
        self._group_id = group_id
        self._received_nimbers = 0
        self._cycles = {}
        self._database_path, self._download_script_path = database_path, download_script_path
        self._verbose = verbose
        self._stats = (None, None)  # stats before and right after the last job assignment

    def init(self):
        """
        Restores the state of the group from the database `self._database_path`. If a downloading script was
        configured, it first download the database locally.
        """
        if self._database_path:
            if self._download_script_path:
                self.__download_database(self._database_path, self._download_script_path)

            self._group.load_nimbers(self._database_path)

        if self._verbose:
            print(f"Group {self._group_id} initialized with {self.nimbers()} nimbers.", flush=True)

    def __download_database(self, database_path, download_script_path):
        """
        Downloads the database file locally using the given script.

        Args:
            database_path (str): The local path to the database file.
            download_script_path (str): The path to the downloading script.
        """
        if download_script_path and database_path:
            try:
                subprocess.run(["bash", download_script_path, database_path], check=True)
                if self._verbose:
                    print(f"Database {database_path} downloaded.")
            except subprocess.CalledProcessError:
                if self._verbose:
                    print(f"Downloading database {database_path} failed.")

    def __add_nimbers(self, new_nimbers):
        """
        Adds new nimbers to the nimber database of the group.

        Args:
            new_nimbers (spots_cpp.ComputedNimbers): The number of new nimbers to add.
        """
        self._received_nimbers += self._group.add_nimbers(new_nimbers)

    def ping(self):
        pass

    def complete_jobs(self, jobs, job_cycles, max_iterations, pending_nimbers):
        """
        Completes the given jobs by assigning them to the underlying group of workers.

        Args:
            jobs (list): A list of jobs to be completed.
            job_cycles (list): A list of current number of cycles for each job.
            max_iterations (int): The maximum number of iterations for the completion.
            pending_nimbers (spots_cpp.ComputedNimbers): The nimbers shared by other groups.

        Returns:
            tuple: A tuple containing the completed jobs, their current cycles, and newly computed nimbers.
        """
        pre_stats = self.get_stats()
        for job, cycle in zip(jobs, job_cycles):
            self._cycles[job.to_string()] = cycle

        self.__add_nimbers(pending_nimbers)
        completed_jobs, new_nimbers = self._group.complete_jobs(jobs, max_iterations)

        completed_job_cycles = []
        for completed_job in completed_jobs:
            completed_job_cycles.append(self._cycles[completed_job.to_string()] + 1)
            del self._cycles[completed_job.to_string()]

        post_stats = self.get_stats()
        self._stats = (pre_stats, post_stats)
        return completed_jobs, completed_job_cycles, new_nimbers

    def clear_nimbers(self):
        """
        Clears the nimbers in the worker group.
        """
        self._group.clear_nimbers()

    def get_last_stats(self):
        """
        Returns the statistics of the worker group before and after the last job assignment.
        """
        return self._stats

    def get_stats(self):
        """
        Returns the statistics of the worker group.

        Returns:
            Stats: An object containing the statistics of the worker group.
        """
        return WorkerGroup.Stats(
            self.tree_sizes(),
            self.nimbers(),
            self.received_nimbers(),
            self.iterations(),
            self.jobs_num(),
            self.mini_jobs_num(),
            self.working_times(),
            self.waiting_times(),
        )

    def iterations(self):
        """
        Returns the number of iterations performed by the worker group in total.
        """
        return self._group.iterations()

    def jobs_num(self):
        """
        Returns the number of jobs performed by the worker group in total.
        """
        return self._group.jobs_num()

    def mini_jobs_num(self):
        """
        Returns the number of mini jobs performed by the worker group in total.
        """
        return self._group.mini_jobs_num()

    def tree_sizes(self):
        """
        Returns the number of nodes stored in the transposition tables of underlying df-pn solvers the worker group.
        """
        return self._group.tree_sizes()

    def nimbers(self):
        """
        Returns the number of nimbers stored in the nimber database of the group.
        """
        return self._group.nimbers()

    def received_nimbers(self):
        """
        Returns the number of new nimbers recieved from other groups.
        """
        return self._received_nimbers

    def working_times(self):
        """
        Returns the computations times of individual workers in the group in seconds.
        """
        return [time / 1000 for time in self._group.working_times()]

    def waiting_times(self):
        """
        Returns the computations times of individual workers in the group in seconds.
        """
        return [time / 1000 for time in self._group.waiting_times()]

    def get_id(self):
        """
        Returns the ID of the worker group.
        """
        return self._group_id
