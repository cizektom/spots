#include <chrono>

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/iostream.h>
#include <memory>

#include "spots/solver/dfs.hpp"
#include "spots/solver/dfpn.hpp"
#include "spots/solver/basic_pns.hpp"
#include "spots/solver/parallel_dfpn.hpp"
#include "spots/solver/parallel_group.hpp"
#include "spots/solver/pns_tree_manager.hpp"
#include "spots/solver/heuristics.hpp"

#include "spots/games/sprouts/position.hpp"

using namespace std;
using namespace std::chrono;

namespace py = pybind11;

struct Outcome
{
    Outcome() : outcome{spots::Outcome::Unknown} {}
    Outcome(spots::Outcome outcome) : outcome{outcome} {}

    bool isWin() const { return outcome == spots::Outcome::Win; }
    bool isLoss() const { return outcome == spots::Outcome::Loss; }
    bool isUnknown() const { return outcome == spots::Outcome::Unknown; }
    std::string to_string() const { return isWin() ? "Win" : (isLoss() ? "Loss" : "Unknown"); }

    py::tuple serialize() const
    {
        if (isWin())
            return py::make_tuple(1);
        else if (isLoss())
            return py::make_tuple(-1);
        else
            return py::make_tuple(0);
    }
    static Outcome deserialize(py::tuple t)
    {
        if (t.size() != 1)
            throw std::runtime_error("Invalid state.");

        int value = t[0].cast<int>();
        if (value == 1)
            return Outcome(spots::Outcome::Win);
        else if (value == -1)
            return Outcome(spots::Outcome::Loss);
        else if (value == 0)
            return Outcome(spots::Outcome::Unknown);
        else
            throw std::runtime_error("Invalid state.");
    }

    spots::Outcome outcome;
};

struct JobAssignment
{
    JobAssignment(const std::string &coupleStr) : coupleStr{coupleStr} {}
    JobAssignment(std::string &&coupleStr) : coupleStr{std::move(coupleStr)} {}

    std::string to_string() const { return coupleStr; }

    py::tuple serialize() const { return py::make_tuple(coupleStr); }
    static JobAssignment deserialize(py::tuple t)
    {
        if (t.size() != 1)
            throw std::runtime_error("Invalid state.");

        return JobAssignment{t[0].cast<std::string>()};
    }

    std::string coupleStr;
};

struct CompletedJob
{
    CompletedJob(const spots::PnsNodeExpansionInfo &info) : info{info} {}
    CompletedJob(spots::PnsNodeExpansionInfo &&info) : info{std::move(info)} {}

    std::string to_string() const { return info.parentStr; }
    bool isProved() { return info.proofNumbers.isProved(); }
    JobAssignment getAssignment() { return JobAssignment{info.parentStr}; }

    using SerializedChildren = std::vector<std::pair<std::string, std::pair<spots::PN::simple_value_type, spots::PN::simple_value_type>>>;
    py::tuple serialize() const
    {
        SerializedChildren children;
        children.reserve(info.children.size());
        for (auto &&child : info.children)
            children.emplace_back(child.first, child.second.getValues());

        return py::make_tuple(info.parentStr, info.proofNumbers.proof.getValue(), info.proofNumbers.disproof.getValue(), info.mergedNimber.value, std::move(children));
    }

    static CompletedJob deserialize(py::tuple t)
    {
        if (t.size() != 5)
            throw std::runtime_error("Invalid state.");

        std::string coupleStr = t[0].cast<std::string>();
        spots::ProofNumbers proofNumbers = {t[1].cast<spots::PN::simple_value_type>(), t[2].cast<spots::PN::simple_value_type>()};
        spots::Nimber mergedNimber{t[3].cast<spots::Nimber::value_type>()};

        SerializedChildren serializedChildren = t[4].cast<SerializedChildren>();
        spots::PnsNodeExpansionInfo::Children children;
        children.reserve(serializedChildren.size());
        for (auto &&serializedChild : serializedChildren)
            children.emplace_back(serializedChild.first, spots::ProofNumbers{serializedChild.second.first, serializedChild.second.second});

        return CompletedJob{spots::PnsNodeExpansionInfo{coupleStr, proofNumbers, mergedNimber, std::move(children)}};
    }

    spots::PnsNodeExpansionInfo info;
};

