#ifndef PARALLEL_DFPN_H
#define PARALLEL_DFPN_H

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "dfpn.hpp"
#include "pns_tree_manager.hpp"
#include "data_structures/mailbox.hpp"

namespace spots
{
    /// @brief A parallel df-pn used for processing jobs by workers during distributed computation.
    template <typename Game>
    class ParallelDfpn : public PnsSolver<Game>
    {
        struct Node : public PnsNode<Game, Node>
        {
        public:
            Node(const Couple<Game> &state) : PnsNode<Game, Node>{state}, workingThreadsNum{0} {}
            Node(const Couple<Game> &state, ProofNumbers proofNumbers) : PnsNode<Game, Node>{state, proofNumbers}, workingThreadsNum{0} {}
            Node(const Couple<Game> &state, ProofNumbers proofNumbers, size_t iterations) : PnsNode<Game, Node>{state, proofNumbers, iterations}, workingThreadsNum{0} {}
            Node(const Couple<Game> &state, ProofNumbers proofNumbers, size_t iterations, size_t workingThreadsNum) : PnsNode<Game, Node>{state, proofNumbers, iterations}, workingThreadsNum{workingThreadsNum} {}

            size_t getWorkingThreadsNum() const { return workingThreadsNum; }

            PN::value_type getSwitchingThreshold(size_t mpnIdx, size_t mpn2Idx) const
            {
                PN::value_type mpn2Complexity = this->getChildComplexity(mpn2Idx);

                PN::value_type result;
                if (mpn2Complexity.is_inf())
                    result = PN::INF;
                else
                {
                    size_t mpnThreadsNum = this->children[mpnIdx].getWorkingThreadsNum();
                    // result = std::ceil(((float)mpn2Complexity.getValue() + 1.0f) / (1 + mpnThreadsNum));
                    result = mpn2Complexity + 1 - mpnThreadsNum;
                }

                return result;
            }
            PN::value_type getChildComplexity(size_t childIdx) const override
            {
                auto childProofNumbers = this->children[childIdx].getProofNumbers();
                size_t childThreadsNum = this->children[childIdx].getWorkingThreadsNum();

                if (this->isMultiLandNode())
                {
                    // return std::min(childProofNumbers.proof, childProofNumbers.disproof) * (1 + childThreadsNum);
                    return std::min(childProofNumbers.proof, childProofNumbers.disproof) + childThreadsNum;
                }
                else
                {
                    // return childProofNumbers.disproof * (1 + childThreadsNum);
                    return childProofNumbers.disproof + childThreadsNum;
                }
            }

            void updateInfo(ProofNumbers proofNumbers, size_t iterations, size_t workingThreadsNum)
            {
                this->info.proofNumbers = proofNumbers;
                this->info.iterations = iterations;
                this->workingThreadsNum = workingThreadsNum;
            }

        private:
            size_t workingThreadsNum; // number of threads working on this node
        };

        struct StoredParallelNodeInfo
        {
            StoredParallelNodeInfo() : iterations{0} {}
            StoredParallelNodeInfo(ProofNumbers proofNumbers) : proofNumbers{proofNumbers}, iterations{0} {}
            StoredParallelNodeInfo(ProofNumbers proofNumbers, size_t iterations) : proofNumbers{proofNumbers}, iterations{iterations} {}

            void update(const StoredParallelNodeInfo &other)
            {
                if (proofNumbers.isProved())
                    return; // do not overwrite proved proofNumbers

                proofNumbers = other.proofNumbers;
                iterations = std::max(iterations, other.iterations);
            }
            void mark(int threadId) { threadIds.insert(threadId); }
            void unmark(int threadId) { threadIds.erase(threadId); }

            bool operator<(const StoredParallelNodeInfo &other) const { return proofNumbers.isProved() || iterations < other.iterations; }

            ProofNumbers proofNumbers;
            size_t iterations;

            std::unordered_set<int> threadIds;
        };

        using Thresholds = DfpnSolver<Game>::Thresholds;

