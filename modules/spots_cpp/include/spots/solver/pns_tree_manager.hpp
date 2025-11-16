#ifndef PNS_TREE_MANAGER_H
#define PNS_TREE_MANAGER_H

#include "data_structures/pns_tree.hpp"
#include "logger.hpp"

namespace spots
{
    /// @brief A class that manages the master tree for distributed computations.
    template <typename Game>
    class PnsTreeManager
    {
    public:
        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        PnsTreeManager(bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : tree{estimator}, logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt} {}
        PnsTreeManager(const NimberDatabase<Game> &database, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : nimberDatabase{database}, tree{estimator}, logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt} {}
        PnsTreeManager(NimberDatabase<Game> &&database, bool verbose = true, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(), unsigned int seed = 0) : nimberDatabase{std::move(database)}, tree{estimator}, logger{(verbose) ? std::optional<Logger>{Logger{}} : std::nullopt}, rng{(seed > 0) ? std::optional<std::mt19937>{seed} : std::nullopt} {}

        void initTree(const Couple<Game> &root, size_t initSize = 0);
        void initTree(const Game &root, size_t initSize = 0) { initTree(Couple{root, 0}, initSize); }
        void clearNimbers() { nimberDatabase.clear(); }
        void clearTree() { tree.clear(); }

        size_t getLockedNodesNumber() const { return tree.getLockedNodesNumber(); }
        const NimberDatabase<Game> &getNimberDatabase() const { return nimberDatabase; }
        size_t loadNimbers(const std::string &filePath) { return nimberDatabase.load(filePath); }
        const std::unordered_map<typename Game::Compact, Nimber> &getTrackedNimbers() const { return nimberDatabase.getTrackedNimbers(); }
        void clearTrackedNimbers() { nimberDatabase.clearTracked(); }
        size_t addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers);
        PnsTree<Game> &getTree() { return tree; }
        size_t getIterations() const { return iterations; }
        PnsTree<Game>::Node *getRoot() { return tree.getRoot(); }
        PnsTree<Game>::Node *getNode(const typename Couple<Game>::Compact &compactCouple) { return tree.getNode(compactCouple); }
        bool isProved() const { return tree.isProved(); }

        /// @brief Returns a new job to be assigned.
        PnsTree<Game>::Node *getJob();
        /// @brief Updates the proof numbers of the given job and the paths to the root.
        /// The node will not be expanded, as it is expected to be reassigned again due to cycles.
        void updateJob(PnsTree<Game>::Node &node, ProofNumbers updatedProofNumbers);
        /// @brief Submits a completed job, which results into a potential node expansion
        /// and the update of paths to the root.
        void submitJob(PnsTree<Game>::Node &node, const PnsNodeExpansionInfo &expansionInfo);
        /// @brief Unlocks the given job to be again assignable. Useful if the job processing failed.
        void closeJob(PnsTree<Game>::Node &node);

    private:
        NimberDatabase<Game> nimberDatabase;
        PnsTree<Game> tree;

        size_t iterations = 0;
        std::optional<Logger> logger;
        std::optional<std::mt19937> rng;
    };

    template <typename Game>
    void PnsTreeManager<Game>::initTree(const Couple<Game> &root, size_t initSize)
    {
        iterations = 0;
        tree.clear();
        tree.setRoot(root);

        while (!tree.isProved() && initSize > 0 && tree.size() < initSize)
        {
            typename PnsTree<Game>::Node *mpn = tree.getMpn((this->rng) ? &*this->rng : nullptr, true, (logger) ? &*logger : nullptr);
            if (!mpn)
                break;

            tree.expand(*mpn, nimberDatabase);
            tree.updatePaths(*mpn, nimberDatabase);

            this->iterations++;
        }
    }

    template <typename Game>
    PnsTree<Game>::Node *PnsTreeManager<Game>::getJob()
    {
        typename PnsTree<Game>::Node *mpn = tree.getMpn((this->rng) ? &*this->rng : nullptr, true, (logger) ? &*logger : nullptr);
        if (!mpn)
            return nullptr;

        mpn->lock();
        tree.updatePaths(*mpn, nimberDatabase);

        return mpn;
    }

    template <typename Game>
    void PnsTreeManager<Game>::updateJob(PnsTree<Game>::Node &node, ProofNumbers updatedProofNumbers)
    {
        assert(updatedProofNumbers.isProved());
        node.setProofNumbers(updatedProofNumbers);
        tree.updatePaths(node, nimberDatabase);
    }

    template <typename Game>
    void PnsTreeManager<Game>::submitJob(PnsTree<Game>::Node &node, const PnsNodeExpansionInfo &expansionInfo)
    {
        iterations++;
        tree.expand(node, expansionInfo);
        closeJob(node);
    }

    template <typename Game>
    void PnsTreeManager<Game>::closeJob(PnsTree<Game>::Node &node)
    {
        node.unlock();
        tree.updatePaths(node, nimberDatabase);
    }

    template <typename Game>
    size_t PnsTreeManager<Game>::addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers)
    {
        // propagate nimbers
        for (auto &&[compactPosition, nimber] : nimbers)
        {
            if (nimberDatabase.get(compactPosition).has_value())
                continue;

            for (auto &&node : tree.getNodes(compactPosition))
            {
                Nimber mergedNimber = Nimber::mergeNimbers(nimber, node->getCompactState().nimber);
                if (mergedNimber.isWin())
                    node->setToWin();
                else
                    node->setToLoss();

                tree.updatePaths(*node, nimberDatabase);
            }
        }

        return nimberDatabase.addNimbers(std::move(nimbers));
    }
}
#endif
