#ifndef SOLVER_H
#define SOLVER_H

#include "data_structures/pns_node.hpp"
#include "spots/solver/logger.hpp"

namespace spots
{
    /// @brief An abstract class defining common functions of solvers.
    template <typename Game>
    class Solver
    {
    public:
        Solver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt}, sharedNimberDatabase{sharedDatabase} {}
        Solver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt}, nimberDatabase{database}, sharedNimberDatabase{sharedDatabase} {}
        Solver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt}, nimberDatabase{std::move(database)}, sharedNimberDatabase{sharedDatabase} {}

        virtual ~Solver() {}

        virtual Outcome solveCouple(const Couple<Game> &couple) = 0;
        Outcome solvePosition(const Game &position) { return solveCouple(Couple{position, 0}); }

        const NimberDatabase<Game> *getSharedNimberDatabase() const { return sharedNimberDatabase; }
        const NimberDatabase<Game> &getLocalNimberDatabase() const { return nimberDatabase; }
        const NimberDatabase<Game> &getNimberDatabase() const { return (sharedNimberDatabase) ? *sharedNimberDatabase : nimberDatabase; }

        size_t loadNimbers(const std::string &filePath) { return getNimberDatabase().load(filePath); }
        const std::unordered_map<typename Game::Compact, Nimber> &getTrackedNimbers() const { return getNimberDatabase().getTrackedNimbers(); }
        std::unordered_map<typename Game::Compact, Nimber> getTrackedNimbers(bool clearTracked = false) { return getNimberDatabase().getTrackedNimbers(clearTracked); }
        void clearNimbers() { getNimberDatabase().clear(); }
        void clearTrackedNimbers() { getNimberDatabase().clearTracked(); }
        size_t addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers) { return getNimberDatabase().addNimbers(std::move(nimbers)); }

        size_t getIterations() { return iterations; }

    protected:
        NimberDatabase<Game> &getNimberDatabase() { return (sharedNimberDatabase) ? *sharedNimberDatabase : nimberDatabase; }

        size_t iterations = 0;
        std::optional<Logger> logger;
        std::optional<std::mt19937> rng;

    private:
        NimberDatabase<Game> nimberDatabase;
        NimberDatabase<Game> *sharedNimberDatabase;
    };

    /// @brief An abstract class defining common functions of solvers based on PNS.
    template <typename Game>
    class PnsSolver : public Solver<Game>
    {
    public:
        static constexpr size_t NO_LIMIT = 0;

        PnsSolver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : Solver<Game>{sharedDatabase, verbose, seed} {}
        PnsSolver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : Solver<Game>{database, sharedDatabase, verbose, seed} {}
        PnsSolver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, unsigned int seed = 0) : Solver<Game>{std::move(database), sharedDatabase, verbose, seed} {}

        Outcome solveCouple(const Couple<Game> &couple) override { return expandCouple(couple, NO_LIMIT).proofNumbers.toOutcome(); }
        PnsNodeExpansionInfo expandCouple(const Couple<Game> &couple, size_t maxIterations);

        virtual void clearTree() = 0;
        virtual size_t getTreeSize() = 0;

    protected:
        virtual PnsNodeExpansionInfo _expandCouple(const Couple<Game> &couple) = 0;
        bool maxIterationsReached() { return (maxIterations != NO_LIMIT && this->iterations >= maxIterations); }

        size_t maxIterations = NO_LIMIT;
    };

    template <typename Game>
    PnsNodeExpansionInfo PnsSolver<Game>::expandCouple(const Couple<Game> &couple, size_t maxIterations)
    {
        this->iterations = 0;
        this->maxIterations = maxIterations;

        return _expandCouple(couple);
    }
}

#endif