    public:
        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        ParallelDfpn(
            size_t workers,
            size_t branchingDepth,
            float epsilon,
            NimberDatabase<Game> *sharedDatabase = nullptr,
            EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(),
            size_t ttCapacity = PnsDatabase<Game, StoredParallelNodeInfo>::DEFAULT_TABLE_CAPACITY,
            unsigned int seed = 0)
            : PnsSolver<Game>{sharedDatabase, false, seed},
              workersNum{workers},
              branchingDepth{branchingDepth},
              epsilon{epsilon},
              pnsDatabase{ttCapacity},
              estimator{estimator},
              mailboxes(workers)
        {
            makeDatabasesThreadSafety();
            for (size_t i = 0; i < workersNum; i++)
                rngs.emplace_back(std::mt19937{seed + i});
        }

        ParallelDfpn(
            size_t workers,
            size_t branchingDepth,
            float epsilon,
            const NimberDatabase<Game> &database,
            NimberDatabase<Game> *sharedDatabase = nullptr,
            EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(),
            size_t ttCapacity = PnsDatabase<Game, StoredParallelNodeInfo>::DEFAULT_TABLE_CAPACITY,
            unsigned int seed = 0)
            : PnsSolver<Game>{database, sharedDatabase, false, seed},
              workersNum{workers},
              branchingDepth{branchingDepth},
              epsilon{epsilon},
              pnsDatabase{ttCapacity},
              estimator{estimator},
              mailboxes(workers)
        {
            makeDatabasesThreadSafety();
            for (size_t i = 0; i < workersNum; i++)
                rngs.emplace_back(std::mt19937{seed + i});
        }

        ParallelDfpn(
            size_t workers,
            size_t branchingDepth,
            float epsilon,
            NimberDatabase<Game> &&database,
            NimberDatabase<Game> *sharedDatabase = nullptr,
            EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create(),
            size_t ttCapacity = PnsDatabase<Game, StoredParallelNodeInfo>::DEFAULT_TABLE_CAPACITY,
            unsigned int seed = 0)
            : PnsSolver<Game>{std::move(database), sharedDatabase, false, seed},
              workersNum{workers},
              branchingDepth{branchingDepth},
              epsilon{epsilon},
              pnsDatabase{ttCapacity},
              estimator{estimator},
              mailboxes(workers)
        {
            makeDatabasesThreadSafety();
            for (size_t i = 0; i < workersNum; i++)
                rngs.emplace_back(std::mt19937{seed + i});
        }

        const PnsDatabase<Game, StoredParallelNodeInfo> &getPnsDatabase() { return pnsDatabase; }
        void setPnsDatabase(const PnsDatabase<Game, StoredParallelNodeInfo> &pnsDatabase) { this->pnsDatabase = pnsDatabase; }

        void clearTree() override { pnsDatabase.clear(); }
        size_t getTreeSize() override { return pnsDatabase.size(); }

    protected:
        PnsNodeExpansionInfo _expandCouple(const Couple<Game> &couple) override;

    private:
        void makeDatabasesThreadSafety();
        void updateDatabases(const Node &node, int threadId);
        /// @brief Updates the info in all children of the given node except of the one with the given index.
        void updateChildrenInfo(Node &node);

        void markNode(const Node &node, int threadId) { this->pnsDatabase.mark(node.getCompactState(), threadId); }
        void unmarkNode(const Node &node, int threadId) { this->pnsDatabase.unmark(node.getCompactState(), threadId); }
        void openNode(Node &node, int threadId)
        {
            node.addIterations(1);

            node.expand(this->childFactory, this->getNimberDatabase());
            node.update(this->childFactory, this->getNimberDatabase());
            updateDatabases(node, threadId);

            markNode(node, threadId);
        }
        void closeNode(Node &node, int threadId, bool unexpandNode)
        {
            if (unexpandNode)
                node.close();

            unmarkNode(node, threadId);
        }