struct ComputedNimbers
{
    ComputedNimbers() {}
    ComputedNimbers(const std::unordered_map<std::string, spots::Nimber::value_type> &nimbers) : data{nimbers} {}
    ComputedNimbers(std::unordered_map<std::string, spots::Nimber::value_type> &&nimbers) : data{std::move(nimbers)} {}
    ComputedNimbers(const ComputedNimbers &other) : data{other.data} {}
    ComputedNimbers(ComputedNimbers &&other) : data{std::move(other.data)} {}

    size_t size() const { return data.size(); }
    void clear() { data.clear(); }
    void merge(const ComputedNimbers &other)
    {
        for (auto &&[key, value] : other.data)
            data[key] = value;
    }

    template <typename Game>
    std::unordered_map<typename Game::Compact, spots::Nimber> toCompactNimbers() const
    {
        std::unordered_map<typename Game::Compact, spots::Nimber> nimbers;
        nimbers.reserve(data.size());
        for (auto &&[positionStr, nimber] : data)
            nimbers[typename Game::Compact{positionStr}] = nimber;

        return nimbers;
    }

    template <typename Game>
    static ComputedNimbers createNimbers(const std::unordered_map<typename Game::Compact, spots::Nimber> &compactNimbers)
    {
        ComputedNimbers stringNimbers;

        stringNimbers.data.reserve(compactNimbers.size());
        for (auto &&[compactPosition, nimber] : compactNimbers)
            stringNimbers.data[compactPosition.to_string()] = nimber.value;

        return stringNimbers;
    }

    py::tuple serialize() const { return py::make_tuple(data); }
    static ComputedNimbers deserialize(py::tuple t)
    {
        if (t.size() != 1)
            throw std::runtime_error("Invalid state.");

        return ComputedNimbers{t[0].cast<std::unordered_map<std::string, spots::Nimber::value_type>>()};
    }

    std::unordered_map<std::string, spots::Nimber::value_type> data;
};

template <typename Game>
class Estimators
{
public:
    static std::shared_ptr<spots::heuristics::ProofNumberEstimator<Game>> get(bool useHeuristics)
    {
        return (useHeuristics) ? spots::heuristics::DepthEstimator<Game>::create() : spots::heuristics::DefaultEstimator<Game>::create();
    }
};

template <typename Game>
class PnsTreeManager
{
public:
    PnsTreeManager(bool verbose, bool useHeuristics, unsigned int seed) : manager{spots::NimberDatabase<Game>{true}, verbose, Estimators<Game>::get(useHeuristics), seed} {}
    PnsTreeManager(const std::string &databasePath, bool verbose, bool useHeuristics, unsigned int seed) : manager{spots::NimberDatabase<Game>::load(databasePath, true, false), verbose, Estimators<Game>::get(useHeuristics), seed} {}

    ComputedNimbers initTree(const std::string &positionStr, spots::Nimber::value_type nimber, size_t initSize)
    {
        manager.initTree(spots::Couple<Game>{Game{positionStr}, nimber}, initSize);

        ComputedNimbers nimbers = ComputedNimbers::createNimbers<Game>(manager.getTrackedNimbers());
        manager.clearTrackedNimbers();
        return nimbers;
    }
    size_t getLockedNodesNumber() const { return manager.getLockedNodesNumber(); }
    size_t pruneTree() { return manager.getTree().pruneUnreachable(); }
    bool isProved() { return manager.isProved(); }
    bool isLocked() { return manager.getRoot() ? manager.getRoot()->isLocked() : false; }
    std::pair<spots::PN::simple_value_type, spots::PN::simple_value_type> getRootProofNumbers()
    {
        auto &&[proof, disproof] = *manager.getRoot()->getProofNumbers();
        return {proof.getValue(), disproof.getValue()};
    }
    size_t getTreeSize() { return manager.getTree().size(); }
    Outcome getOutcome()
    {
        auto &&node = manager.getRoot();
        if (node == nullptr)
            return Outcome{spots::Outcome::Unknown};

        return Outcome{node->getProofNumbers().toOutcome()};
    }

