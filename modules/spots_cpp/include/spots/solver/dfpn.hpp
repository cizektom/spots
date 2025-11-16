#ifndef DFPN_H
#define DFPN_H

#include <memory>

#include "solver.hpp"
#include "data_structures/pns_database.hpp"
namespace spots
{
    /// @brief A solver based on a sequential df-pn.
    template <typename Game>
    class DfpnSolver : public PnsSolver<Game>
    {
    protected:
        /// @brief A node whose children are directly stored inside, as only a single path is stored in df-pn.
        struct Node : public PnsNode<Game, Node>
        {
        public:
            Node(const Couple<Game> &state) : PnsNode<Game, Node>{state} {}
            Node(const Couple<Game> &state, ProofNumbers proofNumbers) : PnsNode<Game, Node>{state, proofNumbers} {}
            Node(const Couple<Game> &state, ProofNumbers proofNumbers, size_t iterations) : PnsNode<Game, Node>{state, proofNumbers, iterations} {}

            PN::value_type getSwitchingThreshold(size_t, size_t mpn2Idx) const { return this->getChildComplexity(mpn2Idx) + 1; }
        };

    public:
        struct StoredNodeInfo
        {
            StoredNodeInfo() : iterations{0} {}
            StoredNodeInfo(ProofNumbers proofNumbers) : proofNumbers{proofNumbers}, iterations{0} {}
            StoredNodeInfo(ProofNumbers proofNumbers, size_t iterations) : proofNumbers{proofNumbers}, iterations{iterations} {}

            void update(const StoredNodeInfo &other)
            {
                if (proofNumbers.isProved())
                    return; // do not overwrite proved proofNumbers

                proofNumbers = other.proofNumbers;
                iterations = std::max(iterations, other.iterations);
            }
            void mark(int) {}
            void unmark(int) {}

            bool operator<(const StoredNodeInfo &other) const { return proofNumbers.isProved() || iterations < other.iterations; }

            ProofNumbers proofNumbers;
            size_t iterations;
        };

        /// @brief Thresholds guiding df-pn by guaranteeing an MPN to occurr in the subtree of
        /// a node for which they hold.
        struct Thresholds
        {
        public:
            Thresholds() : proofTh{PN::INF}, disproofTh{PN::INF},
                           pShift{0}, dShift{0}, minTh{PN::INF} {}
            Thresholds(PN::value_type proofTh, PN::value_type disproofTh, PN::value_type pShift, PN::value_type dShift, PN::value_type minTh) : proofTh{proofTh}, disproofTh{disproofTh},
                                                                                                                                                pShift{pShift}, dShift{dShift}, minTh{minTh} {}
            template <typename NodeType>
            bool areHolding(const NodeType &node) const;
            template <typename NodeType>
            Thresholds toMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx, float epsilon = 1.0f) const;

            std::string to_string() const { return "{" + proofTh.to_string() + ", " + disproofTh.to_string() + ", " + pShift.to_string() + ", " + dShift.to_string() + ", " + minTh.to_string() + "}"; }

            PN::value_type proofTh, disproofTh;
            PN::value_type pShift, dShift, minTh;