        /// @brief Selects an MPN in the syncTree to be process by a thread.
        std::tuple<typename PnsTree<Game>::Node *, Thresholds, size_t, size_t> getSyncMpn();
        bool isTimeLimitReached(size_t threadIterations) { return this->maxIterations != this->NO_LIMIT && threadIterations >= this->maxIterations; }
        Node *checkMailbox(Mailbox<Game> &mailbox, std::deque<Node *> &stack);

        /// @brief A computation of an individual thread processing leaf nodes of the syncTree.
        void run(Couple<Game> root, int threadId);
        void kaneko_pdfpn(const Couple<Game> &root, int threadId);
        /// @brief Selects and processes a leaf node of the syncTree for the given maximum number of iterations.
        size_t tryRunJob(size_t maxIterations, int threadId, std::unique_lock<std::mutex> &lock);
        std::pair<size_t, Node *> dfpn(std::deque<Node *> &stack, const Thresholds &thresholds, size_t remainingIterations, int threadId, bool unexpandNode);

        /// @brief Initializes `childFactory` that creates nodes with initialized proof and disproof numbers
        /// from the `pnsDatabase`.
        Node::ChildFactory initChildFactory();
        void initSyncTree(const Couple<Game> &root);

        size_t workersNum;
        size_t branchingDepth;
        float epsilon;

        PnsDatabase<Game, StoredParallelNodeInfo> pnsDatabase;
        Node::ChildFactory childFactory = initChildFactory();
        EstimatorPtr estimator;

        mutable std::mutex mutex;
        bool computationFinished = false;
        std::condition_variable cv;
        std::atomic<bool> terminate = false;

        std::vector<Mailbox<Game>> mailboxes;
        PnsTree<Game> syncTree;

        std::vector<std::mt19937> rngs;
    };

    template <typename Game>
    void ParallelDfpn<Game>::makeDatabasesThreadSafety()
    {
        this->getNimberDatabase().setThreadSafety(true);
        this->pnsDatabase.setThreadSafety(true);
    }

    template <typename Game>
    PnsNodeExpansionInfo ParallelDfpn<Game>::_expandCouple(const Couple<Game> &root)
    {
        if (branchingDepth > 0)
            initSyncTree(root);

        computationFinished = false;
        terminate = false;
        for (auto &&mailbox : mailboxes)
            mailbox.clear();

        std::vector<std::thread> threads;
        for (size_t i = 0; i < workersNum; i++)
            threads.push_back(std::thread{&ParallelDfpn::run, this, root, i});

        for (size_t i = 0; i < workersNum; i++)
            threads[i].join();

        if (branchingDepth > 0)
        {
            // Use info in the sync tree
            return syncTree.getRoot()->getExpansionInfo();
        }
        else
        {
            // Kaneko PDFPN => use info in the pns database
            Node rootNode{root};
            rootNode.expand(this->childFactory, this->getNimberDatabase());
            rootNode.update(this->childFactory, this->getNimberDatabase());
            return rootNode.getExpansionInfo();
        }
    }

    template <typename Game>
    void ParallelDfpn<Game>::kaneko_pdfpn(const Couple<Game> &root, int threadId)
    {
        Node rootNode{root};
        std::deque<Node *> stack{&rootNode};
        size_t remainingIterations = (this->maxIterations != this->NO_LIMIT) ? this->maxIterations : std::numeric_limits<size_t>::max();
        auto &&[threadIterations, _] = dfpn(stack, {}, remainingIterations, threadId, true);
        terminate = true;

        std::lock_guard<std::mutex> lock(mutex);
        this->iterations += threadIterations;
    }

    template <typename Game>
    void ParallelDfpn<Game>::run(Couple<Game> root, int threadId)
    {
        if (branchingDepth == 0)
        {
            kaneko_pdfpn(root, threadId);
            return;
        }

        size_t threadIterations = 0;
        while (true)
        {
            std::unique_lock lock{mutex};
            if (syncTree.isProved() || computationFinished || isTimeLimitReached(threadIterations))
            {
                computationFinished = true;
                cv.notify_all();
                break;
            }

            if (syncTree.getRoot()->isLocked())
            {
                cv.wait(lock, [this]
                        { return !this->syncTree.getRoot()->isLocked() || syncTree.isProved() || this->computationFinished; });

                if (syncTree.isProved() || this->computationFinished)
                    break;
            }

            size_t remainingIterations = (this->maxIterations != this->NO_LIMIT) ? this->maxIterations - threadIterations : std::numeric_limits<size_t>::max();
            size_t localIterations = tryRunJob(remainingIterations, threadId, lock);
            threadIterations += localIterations;
            this->iterations += localIterations;
        }

        terminate = true;
    }