    std::optional<JobAssignment> getJob()
    {
        auto &&mpn = manager.getJob();
        if (mpn)
            return JobAssignment{mpn->getCompactState().to_string()};
        else
            return {};
    }
    void updateJob(const CompletedJob &job)
    {
        auto &&node = manager.getNode(typename spots::Couple<Game>::Compact{job.info.parentStr});
        if (node)
            manager.updateJob(*node, job.info.proofNumbers);
    }
    ComputedNimbers submitJob(const CompletedJob &job)
    {
        auto &&node = manager.getNode(typename spots::Couple<Game>::Compact{job.info.parentStr});
        if (!node)
            throw logic_error("Job " + job.to_string() + " is not opened.");

        manager.submitJob(*node, job.info);
        ComputedNimbers nimbers = ComputedNimbers::createNimbers<Game>(manager.getTrackedNimbers());
        manager.clearTrackedNimbers();
        return nimbers;
    }
    void closeJob(const JobAssignment &job)
    {
        auto &&node = manager.getNode(typename spots::Couple<Game>::Compact{job.coupleStr});
        if (node)
            manager.closeJob(*node);
    }
    size_t getIterations() { return manager.getIterations(); }
    size_t getNimbers() { return manager.getNimberDatabase().size(); }
    void storeDatabase(const std::string &filePath) { manager.getNimberDatabase().store(filePath, false); }
    void clearNimbers() { manager.clearNimbers(); }
    size_t addNimbers(const ComputedNimbers &nimbers) { return manager.addNimbers(nimbers.toCompactNimbers<Game>()); }
    size_t loadNimbers(const std::string &filePath) { return manager.loadNimbers(filePath); }

private:
    spots::PnsTreeManager<Game> manager;
};

template <typename Game>
class PnsWorkersGroup
{
public:
    PnsWorkersGroup(
        size_t groupSize,
        size_t workers2Num,
        size_t depth,
        float epsilon,
        bool useHeuristics,
        size_t ttCapacity,
        int state_level,
        bool shareNimbers,
        unsigned int seed)
        : workerGroup{
              groupSize,
              workers2Num,
              depth,
              epsilon,
              Estimators<Game>::get(useHeuristics),
              ttCapacity,
              state_level,
              seed},
          shareNimbers{shareNimbers} {}

    PnsWorkersGroup(
        size_t groupSize,
        size_t workers2Num,
        size_t depth,
        float epsilon,
        const std::string &databasePath,
        bool useHeuristics,
        size_t ttCapacity,
        int state_level,
        bool shareNimbers,
        unsigned int seed)
        : workerGroup{
              groupSize,
              workers2Num,
              depth,
              epsilon,
              databasePath,
              Estimators<Game>::get(useHeuristics),
              ttCapacity,
              state_level,
              seed},
          shareNimbers{shareNimbers} {}

    std::pair<std::vector<CompletedJob>, ComputedNimbers> completeJobs(const std::vector<JobAssignment> &jobs, size_t maxIterations)
    {
        std::vector<std::pair<spots::Couple<Game>, size_t>> work;
        work.reserve(jobs.size());
        for (auto &&job : jobs)
            work.emplace_back(spots::Couple<Game>{job.coupleStr}, maxIterations);

        std::vector<spots::PnsNodeExpansionInfo> infos = workerGroup.expand(std::move(work));

        std::vector<CompletedJob> completedJobs;
        completedJobs.reserve(infos.size());
        for (auto &&info : infos)
            completedJobs.emplace_back(std::move(info));

        if (shareNimbers)
            return {completedJobs, ComputedNimbers::createNimbers<Game>(workerGroup.getTrackedNimbers(true))}; // the clear must be done within a lock to prevent race-condition
        else
            return {completedJobs, ComputedNimbers{}};
    }
    const std::vector<size_t> getIterations() const { return workerGroup.getIterations(); }
    const std::vector<size_t> getJobsNum() const { return workerGroup.getJobsNum(); }
    const std::vector<size_t> getMiniJobsNum() const { return workerGroup.getMiniJobsNum(); }
    const std::vector<size_t> getTreeSizes() const { return workerGroup.getTreeSizes(); }
    const std::vector<size_t> getWorkingTimes() const { return workerGroup.getWorkingTimes(); }
    const std::vector<size_t> getWaitingTimes() const { return workerGroup.getWaitingTimes(); }
    void clearNimbers() { workerGroup.clearNimbers(); }
    size_t getNimbers() { return workerGroup.getNimbers(); }
    void storeDatabase(const std::string &filePath) { workerGroup.storeDatabase(filePath); }
    size_t addNimbers(const ComputedNimbers &nimbers) { return workerGroup.addNimbers(nimbers.toCompactNimbers<Game>()); }
    size_t loadNimbers(const std::string &filePath) { return workerGroup.loadNimbers(filePath); }

private:
    spots::ParallelGroup<Game> workerGroup;
    bool shareNimbers;
};

