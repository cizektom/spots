#ifndef PARALLEL_GROUP_H
#define PARALLEL_GROUP_H

#include "parallel_dfpn.hpp"

namespace spots
{
    /// @brief A class representing a group of parallel df-pn solvers sharing a single nimber database, between
    /// who the class distributes given jobs.
    template <typename Game>
    class ParallelGroup
    {
    public:
        using Job = std::pair<Couple<Game>, size_t>;
        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        ParallelGroup(
            size_t groupSize,
            size_t workersNum,
            size_t branchingDepth,
            float epsilon,
            EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(),
            size_t ttCapacity = PnsDatabase<Game, typename ParallelDfpn<Game>::StoredParallelNodeInfo>::DEFAULT_TABLE_CAPACITY,
            int stateLevel = 0,
            unsigned int seed = 0)
            : sharedNimberDatabase{true, true},
              assignedJobs(groupSize, std::nullopt),
              treeSizes(groupSize, 0),
              iterations(groupSize, 0),
              workingTimes(groupSize, 0),
              waitingTimes(groupSize, 0),
              waitingStartTimes(groupSize, std::chrono::high_resolution_clock::now()),
              lastJobs(groupSize, Job{}),
              jobsNum(groupSize, 0),
              miniJobsNum(groupSize, 0),
              stateLevel{stateLevel}
        {
            initGroup(groupSize, workersNum, branchingDepth, epsilon, estimator, ttCapacity, seed);
        }

        ParallelGroup(
            size_t groupSize,
            size_t workersNum,
            size_t branchingDepth,
            float epsilon,
            const std::string &databasePath,
            EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(),
            size_t ttCapacity = PnsDatabase<Game, typename ParallelDfpn<Game>::StoredParallelNodeInfo>::DEFAULT_TABLE_CAPACITY,
            int stateLevel = 0,
            unsigned int seed = 0)
            : sharedNimberDatabase{NimberDatabase<Game>::load(databasePath, true, true)},
              assignedJobs(groupSize, std::nullopt),
              treeSizes(groupSize, 0),
              iterations(groupSize, 0),
              workingTimes(groupSize, 0),
              waitingTimes(groupSize, 0),
              waitingStartTimes(groupSize, std::chrono::high_resolution_clock::now()),
              lastJobs(groupSize, Job{}),
              jobsNum(groupSize, 0),
              miniJobsNum(groupSize, 0),
              stateLevel{stateLevel}
        {
            initGroup(groupSize, workersNum, branchingDepth, epsilon, estimator, ttCapacity, seed);
        }

        ~ParallelGroup();

        /// @brief Assigns jobs to the parallel-dfpn solvers in the group.
        std::vector<PnsNodeExpansionInfo> expand(std::vector<Job> &&jobs);

        std::vector<size_t> getTreeSizes() const;
        std::vector<size_t> getIterations() const;
        std::vector<size_t> getMiniJobsNum() const;
        std::vector<size_t> getJobsNum() const;
        std::vector<size_t> getWorkingTimes() const;
        std::vector<size_t> getWaitingTimes() const;
        void clearNimbers() { sharedNimberDatabase.clear(); }
        size_t getNimbers() const { return sharedNimberDatabase.size(); }
        void storeDatabase(const std::string &filePath) { sharedNimberDatabase.store(filePath, false); }
        size_t addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers) { return sharedNimberDatabase.addNimbers(std::move(nimbers)); }
        size_t loadNimbers(const std::string &filePath) { return sharedNimberDatabase.load(filePath); }
        std::unordered_map<typename Game::Compact, Nimber> getTrackedNimbers(bool clearTracked = false) { return sharedNimberDatabase.getTrackedNimbers(clearTracked); }

    private:
        void initGroup(size_t groupSize, size_t workers2Num, size_t branchingDepth, float epsilon, EstimatorPtr estimator, size_t ttCapacity, unsigned int seed);
        void run(size_t workerId);
        /// @brief A simplified expansion without synchronization if the group size equals 1.
        std::vector<PnsNodeExpansionInfo> standaloneExpand(std::vector<Job> &&jobs);

