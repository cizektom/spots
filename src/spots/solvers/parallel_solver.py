"""
Distributed parallel solver based on PNS-PDFPN using Ray framework for Sprouts game positions.

This module implements a master-worker architecture for distributed solving of
Sprouts positions using Proof-Number Search with parallel DFPN workers. The
master coordinates job distribution while workers perform intensive tree search.

Key features:
- Ray-based distributed computing
- Dynamic load balancing with worker groups
- Shared Grundy number (nimber) databases for efficiency
- Comprehensive performance monitoring
- Fault tolerance and worker management
"""

import logging
import time
import datetime
import subprocess
from enum import Enum
import ray

import ray.exceptions
import spots._cpp
from .stats_logger import log_stats_csv, log_parallel_stats_stdout, log_sequential_stats_stdout

from .config import games
from .worker_group import WorkerGroup

logger = logging.getLogger(__name__)
logging.basicConfig(format="%(asctime)s %(levelname)-8s %(message)s", level=logging.ERROR, datefmt="%Y-%m-%d %H:%M:%S")


class ParallelSolver:
    """
    A parallel depth-first proof-number search solver for Sprouts.

    This solver uses multiple threads to perform parallel depth-first exploration
    of the game tree while maintaining proof and disproof numbers for efficient
    pruning. It follows Kaneko's parallel DFPN algorithm.

    Attributes:
        _solver: The underlying C++ parallel DFPN solver implementation.
        _output_database_path (str): Path where the solved nimber database will be stored.
    """

    def __init__(
        self,
        game,
        threads,
        depth=0,
        epsilon=1,
        heuristics=False,
        capacity=100_000,
        input_database_path="",
        output_database_path="",
        seed=0,
    ):
        """
        Initializes the parallel DFPN solver.

        Args:
            game (str): The game type (currently supports "Sprouts").
            threads (int): Number of threads to use (minimum 1).
            depth (int): Maximum search depth of synchronization tree (0 for no synchronization in pure Kaneko PDFPN).
            epsilon (int): Epsilon parameter for proof-number thresholds.
            heuristics (bool): Whether to enable heuristic evaluations.
            capacity (int): Maximum number of nodes in transposition table.
            input_database_path (str): Path to pre-existing nimber database to load.
            output_database_path (str): Path where solved database will be saved.
            seed (int): Random seed for reproducible behavior (0 for no randomization).
        """
        self._solver = (
            games[game]["pdfpn"](max(threads, 1), depth, epsilon, input_database_path, heuristics, capacity, seed)
            if input_database_path
            else games[game]["pdfpn"](max(threads, 1), depth, epsilon, heuristics, capacity, seed)
        )
        self._output_database_path = output_database_path

    def clear(self):
        """
        Clears the internal state of the solver.

        This resets the transposition table and other internal data structures.
        """
        self._solver.clear()

    def solve(self, position, nimber=0):
        """
        Solves the given Sprouts position using parallel DFPN search.

        Args:
            position: The Sprouts position to be solved.
            nimber (int): The target nimber value to compute for the position.

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
            nimber (int): The Nim heap size in the couple part.
            outcome: The computed outcome.
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


class DistributedSolver:
    """
    Master coordinator for distributed Sprouts solving using proof-number search.

    This class implements the master node in a distributed solving architecture,
    coordinating multiple worker groups that perform parallel tree search while
    sharing nimber databases. The master handles job assignment, result collection,
    progress monitoring, and performance optimization.

    Key responsibilities:
    - Job decomposition and distribution to worker groups
    - Progress monitoring and statistics collection
    - Inter-group nimber sharing coordination
    - Database backup and tree pruning management
    - Load balancing and worker utilization optimization

    Attributes:
        BACKUP_FREQ (int): Database backup interval in seconds.
        PRUNE_FREQ (int): Master tree pruning interval in seconds.
        STATS_LOG_FREQ (int): Statistics logging interval in seconds.
        DEBUG_LOG_FREQ (int): Debug information logging interval in seconds.
        INIT_NODES_PER_WORKER (int): Minimum tree nodes per worker before job expansion.
    """

    BACKUP_FREQ = 28800  # 8 hours
    PRUNE_FREQ = 28800  # 8 hours
    STATS_LOG_FREQ = 7200  # 2 hours
    DEBUG_LOG_FREQ = 600  # 10 minutes

    INIT_NODES_PER_WORKER = 100

    class GroupState:
        """
        State management for worker groups in the distributed system.

        Tracks the current status and job assignments for each worker group,
        enabling efficient load balancing and job distribution across the
        distributed workforce.

        Attributes:
            _grouping (int): Number of workers in this group.
            assigned_jobs (list): Currently processing jobs assigned to the group.
            state (GroupState.State): Current operational state of the group.
        """

        State = Enum("State", ["INIT", "READY", "PROCESSING"])
        """
        Enumeration of possible group states:
        - INIT: Group is being initialized (loading databases, starting workers).
        - READY: Group is ready to receive job assignments.
        - PROCESSING: Group is actively working on assigned jobs.
        """

        def __init__(self, grouping):
            """
            Initializes the GroupState with the specified group size.

            Args:
                grouping (int): Number of workers in this group.
            """
            self._grouping = grouping
            self.assigned_jobs = []
            self.state = self.State.INIT

        def available_workers(self):
            """
            Returns the number of available workers in the group.

            Available workers are those not currently assigned to jobs.
            Only groups in READY state have available workers.

            Returns:
                int: Number of workers available for job assignment.
            """
            if self.state != self.State.READY:
                return 0

            return self._grouping - len(self.assigned_jobs)

        def is_assignable(self, jobs):
            """
            Checks if the specified jobs can be assigned to this group.

            Jobs can be assigned if the group is in READY state and has
            sufficient available workers. Empty job lists are only assignable
            if the group is already processing other jobs.

            Args:
                jobs (list): List of jobs to potentially assign.

            Returns:
                bool: True if jobs can be assigned to this group.
            """
            if self.state != self.State.READY:
                return False

            return len(jobs) <= self.available_workers() and (len(jobs) > 0 or len(self.assigned_jobs) > 0)

        def is_being_initialized(self):
            """
            Checks if the group is currently being initialized.

            Returns:
                bool: True if group is in INIT state.
            """
            return self.state == self.State.INIT

        def assign_jobs(self, jobs):
            """
            Assigns jobs to the group and transitions to PROCESSING state.

            Args:
                jobs (list): List of jobs to assign to this group.
            """
            self.assigned_jobs += jobs
            self.state = self.State.PROCESSING

        def deassign_jobs(self, completed_jobs):
            """
            Removes completed jobs from the group's assignment list.

            Transitions the group back to READY state after deassigning jobs,
            making it available for new job assignments.

            Args:
                completed_jobs (list): List of jobs that have been completed.
            """
            for completed_job in completed_jobs:
                self.assigned_jobs = [job for job in self.assigned_jobs if job.to_string() != completed_job.to_string()]

            self.state = self.State.READY

        def restart(self):
            """
            Restarts the group to INIT state, clearing all job assignments.

            Used when a group needs to be reinitialized, typically after
            an error or when changing configuration parameters.
            """
            self.assigned_jobs = []
            self.state = self.State.INIT

        def restore(self):
            """
            Restores the group to READY state, clearing all job assignments.

            Used to reset a group to a clean ready state without full
            reinitialization, making it available for new work.
            """
            self.assigned_jobs = []
            self.state = self.State.READY

    class TimeStamps:
        """
        Timestamp tracking for periodic maintenance operations.

        Tracks when various maintenance operations were last performed
        to schedule regular database backups, tree pruning, and logging.
        """

        def __init__(self):
            """Initialize all timestamps to current time."""
            self.last_backup, self.last_prune, self.last_stats_log, self.last_debug_log = (time.time() for _ in range(4))

        def reset(self):
            """Reset all timestamps to current time."""
            self.__init__()

    class RunningTimes:
        """
        Cumulative timing statistics for master operations.

        Tracks the total time spent in different master operations to
        analyze performance bottlenecks and system efficiency.
        """

        def __init__(self):
            """Initialize all timing counters to zero."""
            self.init_time, self.assign_time, self.submit_time, self.store_time, self.backup_time, self.prune_time = (
                0 for _ in range(6)
            )

        def reset(self):
            """Reset all timing counters to zero."""
            self.__init__()

        def total_time(self):
            """
            Calculates the total time spent in all tracked operations.

            Returns:
                float: Sum of all operation times in seconds.
            """
            return self.init_time + self.assign_time + self.submit_time + self.store_time + self.backup_time + self.prune_time

    def __init__(
        self,
        game,
        workers,
        threads,
        iterations=100,
        updates=100,
        depth=0,
        epsilon=1,
        grouping=1,
        heuristics=False,
        capacity=100_000,
        input_database_path="",
        output_database_path="",
        upload_script_path="",
        download_script_path="",
        verbose=False,
        no_vcpus=False,
        no_sharing=False,
        state_level=0,
        seed=0,
    ):
        """
        Initializes the ParallelSolver.

        Args:
            game (str): The name of the game to be solved.
            workers (int): The total number of worker processes.
            iterations (int): The maximum number of iterations for a worker's job processing.
            threads (int): The number of threads per worker.
            depth (int): The maximum depth of the synchronization tree in workers.
            epsilon (float): The e-trick parameter for the synchronization tree in workers.
            updates (int): The frequency of updates of worker's job progress.
            grouping (int): The group size of workers sharing the same nimber database.
            heuristics (bool): Whether to use heuristic estimates of proof numbers.
            capacity (int): Capacity of the transposition table.
            input_database_path (str): Path to the input nimber database.
            download_script_path (str): Path to the script for downloading the nimber database.
            verbose (bool): Whether to enable verbose logging.
            no_vcpus (bool): Whether to disable vCPU allocation for Ray workers.
        """
        self._groups_info, self._result_refs, self._init_refs, self._pending_nimbers = [], {}, {}, []
        self._max_iterations, self._max_cycles = updates, iterations // updates
        self._received_nimbers = 0
        self._output_database_path, self._upload_script_path = output_database_path, upload_script_path
        self._verbose, self._no_sharing = verbose, no_sharing
        self._time_stamps = DistributedSolver.TimeStamps()
        self._running_times = DistributedSolver.RunningTimes()
        self._assigned_jobs, self._submitted_jobs, self._updated_jobs, self._closed_jobs = 0, 0, 0, 0

        self._worker_params = WorkerGroup.Parameters(
            game,
            grouping,
            threads,
            depth,
            epsilon,
            heuristics,
            capacity,
            input_database_path,
            download_script_path,
            verbose,
            state_level,
            not no_sharing,
            seed,
        )
        self._groups = [
            WorkerGroup.options(num_cpus=(2 if no_vcpus else 1) * grouping * max(1, threads), num_gpus=0).remote(
                self._worker_params, group_id
            )
            for group_id in range(workers // grouping)
        ]
        self.__init_groups()
        self._tree_manager = (
            games[game]["manager"](input_database_path, verbose, heuristics, seed)
            if input_database_path
            else games[game]["manager"](verbose, heuristics, seed)
        )

        logger.info("Master loaded with %s nimbers.", self._tree_manager.nimbers())

    def __restore_groups(self):
        """
        Restores the groups that were successfully initialized.
        """
        if len(self._init_refs) == 0:
            return

        ready_init_refs, _ = ray.wait(list(self._init_refs.keys()), num_returns=1, timeout=0)
        for init_ref in ready_init_refs:
            group_id = self._init_refs[init_ref]
            del self._init_refs[init_ref]
            try:
                self._groups_info[group_id].restore()
                logger.info("Group %s initialized.", group_id)
            except ray.exceptions.ActorUnschedulableError as e:
                logger.info("Group %s is not schedulable during initialization: %s", group_id, e.error_msg)
                self.__restart_group(group_id)
            except ray.exceptions.RayActorError as e:
                logger.info("Group %s died during initialization: %s", group_id, e.error_msg)
                self.__restart_group(group_id)

    def __free_group_resources(self, group_id):
        """
        Closes the jobs currently processed by the group and discards the nimbers
        to be shared with the group.
        """
        for assigned_job in self._groups_info[group_id].assigned_jobs:
            self._tree_manager.close_job(assigned_job)
            self._closed_jobs += 1

        self._result_refs = {result_ref: g_id for result_ref, g_id in self._result_refs.items() if g_id != group_id}
        self._init_refs = {init_ref: g_id for init_ref, g_id in self._init_refs.items() if g_id != group_id}
        self._pending_nimbers[group_id].clear()

    def __init_group(self, group_id):
        """
        Initializes a group, which potentially downloads and loads the nimber database.
        """
        init_ref = self._groups[group_id].init.remote()
        self._init_refs[init_ref] = group_id

    def __init_groups(self):
        """
        Initializes all the groups.
        """
        for group_id in range(len(self._groups)):
            self.__init_group(group_id)

    def __restart_group(self, group_id):
        """
        Restarts a group with the given id. Its resources are freed and its state is restarted.
        """
        self.__free_group_resources(group_id)

        ray.kill(self._groups[group_id])
        self._groups[group_id] = WorkerGroup.options(
            num_cpus=2 * self._worker_params.grouping * max(1, self._worker_params.threads), num_gpus=0
        ).remote(self._worker_params, group_id)
        self._groups_info[group_id].restart()
        self.__init_group(group_id)

        logger.info("Group %s restarted.", group_id)

    def __prepare_jobs(self):
        """
        Prepares jobs to be assigned to available workers in groups.
        """
        start = time.time()

        available_workers = 0
        for info in self._groups_info:
            available_workers += info.available_workers()

        prepared_jobs = []
        while len(prepared_jobs) < available_workers:
            job = self._tree_manager.get_job()
            if job is None:
                break

            prepared_jobs.append(job)
            self._assigned_jobs += 1

        self._running_times.assign_time += time.time() - start
        return prepared_jobs

    def __assign_jobs_to_group(self, group_id, chosen_jobs, job_cycles):
        """
        Assigns jobs to a group with the given id. The nimbers to be shared
        are also sent to the group.

        Args:
            group_id (int): An id of the group to be assigned the jobs.
            chosen_jobs (list): The list of jobs to be assigned.
            job_cycles (list): The current cycles of the jobs to be assigned.
        """

        # assign additional work if not enough of jobs were chosen
        while len(chosen_jobs) < self._groups_info[group_id].available_workers():
            job = self._tree_manager.get_job()
            if job is None:
                break

            chosen_jobs.append(job)
            job_cycles.append(0)
            self._assigned_jobs += 1

        group_nimbers = self._pending_nimbers[group_id]
        self._groups_info[group_id].assign_jobs(chosen_jobs)
        result_ref = self._groups[group_id].complete_jobs.remote(chosen_jobs, job_cycles, self._max_iterations, group_nimbers)
        self._result_refs[result_ref] = group_id

        group_nimbers.clear()
        logger.debug("Assigned: id=%s, jobs=%s", group_id, [job.to_string() for job in chosen_jobs])

    def __assign_jobs(self, jobs):
        """
        Assigns jobs to all the groups with available workers.
        The jobs are assigned with initial cycle.

        Args:
            jobs (list): The list of jobs to be assigned.
        """
        start = time.time()

        for group_id, info in enumerate(self._groups_info):
            chosen_jobs = []
            for _ in range(info.available_workers()):
                if len(jobs) > 0:
                    chosen_jobs.append(jobs.pop())

            if info.is_assignable(chosen_jobs):
                self.__assign_jobs_to_group(group_id, chosen_jobs, job_cycles=[0 for _ in range(len(chosen_jobs))])

        self._running_times.assign_time += time.time() - start

    def __is_final_result(self, completed_job, cycle):
        """
        Checks if the given completed job is the final result or if it should be repeated.

        Args:
            completed_job (Job): The completed job.
            cycle (int): The current cycle of the job.
        """
        return self._tree_manager.is_locked() or cycle >= self._max_cycles or completed_job.is_proved()

    def __collect_results(self):
        """
        Collects the results from the groups whose workers completed some jobs.

        Returns:
            tuple: A tuple containing the list of results and the list of group IDs
            who returned them.
        """
        if len(self._result_refs) == 0:
            return [], []

        ready_result_refs, _ = ray.wait(list(self._result_refs.keys()), num_returns=1, timeout=0)

        results, ids = [], []
        for result_ref in ready_result_refs:
            group_id = self._result_refs[result_ref]
            del self._result_refs[result_ref]
            try:
                result = ray.get(result_ref)
                results.append(result)
                ids.append(group_id)

                completed_jobs, job_cycles, _ = result
                self._groups_info[group_id].deassign_jobs(completed_jobs)
                logger.debug("Deassigned: id=%s, jobs=%s", group_id, [job.to_string() for job in completed_jobs])

                jobs_to_repeat, repeated_jobs_cycles = [], []
                for completed_job, cycle in zip(completed_jobs, job_cycles):
                    if not self.__is_final_result(completed_job, cycle):
                        jobs_to_repeat.append(completed_job.get_assignment())
                        repeated_jobs_cycles.append(cycle)

                if jobs_to_repeat:
                    self.__assign_jobs_to_group(group_id, jobs_to_repeat, repeated_jobs_cycles)

            except ray.exceptions.ActorUnschedulableError as e:
                logger.info("Group %s is not schedulable: %s", group_id, e.error_msg)
                self.__restart_group(group_id)

            except ray.exceptions.RayActorError as e:
                logger.info("Group %s died: %s", group_id, e.error_msg)
                self.__restart_group(group_id)

        return results, ids

    def __submit_jobs(self, results):
        """
        Submits completed jobs by workers to the master tree.

        Args:
            results (list): The list of results, each of them being a triple of a list completed_jobs,
            its current cycles, and shared nimbers.
        """
        start = time.time()

        for result in results:
            completed_jobs, cycles, _ = result
            for completed_job, cycle in zip(completed_jobs, cycles):
                if self.__is_final_result(completed_job, cycle):
                    new_nimbers = self._tree_manager.submit_job(completed_job)
                    self._submitted_jobs += 1
                    self._updated_jobs += 1
                    if not self._no_sharing and new_nimbers.size() > 0:
                        for p in self._pending_nimbers:
                            p.merge(new_nimbers)
                else:
                    self._tree_manager.update_job(completed_job)  # update proofNumbers only
                    self._updated_jobs += 1

        self._running_times.submit_time += time.time() - start

    def __add_pending_nimbers(self, results, ids):
        """
        Stores the nimbers received from completed jobs to be shared with other groups.

        Args:
            results (list): The list of results, each containing the completed jobs, their current cycles, and new nimbers.
            ids (list): The list of group IDs corresponding to the results.
        """
        start = time.time()

        for (_, _, new_nimbers), group_id in zip(results, ids):
            if new_nimbers.size() == 0 or self._groups_info[group_id].is_being_initialized():
                continue

            self._received_nimbers += self._tree_manager.add_nimbers(new_nimbers)
            for other_id in range(len(self._pending_nimbers)):
                if group_id != other_id:
                    self._pending_nimbers[other_id].merge(new_nimbers)

        self._running_times.store_time += time.time() - start

    def __upload_database(self, output_database_path, upload_script_path):
        """
        Uploads the nimber database using the upload script.

        Args:
            output_database_path (str): The path to the output database.
            upload_script_path (str): The path to the upload script.
        """
        # make sure that no worker is being initialised for a mut. exc. on the database
        logger.info("Synchronizing with the groups...")

        for group_id in range(len(self._groups)):
            group = self._groups[group_id]
            try:
                logger.info("Synchronizing with Group %s...", group_id)
                ray.get(group.ping.remote(), timeout=60)
            except ray.exceptions.GetTimeoutError:
                # the group is either dead or is being initialized
                logger.info("Group %s is not responding during synchronization.", group_id)
                self.__restart_group(group_id)
            except ray.exceptions.RayActorError as e:
                logger.info("Group %s died during synchronization: %s", group_id, e.error_msg)
                self.__restart_group(group_id)

        logger.info("Synchornization done.")

        try:
            subprocess.run(["bash", upload_script_path, output_database_path], check=True)
            logger.info("Database %s uploaded.", output_database_path)
        except subprocess.CalledProcessError:
            logger.info("Uploading database %s failed.", output_database_path)

    def __backup_results(self, force_backup=False):
        """
        Regularly backs up the results by storing the nimbers and uploading
        the database if the uploading script is given.

        Args:
            output_database_path (str): The path to the output nimber database.
            upload_script_path (str): The path to the uploading script.
            force_backup (bool): Whether to force a backup even if regular frequency was not reached.
        """
        if self._output_database_path and (
            force_backup or (time.time() - self._time_stamps.last_backup > DistributedSolver.BACKUP_FREQ)
        ):
            start = self._time_stamps.last_backup = time.time()
            self._tree_manager.store_database(self._output_database_path)
            if self._verbose:
                logger.info("Stored %s nimbers.", self._tree_manager.nimbers())

            if self._upload_script_path:
                self.__upload_database(self._output_database_path, self._upload_script_path)

            self._running_times.backup_time += time.time() - start

    def __prune_tree(self):
        """
        Prunes the tree if necessary.
        """
        if time.time() - self._time_stamps.last_prune > DistributedSolver.PRUNE_FREQ:
            start = self._time_stamps.last_prune = time.time()
            original_tree_size = self._tree_manager.tree_size()
            pruned = self._tree_manager.prune_tree()
            if self._verbose:
                logger.info("Pruned: %s%% nodes", round(100 * (pruned / original_tree_size), 2))

            self._running_times.prune_time += time.time() - start

    def __debug_log(self):
        """
        Logs debug information about the current state of the solver.
        """
        if time.time() - self._time_stamps.last_debug_log > DistributedSolver.DEBUG_LOG_FREQ:
            self._time_stamps.last_debug_log = time.time()
            jobs = [len(group_info.assigned_jobs) for group_info in self._groups_info]
            jobs_str = [job.to_string() for group_info in self._groups_info for job in group_info.assigned_jobs]
            states = [group_info.state for group_info in self._groups_info]
            result_refs = [id for _, id in self._result_refs.items()]
            init_refs = [id for _, id in self._init_refs.items()]

            logger.debug("JOBS_STR: %s", str(jobs_str))
            logger.debug("JOBS: %s", str(jobs))
            logger.debug("STATES: %s", str(states))
            logger.debug("RESULT_REFS: %s", str(result_refs))
            logger.debug("INIT_REFS: %s", str(init_refs))
            logger.debug("LOCKED: %s\n", self._tree_manager.locked())

    def __wait_groups(self):
        for group in self._groups:
            try:
                ray.get(group.ping.remote(), timeout=60)
            except ray.exceptions.GetTimeoutError:
                logger.info("Group %s is not responding during synchronization.", group.get_id())
                self.__restart_group(group.get_id())
            except ray.exceptions.RayActorError as e:
                logger.info("Group %s died during synchronization: %s", group.get_id(), e.error_msg)
                self.__restart_group(group.get_id())

    def solve(self, position, nimber=0):
        """
        Solves the given position using parallel processing across many workers.

        Args:
            position (str): The position to be solved.
            output_database_path (str): The path to the output database.
            upload_script_path (str): The path to the upload script.
            csv_path (str, optional): Path to the CSV file for logging stats.

        Returns:
            dict: The statistics of the solved position.
        """
        init_nimbers = self._tree_manager.init_tree(
            position, nimber, len(self._groups) * self._worker_params.grouping * DistributedSolver.INIT_NODES_PER_WORKER
        )
        self._time_stamps.reset()
        self._running_times.reset()

        self._groups_info = [self.GroupState(self._worker_params.grouping) for _ in range(len(self._groups))]
        self._result_refs = {}
        self._pending_nimbers = [spots._cpp.ComputedNimbers(init_nimbers) for _ in range(len(self._groups))]

        self.__wait_groups()

        start = time.time()
        ids = []
        while not self._tree_manager.is_proved():
            self.__restore_groups()

            prepared_jobs = self.__prepare_jobs()
            self.__assign_jobs(prepared_jobs)

            results, ids = self.__collect_results()

            self.__submit_jobs(results)
            if not self._no_sharing:
                self.__add_pending_nimbers(results, ids)

            # self.__backup_results()
            # self.__prune_tree()
            # self.__print_stats(position, nimber, start)
            # self.__debug_log()

        stats = self.get_stats(position, nimber, start, finished_group_ids=ids)
        self.__backup_results(force_backup=True)

        return stats

    def __print_stats(self, position, nimber, start_time, force_log=False):
        """
        Prints the statistics of the solver.
        """
        if force_log or (self._verbose and time.time() - self._time_stamps.last_stats_log > DistributedSolver.STATS_LOG_FREQ):
            self._time_stamps.last_stats_log = time.time()
            log_parallel_stats_stdout(self.get_stats(position, nimber, start_time))

    def __collect_stats(self, finished_group_ids):
        """
        Collects statistics from each worker group.
        """
        (
            workers_nodes,
            workers_iterations,
            workers_times,
            workers_utils,
            workers_nimbers,
            workers_computed_nimbers,
            workers_jobs_num,
            workers_mini_jobs_num,
        ) = [[] for _ in range(8)]
        for group_id, group in enumerate(self._groups):
            nodes, iterations, working_times, utils, jobs_num, mini_jobs_num = [
                [None] * self._worker_params.grouping for _ in range(6)
            ]
            nimbers, computed_nimbers = None, None
            try:
                timeout = 600
                pre_stats, post_stats = ray.get(group.get_last_stats.remote(), timeout=timeout)
                stats = post_stats if group_id in finished_group_ids else pre_stats

                if stats is None:
                    continue

                nodes = stats.tree_sizes
                iterations = stats.iterations
                working_times = stats.working_times
                nimbers = stats.nimbers
                received_nimbers = stats.received_nimbers
                computed_nimbers = 1 - (received_nimbers / max(1, nimbers))
                jobs_num = stats.jobs_num
                mini_jobs_num = stats.mini_jobs_num
                utils = [
                    working_time / (working_time + waiting_time) if working_time + waiting_time > 0 else None
                    for working_time, waiting_time in zip(working_times, stats.waiting_times)
                ]

            except ray.exceptions.GetTimeoutError:
                pass  # do not react
            except ray.exceptions.RayActorError as e:
                logger.info("Group %s died while collecting stats: %s", group_id, e.error_msg)
                self.__restart_group(group_id)
            finally:
                workers_nodes += nodes
                workers_iterations += iterations
                workers_jobs_num += jobs_num
                workers_mini_jobs_num += mini_jobs_num
                workers_times += working_times
                workers_utils += utils
                workers_nimbers.append(nimbers)
                workers_computed_nimbers.append(computed_nimbers)

        return (
            workers_nodes,
            workers_iterations,
            workers_times,
            workers_utils,
            workers_nimbers,
            workers_computed_nimbers,
            workers_jobs_num,
            workers_mini_jobs_num,
        )

    def get_stats(self, position, nimber, start_time, finished_group_ids=[]):
        """
        Collects and returns statistics of the solver in a dictionary.
        """
        solving_time = time.time() - start_time
        master_time = self._running_times.total_time()
        outcome = self._tree_manager.get_outcome()
        (
            workers_nodes,
            workers_iterations,
            workers_times,
            workers_utils,
            workers_nimbers,
            workers_computed_nimbers,
            jobs_num,
            mini_jobs_num,
        ) = self.__collect_stats(finished_group_ids)

        def none_aware_mean(values):
            return sum(v for v in values if v is not None) / max(1, len([v for v in values if v is not None]))

        def none_aware_sum(values):
            return sum(v for v in values if v is not None)

        stats = {
            "position": position,
            "nimber": nimber,
            "outcome": outcome,
            "datetime": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "master_tree_nodes": self._tree_manager.tree_size(),
            "master_nimbers": self._tree_manager.nimbers(),
            "master_computed_nimbers": 100 * (1 - self._received_nimbers / max(1, self._tree_manager.nimbers())),
            "jobs_assigned": self._assigned_jobs,
            "jobs_done": self._submitted_jobs,
            "jobs_closed": self._closed_jobs,
            "jobs_open": self._assigned_jobs - self._submitted_jobs - self._closed_jobs,
            "jobs_updated": self._updated_jobs,
            "master_time": master_time,
            "master_time_breakdown": {
                "init": self._running_times.init_time,
                "assign": self._running_times.assign_time,
                "submit": self._running_times.submit_time,
                "backup": self._running_times.backup_time,
                "prune": self._running_times.prune_time,
            },
            "workers_nodes_mean": none_aware_mean(workers_nodes),
            "workers_nodes": workers_nodes,
            "workers_nimbers_mean": none_aware_mean(workers_nimbers),
            "workers_nimbers": workers_nimbers,
            "workers_computed_nimbers": workers_computed_nimbers,
            "jobs_num_sum": none_aware_sum(jobs_num),
            "jobs_num": jobs_num,
            "mini_jobs_num_sum": none_aware_sum(mini_jobs_num),
            "mini_jobs_num": mini_jobs_num,
            "workers_iterations_mean": none_aware_mean(workers_iterations),
            "workers_iterations": workers_iterations,
            "workers_times_mean": none_aware_mean(workers_times),
            "workers_times": workers_times,
            "workers_utils_mean": 100 * none_aware_mean(workers_utils),
            "workers_utils": [100 * v if v is not None else None for v in workers_utils],
            "solving_time": solving_time,
            "total_iterations": none_aware_sum(workers_iterations),
            "master_time_percent": 100 * master_time / solving_time if solving_time > 0 else 0,
            "workers_time_percent": 100 * (none_aware_mean(workers_times)) / solving_time if solving_time > 0 else 0,
            "workers_utils_raw": workers_utils,
        }

        return stats

    def log_stats(self, stats, args):
        log_parallel_stats_stdout(stats, args)
        log_stats_csv(stats, args.stats_path, args)