    template <typename Game>
    size_t ParallelDfpn<Game>::tryRunJob(size_t remainingIterations, int threadId, std::unique_lock<std::mutex> &lock)
    {
        auto &&[mpn, mpnThresholds, mpnDepth, selectionIterations] = getSyncMpn();
        if (mpn == nullptr)
            return selectionIterations;

        mpn->lock();
        syncTree.updatePaths(*mpn, this->getNimberDatabase());
        Couple<Game> mpnState = mpn->getState();
        lock.unlock();

        Node dfpnRoot{mpnState};
        std::deque<Node *> stack{&dfpnRoot};
        auto &&[mpnIterations, _] = dfpn(stack, mpnThresholds, remainingIterations, threadId, false);

        lock.lock();
        mpn->unlock();

        if (mpnDepth < branchingDepth)
            syncTree.expand(*mpn, dfpnRoot.getExpansionInfo());
        else
            mpn->setProofNumbers(dfpnRoot.getProofNumbers());

        syncTree.updatePaths(*mpn, this->getNimberDatabase());
        return mpnIterations + selectionIterations;
    }

    template <typename Game>
    std::tuple<typename PnsTree<Game>::Node *, typename ParallelDfpn<Game>::Thresholds, size_t, size_t> ParallelDfpn<Game>::getSyncMpn()
    {
        auto &&rootPtr = syncTree.getRoot();
        if (rootPtr == nullptr || rootPtr->isProved() || rootPtr->isLocked())
            return {nullptr, {}, 0, 0};

        typename PnsTree<Game>::Node *mpn = rootPtr;
        bool expandMpn = false;

        Thresholds thresholds;
        size_t depth = 0;
        size_t iterations = 0;
        while (mpn->isExpanded() || expandMpn)
        {
            if (expandMpn && !mpn->isExpanded())
            {
                Node temp{mpn->getState()};
                temp.expand(this->childFactory, this->getNimberDatabase());

                syncTree.expand(*mpn, temp.getExpansionInfo());
                syncTree.updatePaths(*mpn, this->getNimberDatabase());
                iterations += 1;
            }

            if (mpn->getChildren().size() == 0 || mpn->isLocked() || !thresholds.areHolding(*mpn))
                return {nullptr, {}, 0, iterations};

            mpn->addIterations(1);
            auto &&[mpnIdx, mpn2Idx] = mpn->getMpnIdx((this->rng) ? &*this->rng : nullptr, true);
            thresholds = thresholds.toMpnThresholds(*mpn, mpnIdx, mpn2Idx);

            expandMpn = !mpn2Idx.has_value();
            mpn = &mpn->getChild(mpnIdx).getNode();
            depth++;
        }

        return {mpn, thresholds, depth, iterations};
    }

