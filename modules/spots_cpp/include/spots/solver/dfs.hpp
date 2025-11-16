#ifndef DFS_H
#define DFS_H

#include "solver.hpp"

namespace spots
{
    /// @brief A solver based on a simple Alpha-beta pruning.
    template <typename Game>
    class DfsSolver : public Solver<Game>
    {

    public:
        DfsSolver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true) : Solver<Game>{sharedDatabase, verbose} {}
        DfsSolver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true) : Solver<Game>{database, sharedDatabase, verbose} {}
        DfsSolver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true) : Solver<Game>{std::move(database), sharedDatabase, verbose} {}

        Outcome solveCouple(const Couple<Game> &couple) override;
        size_t getMaxTreeSize() const { return maxTreeSize; }

    private:
        Outcome computeCoupleOutcome(Couple<Game> &couple);
        void computeAndMergeExtraLands(Couple<Game> &couple);
        Outcome computeSingleLandCoupleOutcome(Couple<Game> &couple);
        Nimber computeNimber(const Game &position);

        size_t currentTreeSize = 0;
        size_t maxTreeSize = 0;
    };

    template <typename Game>
    Outcome DfsSolver<Game>::solveCouple(const Couple<Game> &couple)
    {
        Couple<Game> root{couple};
        currentTreeSize = 0;
        maxTreeSize = 0;

        auto outcome = computeCoupleOutcome(root);
        if (this->logger)
            this->logger->clearLog();

        return outcome;
    }

    template <typename Game>
    Outcome DfsSolver<Game>::computeCoupleOutcome(Couple<Game> &couple)
    {
        couple.mergeComputedLands(this->getNimberDatabase());
        computeAndMergeExtraLands(couple);
        couple.mergeComputedLands(this->getNimberDatabase());
        auto outcome = couple.getOutcome();
        if (outcome != Outcome::Unknown)
            return outcome;

        return computeSingleLandCoupleOutcome(couple);
    }

    template <typename Game>
    void DfsSolver<Game>::computeAndMergeExtraLands(Couple<Game> &couple)
    {
        if (!couple.position.isMultiLand())
            return;

        auto subgames = couple.position.getSubgames();
        sort(subgames.begin(), subgames.end(), heuristics::DefaultGameComparer<Game>{});

        if (this->logger)
            this->logger->addNode();

        this->iterations++;
        Nimber mergedNimber = couple.nimber;
        for (size_t i = 0; i < subgames.size() - 1; i++)
        {
            if (this->logger)
                this->logger->updateLastNode(0, subgames.size(), true);

            mergedNimber = Nimber::mergeNimbers(mergedNimber, computeNimber(subgames[i]));
        }

        couple.position = std::move(subgames[subgames.size() - 1]);
        couple.nimber = mergedNimber;

        if (this->logger)
            this->logger->popNode();
    }

    template <typename Game>
    Outcome DfsSolver<Game>::computeSingleLandCoupleOutcome(Couple<Game> &couple)
    {
        std::vector<Couple<Game>> children;
        auto outcome = couple.computeChildren(this->getNimberDatabase(), children);
        if (outcome != Outcome::Unknown)
        {
            if (outcome == Outcome::Loss)
                this->getNimberDatabase().insert(couple.position, couple.nimber); // store nimbers

            return outcome;
        }

        if (this->logger)
            this->logger->addNode();

        this->iterations++;
        currentTreeSize += children.size();
        if (currentTreeSize > maxTreeSize)
            maxTreeSize = currentTreeSize;

        for (auto &&child : children)
        {
            if (this->logger)
            {
                this->logger->updateLastNode(0, children.size(), false);
                this->logger->log();
            }

            outcome = computeCoupleOutcome(child);
            if (outcome == Outcome::Loss)
                break;
        }

        currentTreeSize -= children.size();

        if (this->logger)
            this->logger->popNode();

        if (outcome == Outcome::Loss)
            return Outcome::Win;

        this->getNimberDatabase().insert(couple.position, couple.nimber); // store nimber
        return Outcome::Loss;
    }

    template <typename Game>
    Nimber DfsSolver<Game>::computeNimber(const Game &position)
    {
        std::optional<Nimber> storedNimber = this->getNimberDatabase().get(position);
        if (storedNimber)
            return *storedNimber;

        Nimber nimber = 0;
        while (true)
        {
            Couple couple{position, nimber};
            if (this->computeCoupleOutcome(couple) == Outcome::Loss)
                return nimber;

            ++nimber;
        }
    }
}

#endif