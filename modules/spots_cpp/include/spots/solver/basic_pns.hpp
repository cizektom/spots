#ifndef BASIC_PNS_H
#define BASIC_PNS_H

#include "solver.hpp"
#include "spots/solver/logger.hpp"
#include "data_structures/pns_tree.hpp"

namespace spots
{
    /// @brief A solver based on the basic variant of PNS.
    template <typename Game>
    class BasicPnsSolver : public PnsSolver<Game>
    {
    public:
        using Node = PnsTree<Game>::Node;
        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        BasicPnsSolver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : PnsSolver<Game>{sharedDatabase, verbose, seed}, tree{estimator} {}
        BasicPnsSolver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : PnsSolver<Game>{database, sharedDatabase, verbose, seed}, tree{estimator} {}
        BasicPnsSolver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : PnsSolver<Game>{std::move(database), sharedDatabase, verbose, seed}, tree{estimator} {}

        PnsTree<Game> &getTree() { return tree; }
        void clearTree() override { tree.clear(); }
        size_t getTreeSize() override { return tree.size(); }

    protected:
        PnsNodeExpansionInfo _expandCouple(const Couple<Game> &couple) override;
        virtual void expandNode(Node &node) { tree.expand(node, this->getNimberDatabase()); }

        PnsTree<Game> tree;
    };

    template <typename Game>
    PnsNodeExpansionInfo BasicPnsSolver<Game>::_expandCouple(const Couple<Game> &couple)
    {
        tree.setRoot(couple);
        while (!tree.isProved() && !this->maxIterationsReached())
        {
            Node *mpn = tree.getMpn((this->rng) ? &*this->rng : nullptr, false, (this->logger) ? &*this->logger : nullptr);
            expandNode(*mpn);
            tree.updatePaths(*mpn, this->getNimberDatabase());

            this->iterations++;
        }

        if (this->logger)
            this->logger->clearLog();

        return tree.getRoot()->getExpansionInfo();
    }
}

#endif