        NimberDatabase<Game> sharedNimberDatabase;

        mutable std::mutex mutex;
        std::condition_variable cv;
        std::atomic<bool> terminate = false;

        std::vector<Job> unassignedJobs;
        std::vector<std::optional<Job>> assignedJobs;
        std::vector<PnsNodeExpansionInfo> completedJobs;
        std::vector<std::thread> threads;

        std::vector<size_t> treeSizes;
        std::vector<size_t> iterations;
        std::vector<size_t> workingTimes;
        std::vector<size_t> waitingTimes;
        std::vector<std::chrono::_V2::system_clock::time_point> waitingStartTimes;

        std::vector<Job> lastJobs;
        std::vector<size_t> jobsNum;
        std::vector<size_t> miniJobsNum;

        size_t time = 0;

        std::vector<std::unique_ptr<PnsSolver<Game>>> expanders;       // used if groupSize > 1
        std::unique_ptr<PnsSolver<Game>> standaloneExpander = nullptr; // used if groupSize = 1
        int stateLevel;
    };

    template <typename Game>
    ParallelGroup<Game>::~ParallelGroup()
    {
        if (standaloneExpander == nullptr)
        {
            // groupSize > 1
            std::unique_lock lock{mutex};
            terminate = true;
            cv.notify_all();

            for (auto &&t : threads)
                t.join();
        }
    }

    template <typename Game>
    void ParallelGroup<Game>::initGroup(size_t groupSize, size_t workers2Num, size_t branchingDepth, float epsilon, EstimatorPtr estimator, size_t ttCapacity, unsigned int seed)
    {
        assert(groupSize >= 1);
        if (groupSize > 1)
        {
            std::unique_lock lock{mutex};
            for (size_t i = 0; i < groupSize; i++)
            {
                std::unique_ptr<PnsSolver<Game>> expander;
                if (workers2Num >= 1)
                    expander = std::make_unique<ParallelDfpn<Game>>(workers2Num, branchingDepth, epsilon, &sharedNimberDatabase, estimator, ttCapacity, seed);
                else if (stateLevel == 0)
                    expander = std::make_unique<DfpnSolver<Game>>(&sharedNimberDatabase, false, estimator, ttCapacity, seed);
                else
                    expander = std::make_unique<BasicPnsSolver<Game>>(&sharedNimberDatabase, false, estimator, seed);

                expanders.push_back(std::move(expander));
            }

            for (size_t i = 0; i < groupSize; i++)
            {
                waitingStartTimes[i] = std::chrono::high_resolution_clock::now();
                threads.push_back(std::thread{&ParallelGroup::run, this, i});
            }
        }
        else
        {
            if (workers2Num >= 1)
                standaloneExpander = std::make_unique<ParallelDfpn<Game>>(workers2Num, branchingDepth, epsilon, &sharedNimberDatabase, estimator, ttCapacity, seed);
            else if (stateLevel == 0)
                standaloneExpander = std::make_unique<DfpnSolver<Game>>(&sharedNimberDatabase, false, estimator, ttCapacity, seed);
            else
                standaloneExpander = std::make_unique<BasicPnsSolver<Game>>(&sharedNimberDatabase, false, estimator, seed);
        }
    }

    template <typename Game>
    std::vector<PnsNodeExpansionInfo> ParallelGroup<Game>::expand(std::vector<Job> &&jobs)
    {
        if (standaloneExpander)
            return standaloneExpand(std::move(jobs)); // groupSize == 1

        std::unique_lock lock{mutex};
        for (auto &&job : jobs)
        {
            bool assigned = false;
            for (size_t i = 0; i < assignedJobs.size(); i++)
            {
                if (!assignedJobs[i].has_value() && job.first == lastJobs[i].first)
                {
                    assignedJobs[i] = job;
                    assigned = true;
                    break;
                }
            }

            if (!assigned)
                unassignedJobs.push_back(std::move(job));
        }

        if (jobs.size() > 0)
            cv.notify_all();

        if (completedJobs.empty())
        {
            cv.wait(lock, [this]
                    { return !this->completedJobs.empty(); });
        }

        return std::vector<PnsNodeExpansionInfo>{std::move(completedJobs)};
    }

    template <typename Game>
    std::vector<PnsNodeExpansionInfo> ParallelGroup<Game>::standaloneExpand(std::vector<Job> &&jobs)
    {
        std::vector<PnsNodeExpansionInfo> completedJobs;
        completedJobs.reserve(jobs.size());
        for (auto &&job : jobs)
        {
            if (jobsNum[0] > 0)
                waitingTimes[0] += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - waitingStartTimes[0]).count();

            if (jobsNum[0] == 0 || job.first != lastJobs[0].first)
            {
                lastJobs[0] = job;
                jobsNum[0] += 1;

                if (stateLevel > 1)
                    standaloneExpander->clearNimbers();
                if (stateLevel > 0)
                    standaloneExpander->clearTree();
            }

            auto start = std::chrono::high_resolution_clock::now();
            auto result = standaloneExpander->expandCouple(job.first, job.second);
            auto stop = std::chrono::high_resolution_clock::now();

            completedJobs.push_back(std::move(result));
            treeSizes[0] = standaloneExpander->getTreeSize();
            iterations[0] += standaloneExpander->getIterations();
            miniJobsNum[0] += 1;
            workingTimes[0] += std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
            waitingStartTimes[0] = std::chrono::high_resolution_clock::now();
        }

        return completedJobs;
    }

    template <typename Game>
    void ParallelGroup<Game>::run(size_t workerId)
    {
        PnsSolver<Game> *expander = this->expanders[workerId].get();
        while (true)
        {
            std::unique_lock lock{mutex};
            if (terminate)
                return;

            if (!assignedJobs[workerId].has_value() || unassignedJobs.empty())
            {
                cv.wait(lock, [this, workerId]
                        { return this->assignedJobs[workerId].has_value() || this->unassignedJobs.size() > 0 || this->terminate; });

                if (terminate)
                    return;
            }

            // not terminating and there is a job to process
            if (jobsNum[workerId] > 0)
                waitingTimes[workerId] += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - waitingStartTimes[workerId]).count();

            Job job;
            if (assignedJobs[workerId].has_value())
            {
                job = *assignedJobs[workerId];
                assignedJobs[workerId] = std::nullopt;
            }
            else
            {
                job = unassignedJobs.back();
                unassignedJobs.pop_back();
            }

            if (jobsNum[workerId] == 0 || job.first != lastJobs[workerId].first)
            {
                lastJobs[workerId] = job;
                jobsNum[workerId] += 1;

                if (stateLevel > 1)
                    expander->clearNimbers();
                if (stateLevel > 0)
                    expander->clearTree();
            }
            lock.unlock();

            auto start = std::chrono::high_resolution_clock::now();
            auto result = expander->expandCouple(job.first, job.second);
            auto stop = std::chrono::high_resolution_clock::now();

            lock.lock();
            completedJobs.emplace_back(result);
            treeSizes[workerId] = expander->getTreeSize();
            iterations[workerId] += expander->getIterations();
            miniJobsNum[workerId] += 1;
            workingTimes[workerId] += std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
            waitingStartTimes[workerId] = std::chrono::high_resolution_clock::now();
            cv.notify_all();
        }
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getTreeSizes() const
    {
        std::unique_lock lock{mutex};
        return treeSizes;
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getIterations() const
    {
        std::unique_lock lock{mutex};
        return iterations;
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getJobsNum() const
    {
        std::unique_lock lock{mutex};
        return jobsNum;
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getMiniJobsNum() const
    {
        std::unique_lock lock{mutex};
        return miniJobsNum;
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getWorkingTimes() const
    {
        std::unique_lock lock{mutex};
        return workingTimes;
    }

    template <typename Game>
    std::vector<size_t> ParallelGroup<Game>::getWaitingTimes() const
    {
        std::unique_lock lock{mutex};
        return waitingTimes;
    }
}

#endif