template <typename Game>
class DfpnSolver
{
public:
    DfpnSolver(bool verbose, bool useHeuristics, size_t ttCapacity, unsigned int seed) : solver{spots::NimberDatabase<Game>{}, nullptr, verbose, Estimators<Game>::get(useHeuristics), ttCapacity, seed} {}
    DfpnSolver(const std::string &databasePath, bool verbose, bool useHeuristics, size_t ttCapacity, unsigned int seed) : solver{spots::NimberDatabase<Game>::load(databasePath, false, false), nullptr, verbose, Estimators<Game>::get(useHeuristics), ttCapacity, seed} {}

    Outcome solve(const std::string &position, spots::Nimber::value_type nimber) { return Outcome{solver.solveCouple(spots::Couple<Game>{Game{position}, nimber})}; }
    void clearNimbers() { solver.clearNimbers(); }
    void clearTree() { solver.clearTree(); }
    void clear()
    {
        solver.clearTree();
        solver.clearNimbers();
    }
    size_t getIterations() { return solver.getIterations(); }
    size_t getTreeSize() { return solver.getTreeSize(); }
    size_t getNimbers() { return solver.getLocalNimberDatabase().size(); }
    void storeDatabase(const std::string &filePath) { solver.getLocalNimberDatabase().store(filePath, false); }
    size_t loadNimbers(const std::string &filePath) { return solver.loadNimbers(filePath); }

private:
    spots::DfpnSolver<Game> solver;
};

template <typename Game>
class ParallelDfpnSolver
{
public:
    ParallelDfpnSolver(size_t workers, size_t branchingDepth, float epsilon, bool useHeuristics, size_t ttCapacity, unsigned int seed) : solver{workers, branchingDepth, epsilon, spots::NimberDatabase<Game>{}, nullptr, Estimators<Game>::get(useHeuristics), ttCapacity, seed} {}
    ParallelDfpnSolver(size_t workers, size_t branchingDepth, float epsilon, const std::string &databasePath, bool useHeuristics, size_t ttCapacity, unsigned int seed) : solver{workers, branchingDepth, epsilon, spots::NimberDatabase<Game>::load(databasePath, false, false), nullptr, Estimators<Game>::get(useHeuristics), ttCapacity, seed} {}

    Outcome solve(const std::string &position, spots::Nimber::value_type nimber) { return Outcome{solver.solveCouple(spots::Couple<Game>{Game{position}, nimber})}; }
    void clearNimbers() { solver.clearNimbers(); }
    void clearTree() { solver.clearTree(); }
    void clear()
    {
        solver.clearTree();
        solver.clearNimbers();
    }
    size_t getIterations() { return solver.getIterations(); }
    size_t getTreeSize() { return solver.getTreeSize(); }
    size_t getNimbers() { return solver.getLocalNimberDatabase().size(); }
    void storeDatabase(const std::string &filePath) { solver.getLocalNimberDatabase().store(filePath, false); }
    size_t loadNimbers(const std::string &filePath) { return solver.loadNimbers(filePath); }

private:
    spots::ParallelDfpn<Game> solver;
};

template <typename Game>
class PnsSolver
{
public:
    PnsSolver(bool verbose, bool useHeuristics, size_t, unsigned int seed) : solver{spots::NimberDatabase<Game>{}, nullptr, verbose, Estimators<Game>::get(useHeuristics), seed} {}
    PnsSolver(const std::string &databasePath, bool verbose, bool useHeuristics, size_t, unsigned int seed) : solver{spots::NimberDatabase<Game>::load(databasePath, false, false), nullptr, verbose, Estimators<Game>::get(useHeuristics), seed} {}

    Outcome solve(const std::string &position, spots::Nimber::value_type nimber) { return Outcome{solver.solveCouple(spots::Couple<Game>{Game{position}, nimber})}; }
    void clearNimbers() { solver.clearNimbers(); }
    void clearTree() { solver.clearTree(); }
    void clear()
    {
        solver.clearTree();
        solver.clearNimbers();
    }
    size_t getIterations() { return solver.getIterations(); }
    size_t getTreeSize() { return solver.getTreeSize(); }
    size_t getNimbers() { return solver.getLocalNimberDatabase().size(); }
    void storeDatabase(const std::string &filePath) { solver.getLocalNimberDatabase().store(filePath, false); }
    size_t loadNimbers(const std::string &filePath) { return solver.loadNimbers(filePath); }

private:
    spots::BasicPnsSolver<Game> solver;
};

