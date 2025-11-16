#ifndef PN2S_H
#define PN2S_H

#include <memory>
#include "basic_pns.hpp"
#include "dfpn.hpp"

namespace spots
{
    /// @brief A solver based on the PN2 search using dfpn on the second level.
    template <typename Game>
    class Pn2sSolver : public BasicPnsSolver<Game>
    {
    public:
        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        Pn2sSolver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : BasicPnsSolver<Game>{sharedDatabase, verbose, estimator, seed}, innerSolver{std::make_unique<DfpnSolver<Game>>(sharedDatabase, false, estimator, seed)} {}
        Pn2sSolver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : BasicPnsSolver<Game>{database, sharedDatabase, verbose, estimator, seed}, innerSolver{std::make_unique<DfpnSolver<Game>>(sharedDatabase, false, estimator, seed)} {}
        Pn2sSolver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : BasicPnsSolver<Game>{database, sharedDatabase, verbose, estimator, seed}, innerSolver{std::make_unique<DfpnSolver<Game>>(sharedDatabase, false, estimator, seed)} {}

    private:
        void expandNode(BasicPnsSolver<Game>::Node &node) override { this->tree.expand(node, innerSolver->expandCouple(node.getState(), 100)); }

        std::unique_ptr<PnsSolver<Game>> innerSolver;
    };
}

#endif