    template <typename Game>
    std::pair<size_t, typename ParallelDfpn<Game>::Node *> ParallelDfpn<Game>::dfpn(std::deque<Node *> &stack, const Thresholds &thresholds, size_t remainingIterations, int threadId, bool unexpandNode)
    {
        if (remainingIterations <= 0)
            return {0, nullptr};

        Node &node = *stack.back();
        openNode(node, threadId);

        size_t localIterations = 1;
        while (thresholds.areHolding(node) && localIterations < remainingIterations && !terminate)
        {
            auto &&[mpnIdx, mpn2Idx] = (workersNum > 1) ? node.getMpnIdx(&rngs[threadId], true) : node.getMpnIdx((this->rng) ? &*this->rng : nullptr, false);
            auto &&mpn = node.getChild(mpnIdx);

            stack.push_back(&mpn);
            auto &&[mpnIterations, nodeToBacktrack] = dfpn(stack, thresholds.toMpnThresholds(node, mpnIdx, mpn2Idx), remainingIterations - localIterations, threadId, true);
            stack.pop_back();

            localIterations += mpnIterations;
            node.addIterations(mpnIterations);

            if (workersNum > 1)
                updateChildrenInfo(node);

            node.update(this->childFactory, this->getNimberDatabase());
            updateDatabases(node, threadId);

            if (nodeToBacktrack == nullptr)
                nodeToBacktrack = checkMailbox(mailboxes[threadId], stack);

            if (nodeToBacktrack != nullptr)
            {
                closeNode(node, threadId, unexpandNode);
                return {localIterations, (nodeToBacktrack != &node) ? nodeToBacktrack : nullptr};
            }
        }

        closeNode(node, threadId, unexpandNode);
        return {localIterations, nullptr};
    }

    template <typename Game>
    void ParallelDfpn<Game>::updateDatabases(const Node &node, int threadId)
    {
        auto &&compactCouple = node.getCompactState();
        auto &&nodeInfo = node.getInfo();

        if (nodeInfo.proofNumbers.isLoss() && !node.isMultiLandNode())
            this->getNimberDatabase().insert(compactCouple.compactPosition, compactCouple.nimber);

        auto &&originalNodeInfo = this->pnsDatabase.insert(compactCouple, StoredParallelNodeInfo{nodeInfo.proofNumbers, nodeInfo.iterations});
        if (originalNodeInfo.has_value() && !originalNodeInfo->proofNumbers.isProved() && nodeInfo.proofNumbers.isProved())
        {
            for (int computingThreadId : originalNodeInfo->threadIds)
            {
                if (computingThreadId != threadId)
                    mailboxes[computingThreadId].notify(compactCouple);
            }
        }
    }

    template <typename Game>
    ParallelDfpn<Game>::Node *ParallelDfpn<Game>::checkMailbox(Mailbox<Game> &mailbox, std::deque<Node *> &stack)
    {
        auto &&messages = mailbox.extract_all();

        Node *nodeToBacktrack = nullptr;
        for (auto &&node : stack)
        {
            auto &&state = node->getCompactState();
            if (messages.contains(state))
            {
                nodeToBacktrack = node;
                break;
            }
        }

        return nodeToBacktrack;
    }

    template <typename Game>
    void ParallelDfpn<Game>::updateChildrenInfo(Node &node)
    {
        for (auto &&child : node.getChildren())
        {
            auto &&info = this->pnsDatabase.find(child.getCompactState());
            if (info)
                child.updateInfo(info->proofNumbers, info->iterations, info->threadIds.size());
        }
    }

    template <typename Game>
    ParallelDfpn<Game>::Node::ChildFactory spots::ParallelDfpn<Game>::initChildFactory()
    {
        return [this](PnsNode<Game, Node> *, const Couple<Game> &couple) -> Node
        {
            std::optional<StoredParallelNodeInfo> info = this->getPnsDatabase().find(couple.to_compact());
            if (info)
                return Node{couple, info->proofNumbers, info->iterations, info->threadIds.size()};
            else
                return Node{couple, this->estimator->operator()(couple)};
        };
    }

    template <typename Game>
    void ParallelDfpn<Game>::initSyncTree(const Couple<Game> &root)
    {
        auto &&syncRoot = syncTree.getRoot();
        if (syncRoot != nullptr && syncRoot->getState() == root)
            return;

        syncTree.updatePnsDatabase(this->pnsDatabase);
        syncTree.clear();
        syncTree.setRoot(root);

        Node temp{root}; // temp node using pnsDatabase through childFactory
        temp.expand(this->childFactory, this->getNimberDatabase());

        syncRoot = syncTree.getRoot();
        syncTree.expand(*syncRoot, temp.getExpansionInfo());
        syncTree.update(*syncRoot, this->getNimberDatabase());
    }
}

#endif