template <typename Game>
class DfsSolver
{
public:
    DfsSolver(bool verbose, bool, size_t, unsigned int) : solver{spots::NimberDatabase<Game>{}, nullptr, verbose} {}
    DfsSolver(const std::string &databasePath, bool verbose, bool, size_t, unsigned int) : solver{spots::NimberDatabase<Game>::load(databasePath, false, false), nullptr, verbose} {}

    Outcome solve(const std::string &position, spots::Nimber::value_type nimber) { return Outcome{solver.solveCouple(spots::Couple<Game>{Game{position}, nimber})}; }
    void clearNimbers() { solver.clearNimbers(); }
    void clear() { solver.clearNimbers(); }
    size_t getIterations() { return solver.getIterations(); }
    size_t getTreeSize() { return solver.getMaxTreeSize(); }
    size_t getNimbers() { return solver.getLocalNimberDatabase().size(); }
    void storeDatabase(const std::string &filePath) { solver.getLocalNimberDatabase().store(filePath, false); }
    size_t loadNimbers(const std::string &filePath) { return solver.loadNimbers(filePath); }

private:
    spots::DfsSolver<Game> solver;
};

template <typename Game>
void declarePnsTreeManager(py::module &m, const std::string &typeStr)
{
    using Class = PnsTreeManager<Game>;
    std::string pyclass_name = "PnsTreeManager_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<bool, bool, unsigned int>())
        .def(py::init<const std::string &, bool, bool, unsigned int>())
        .def("tree_size", &Class::getTreeSize)
        .def("locked", &Class::getLockedNodesNumber)
        .def("init_tree", &Class::initTree)
        .def("prune_tree", &Class::pruneTree)
        .def("is_proved", &Class::isProved)
        .def("is_locked", &Class::isLocked)
        .def("root_proofs", &Class::getRootProofNumbers)
        .def("get_outcome", &Class::getOutcome)
        .def("get_job", &Class::getJob)
        .def("update_job", &Class::updateJob)
        .def("submit_job", &Class::submitJob)
        .def("close_job", &Class::closeJob)
        .def("iterations", &Class::getIterations)
        .def("nimbers", &Class::getNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("add_nimbers", &Class::addNimbers)
        .def("load_nimbers", &Class::loadNimbers)
        .def("clear_nimbers", &Class::clearNimbers);
}

template <typename Game>
void declarePnsWorkersGroup(py::module &m, const std::string &typeStr)
{
    using Class = PnsWorkersGroup<Game>;
    std::string pyclass_name = "PnsWorkersGroup_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<size_t, size_t, size_t, float, bool, size_t, int, bool, unsigned int>())
        .def(py::init<size_t, size_t, size_t, float, const std::string &, bool, size_t, int, bool, unsigned int>())
        .def("complete_jobs", &Class::completeJobs, py::call_guard<py::gil_scoped_release>())
        .def("add_nimbers", &Class::addNimbers, py::call_guard<py::gil_scoped_release>())
        .def("iterations", &Class::getIterations)
        .def("jobs_num", &Class::getJobsNum)
        .def("mini_jobs_num", &Class::getMiniJobsNum)
        .def("tree_sizes", &Class::getTreeSizes)
        .def("working_times", &Class::getWorkingTimes)
        .def("waiting_times", &Class::getWaitingTimes)
        .def("clear_nimbers", &Class::clearNimbers)
        .def("nimbers", &Class::getNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("load_nimbers", &Class::loadNimbers);
}

template <typename Game>
void declareDfpnSolver(py::module &m, const std::string &typeStr)
{
    using Class = DfpnSolver<Game>;
    std::string pyclass_name = "DfpnSolver_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<bool, bool, size_t, unsigned int>())
        .def(py::init<const std::string &, bool, bool, size_t, unsigned int>())
        .def("solve", &Class::solve)
        .def("clear_nimbers", &Class::clearNimbers)
        .def("clear_tree", &Class::clearTree)
        .def("clear", &Class::clear)
        .def("iterations", &Class::getIterations)
        .def("nimbers", &Class::getNimbers)
        .def("load_nimbers", &Class::loadNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("tree_size", &Class::getTreeSize);
}

template <typename Game>
void declareParallelDfpnSolver(py::module &m, const std::string &typeStr)
{
    using Class = ParallelDfpnSolver<Game>;
    std::string pyclass_name = "ParallelDfpnSolver_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<size_t, size_t, float, bool, size_t, unsigned int>())
        .def(py::init<size_t, size_t, float, const std::string &, bool, size_t, unsigned int>())
        .def("solve", &Class::solve)
        .def("clear_nimbers", &Class::clearNimbers)
        .def("clear_tree", &Class::clearTree)
        .def("clear", &Class::clear)
        .def("iterations", &Class::getIterations)
        .def("nimbers", &Class::getNimbers)
        .def("load_nimbers", &Class::loadNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("tree_size", &Class::getTreeSize);
}

template <typename Game>
void declarePnsSolver(py::module &m, const std::string &typeStr)
{
    using Class = PnsSolver<Game>;
    std::string pyclass_name = "PnsSolver_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<bool, bool, size_t, unsigned int>())
        .def(py::init<const std::string &, bool, bool, size_t, unsigned int>())
        .def("solve", &Class::solve)
        .def("clear_nimbers", &Class::clearNimbers)
        .def("clear_tree", &Class::clearTree)
        .def("clear", &Class::clear)
        .def("iterations", &Class::getIterations)
        .def("nimbers", &Class::getNimbers)
        .def("load_nimbers", &Class::loadNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("tree_size", &Class::getTreeSize);
}

template <typename Game>
void declareDfsSolver(py::module &m, const std::string &typeStr)
{
    using Class = DfsSolver<Game>;
    std::string pyclass_name = "DfsSolver_" + typeStr;
    py::class_<Class>(m, pyclass_name.c_str())
        .def(py::init<bool, bool, size_t, unsigned int>())
        .def(py::init<const std::string &, bool, bool, size_t, unsigned int>())
        .def("solve", &Class::solve)
        .def("clear_nimbers", &Class::clearNimbers)
        .def("clear", &Class::clear)
        .def("iterations", &Class::getIterations)
        .def("nimbers", &Class::getNimbers)
        .def("load_nimbers", &Class::loadNimbers)
        .def("store_database", &Class::storeDatabase)
        .def("tree_size", &Class::getTreeSize);
}

PYBIND11_MODULE(_cpp, m)
{
    py::class_<Outcome>(m, "Outcome")
        .def(py::init<>())
        .def("is_win", &Outcome::isWin)
        .def("is_loss", &Outcome::isLoss)
        .def("is_unknown", &Outcome::isUnknown)
        .def("to_string", &Outcome::to_string)
        .def(py::pickle(
            [](const Outcome &job)
            { return job.serialize(); },
            [](py::tuple t)
            { return Outcome::deserialize(t); }));

    py::class_<JobAssignment>(m, "JobAssignment")
        .def("to_string", &JobAssignment::to_string)
        .def(py::pickle(
            [](const JobAssignment &job)
            { return job.serialize(); },
            [](py::tuple t)
            { return JobAssignment::deserialize(t); }));

    py::class_<CompletedJob>(m, "CompletedJob")
        .def("to_string", &CompletedJob::to_string)
        .def("is_proved", &CompletedJob::isProved)
        .def("get_assignment", &CompletedJob::getAssignment)
        .def(py::pickle(
            [](const CompletedJob &job)
            { return job.serialize(); },
            [](py::tuple t)
            { return CompletedJob::deserialize(t); }));

    py::class_<ComputedNimbers>(m, "ComputedNimbers")
        .def(py::init<>())
        .def(py::init<const ComputedNimbers &>())
        .def("size", &ComputedNimbers::size)
        .def("clear", &ComputedNimbers::clear)
        .def("merge", &ComputedNimbers::merge)
        .def(py::pickle(
            [](const ComputedNimbers &job)
            { return job.serialize(); },
            [](py::tuple t)
            { return ComputedNimbers::deserialize(t); }));

    declarePnsTreeManager<sprouts::Position>(m, "Sprouts");
    declarePnsWorkersGroup<sprouts::Position>(m, "Sprouts");
    declareDfpnSolver<sprouts::Position>(m, "Sprouts");
    declareParallelDfpnSolver<sprouts::Position>(m, "Sprouts");
    declarePnsSolver<sprouts::Position>(m, "Sprouts");
    declareDfsSolver<sprouts::Position>(m, "Sprouts");

    m.doc() = "Spots C++ Module";
}