        private:
            template <typename NodeType>
            Thresholds toLandMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx) const;
            template <typename NodeType>
            Thresholds toPlainMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx, float epsilon) const;
        };

        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        DfpnSolver(NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), size_t ttCapacity = PnsDatabase<Game, StoredNodeInfo>::DEFAULT_TABLE_CAPACITY, unsigned int seed = 0) : PnsSolver<Game>{sharedDatabase, verbose, seed}, pnsDatabase{ttCapacity}, estimator{estimator} {}
        DfpnSolver(const NimberDatabase<Game> &database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), size_t ttCapacity = PnsDatabase<Game, StoredNodeInfo>::DEFAULT_TABLE_CAPACITY, unsigned int seed = 0) : PnsSolver<Game>{database, sharedDatabase, verbose, seed}, pnsDatabase{ttCapacity}, estimator{estimator} {}
        DfpnSolver(NimberDatabase<Game> &&database, NimberDatabase<Game> *sharedDatabase = nullptr, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), size_t ttCapacity = PnsDatabase<Game, StoredNodeInfo>::DEFAULT_TABLE_CAPACITY, unsigned int seed = 0) : PnsSolver<Game>{std::move(database), sharedDatabase, verbose, seed}, pnsDatabase{ttCapacity}, estimator{estimator} {}

        const PnsDatabase<Game, StoredNodeInfo> &getPnsDatabase() { return pnsDatabase; }
        void setPnsDatabase(const PnsDatabase<Game, StoredNodeInfo> &pnsDatabase) { this->pnsDatabase = pnsDatabase; }

        void clearTree() override { pnsDatabase.clear(); }
        size_t getTreeSize() override { return maxTreeSize; }

    protected:
        PnsNodeExpansionInfo _expandCouple(const Couple<Game> &couple) override;

    private:
        size_t dfpn(Node &node, const Thresholds &thresholds);
        void updateDatabases(const Node &node);

        void checkBackup();

        /// @brief Initializes `childFactory` that creates nodes with initialized proof and disproof numbers
        /// from the `pnsDatabase`.
        Node::ChildFactory initChildFactory();

        PnsDatabase<Game, StoredNodeInfo> pnsDatabase;
        Node::ChildFactory childFactory = initChildFactory();
        EstimatorPtr estimator;

    protected:
        std::chrono::steady_clock::time_point lastBackup = std::chrono::steady_clock::now();
        std::string backupFilename = "";
        static const long int backupFreq = 24;

    private:
        size_t currentTreeSize = 0;
        size_t maxTreeSize = 0;
    };

    template <typename Game>
    template <typename NodeType>
    bool DfpnSolver<Game>::Thresholds::areHolding(const NodeType &node) const
    {
        auto [proof, disproof] = node.getInfo().proofNumbers;
        return proof < proofTh && disproof < disproofTh && std::min(proof + pShift, disproof + dShift) < minTh;
    }

    template <typename Game>
    template <typename NodeType>
    DfpnSolver<Game>::Thresholds DfpnSolver<Game>::Thresholds::toMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx, float epsilon) const
    {
        if (node.isMultiLandNode())
            return toLandMpnThresholds(node, mpnIdx, mpn2Idx);
        else
            return toPlainMpnThresholds(node, mpnIdx, mpn2Idx, epsilon);
    }

    template <typename Game>
    template <typename NodeType>
    DfpnSolver<Game>::Thresholds DfpnSolver<Game>::Thresholds::toLandMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx) const
    {
        // CHECKS:
        // ---------------------------------------------------------------------------------------
        //                                       proof < proofTh
        //                                    disproof < disproofTh
        //      min(proof + pShift, disproof + dShift) < minTh
        //
        // LANDS NODE:
        // ---------------------------------------------------------------------------------------
        // conditions:
        //                                               choice of mpn => min(mpnProof, mpnDisproof) < min(mpn2Proof, mpn2Disproof) + 1
        //                                          proof <    proofTh => min(mpnProof, mpnDisproof) + otherMins < proofTh
        //                                       disproof < disproofTh => min(mpnProof, mpnDisproof) + otherMins < disproofTh
        //         min(proof + pShift, disproof + dShift) <      minTh => min(mpnProof, mpnDisproof) + min(pShift, dShift) + otherMins  < minTh
        //
        //
        //
        // values:
        //                                 =>   mpnMinTh = min((min(mpn2Proof, mpn2Disproof) + 1), min(proofTh, disproofTh, minTh - min(pShift, dShift)) - otherMins)
        //                                      mpnPShift_new = 0
        //                                      mpnDShift_new = 0

        if (node.getChildren().size() == 1)
            return *this; // keep the same

        auto &&mpn = node.getChild(mpnIdx);
        PN::value_type switchingTh = PN::INF;
        if (mpn2Idx.has_value())
            switchingTh = node.getSwitchingThreshold(mpnIdx, mpn2Idx.value());

        auto [parentProof, parentDisproof] = node.getInfo().proofNumbers;
        auto [mpnProof, mpnDisproof] = mpn.getProofNumbers();

        PN::value_type mpnProofTh = PN::INF, mpnDisproofTh = PN::INF;
        PN::value_type mpnPShift = 0, mpnDShift = 0;
        PN::value_type mpnMinTh = std::min(switchingTh, std::min(std::min(proofTh, disproofTh), minTh - std::min(pShift, dShift)) - parentProof + std::min(mpnProof, mpnDisproof));

        return Thresholds{mpnProofTh, mpnDisproofTh, mpnPShift, mpnDShift, mpnMinTh};
    }

    template <typename Game>
    template <typename NodeType>
    DfpnSolver<Game>::Thresholds DfpnSolver<Game>::Thresholds::toPlainMpnThresholds(const NodeType &node, size_t mpnIdx, std::optional<size_t> mpn2Idx, float epsilon) const
    {
        // CHECKS:
        // ---------------------------------------------------------------------------------------
        //                                       proof < proofTh
        //                                    disproof < disproofTh
        //      min(proof + pShift, disproof + dShift) < minTh

        // PLAIN NODE:
        // ---------------------------------------------------------------------------------------
        // conditions:
        //                                             choice of mpn => mpnDisproof < mpn2Disproof + 1
        //                                        proof <    proofTh => mpnDisproof < proofTh
        //                                     disproof < disproofTh => mpnProof + otherProofs < diproofTh
        //       min(proof + pShift, disproof + dShift) <      minTh => min(mpnDisproof + pShift, mpnProof + otherProofs + dShift) < minTh

        // values:
        //                                 =>  mpnDisproofTh = min(mpn2Disproof + 1,
        //                                                         proofTh)
        //                                 =>     mpnProofTh = disproofTh - otherProofs
        //                                 =>          mpnTh = minTh
        //                                         mpnPShift = dShift + otherProofs
        //                                         mpnDShift = pShift

        auto &&mpn = node.getChild(mpnIdx);
        PN::value_type switchingTh = PN::INF;
        if (mpn2Idx.has_value())
            switchingTh = node.getSwitchingThreshold(mpnIdx, mpn2Idx.value());

        auto [parentProof, parentDisproof] = node.getProofNumbers();
        auto [mpnProof, mpnDisproof] = mpn.getProofNumbers();

        PN::value_type mpnProofTh = disproofTh - parentDisproof + mpnProof;
        PN::value_type mpnDisproofTh;

        if (epsilon > 1)
            mpnDisproofTh = std::min(proofTh, (PN::value_type)((1 + epsilon) * switchingTh.getValue()));
        else
            mpnDisproofTh = std::min(proofTh, switchingTh);

        PN::value_type mpnPShift = dShift + parentDisproof - mpnProof;
        PN::value_type mpnDShift = pShift;
        PN::value_type mpnMinTh = minTh;

        return Thresholds{mpnProofTh, mpnDisproofTh, mpnPShift, mpnDShift, mpnMinTh};
    }

    template <typename Game>
    PnsNodeExpansionInfo DfpnSolver<Game>::_expandCouple(const Couple<Game> &couple)
    {
        backupFilename = std::to_string(couple.position.getLives() / 3) + "_backup.spr";
        currentTreeSize = 0;
        maxTreeSize = 0;

        Node root{couple};
        dfpn(root, {});

        if (this->logger)
            this->logger->clearLog();

        root.expand(childFactory, this->getNimberDatabase());
        root.update(childFactory, this->getNimberDatabase());
        return root.getExpansionInfo();
    }

    template <typename Game>
    size_t DfpnSolver<Game>::dfpn(Node &node, const Thresholds &thresholds)
    {
        node.expand(childFactory, this->getNimberDatabase());
        node.update(childFactory, this->getNimberDatabase());

        size_t children_num = node.getChildren().size();
        currentTreeSize += children_num;
        if (currentTreeSize + pnsDatabase.size() > maxTreeSize)
            maxTreeSize = currentTreeSize + pnsDatabase.size();

        if (this->logger)
            this->logger->addNode();

        size_t localIterations = 1;
        this->iterations++;

        while (thresholds.areHolding(node) && !this->maxIterationsReached())
        {
            auto &&[mpnIdx, mpn2Idx] = node.getMpnIdx((this->rng) ? &*this->rng : nullptr);
            auto &&mpn = node.getChild(mpnIdx);

            if (this->logger)
            {
                this->logger->updateLastNode(mpnIdx, node.getChildren().size(), node.isMultiLandNode());
                this->logger->log();
            }

            size_t mpnIterations = dfpn(mpn, thresholds.toMpnThresholds(node, mpnIdx, mpn2Idx));

            localIterations += mpnIterations;
            node.update(childFactory, this->getNimberDatabase());
        }

        node.addIterations(localIterations);
        updateDatabases(node);
        checkBackup();

        currentTreeSize -= children_num;
        if (this->logger)
            this->logger->popNode();

        node.close();
        return localIterations;
    }

    template <typename Game>
    void DfpnSolver<Game>::updateDatabases(const Node &node)
    {
        auto &&compactCouple = node.getCompactState();
        auto &&nodeInfo = node.getInfo();

        if (nodeInfo.proofNumbers.isLoss() && !node.isMultiLandNode())
            this->getNimberDatabase().insert(compactCouple.compactPosition, compactCouple.nimber);

        pnsDatabase.insert(compactCouple, StoredNodeInfo{nodeInfo.proofNumbers, nodeInfo.iterations});
    }

    template <typename Game>
    void DfpnSolver<Game>::checkBackup()
    {
        if (std::chrono::duration_cast<std::chrono::hours>(std::chrono::steady_clock::now() - lastBackup).count() >= backupFreq)
        {
            auto backingUpStart = std::chrono::high_resolution_clock::now();
            this->getNimberDatabase().store(backupFilename);
            auto backingUpTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - backingUpStart).count();

            lastBackup = std::chrono::steady_clock::now();
            std::cout << "Backed up in " << backingUpTime << " ms" << std::endl;
        }
    }

    template <typename Game>
    DfpnSolver<Game>::Node::ChildFactory spots::DfpnSolver<Game>::initChildFactory()
    {
        return [this](PnsNode<Game, Node> *, const Couple<Game> &couple) -> Node
        {
            std::optional<StoredNodeInfo> info = this->getPnsDatabase().find(couple.to_compact());
            if (info)
                return Node{couple, info->proofNumbers, info->iterations};
            else
                return Node{couple, this->estimator->operator()(couple)};
        };
    }
}

#endif