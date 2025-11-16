#ifndef PNS_TREE_H
#define PNS_TREE_H

#include <memory>

#include "spots/solver/data_structures/pns_node.hpp"
#include "spots/solver/data_structures/pns_database.hpp"

#include "spots/solver/logger.hpp"

namespace spots
{

    /// @brief A class representing a NAND tree with nimbers for the basic variant of PNS.
    template <typename Game>
    class PnsTree
    {
    public:
        struct Node;
        /// @brief A proxy struct containing a pointer to the actual child.
        struct ChildPtr
        {
            ChildPtr(Node *parentPtr, Node *childPtr) : parentPtr{parentPtr}, childPtr{childPtr}
            {
                if (parentPtr != nullptr && childPtr != nullptr)
                    childPtr->addParent(parentPtr);
            }

            /// @brief If a proxy child is destroyed, the child is notified to remove the pointer to
            /// the parent who removed it from its list.
            ~ChildPtr() { destroy(); }
            ChildPtr(const ChildPtr &) = delete;
            ChildPtr(ChildPtr &&other) noexcept : parentPtr{other.parentPtr}, childPtr{other.childPtr} { other.clear(); }
            ChildPtr &operator=(const ChildPtr &) = delete;
            ChildPtr &operator=(ChildPtr &&other) noexcept
            {
                if (this == &other)
                    return *this;

                destroy(); // remove the old link

                // swap the ownership of the link
                parentPtr = other.parentPtr;
                childPtr = other.childPtr;
                other.clear();

                return *this;
            }

            Couple<Game> getState() const { return childPtr->getState(); }
            const Couple<Game>::Compact &getCompactState() const { return childPtr->getCompactState(); }
            const PnsNode<Game, ChildPtr>::Info &getInfo() const { return childPtr->getInfo(); }
            ProofNumbers getProofNumbers() const { return childPtr->getProofNumbers(); }
            PN::value_type getChildComplexity(size_t childIdx) const { return childPtr->getChildComplexity(childIdx); }
            Node &getNode() { return *childPtr; }
            bool isLocked() const { return childPtr->isLocked(); }

            void setParentPtr(Node *parentPtr) { this->parentPtr = parentPtr; }

            void destroy()
            {
                // remove the link
                if (parentPtr != nullptr && childPtr != nullptr)
                    childPtr->removeParent(parentPtr);
            }

            void clear()
            {
                parentPtr = nullptr;
                childPtr = nullptr;
            }

            Node *parentPtr;
            Node *childPtr;
        };

        /// @brief A node in the PNS tree that keeps track of multiple parent to be able to update all possible transpositions.
        /// Its children are only proxy instances to the nodes that are stored in a standalone storage, where they are initialized
        /// using a child factory.
        struct Node : public PnsNode<Game, ChildPtr>
        {
            friend class PnsTree;

            using Child = ChildPtr;
            Node(const Couple<Game> &couple) : PnsNode<Game, ChildPtr>{couple} {}
            Node(const Couple<Game> &couple, ProofNumbers proofNumbers) : PnsNode<Game, ChildPtr>{couple, proofNumbers} {}
            Node(const Couple<Game> &couple, ProofNumbers proofNumbers, size_t iterations) : PnsNode<Game, ChildPtr>{couple, proofNumbers, iterations} {}

            /// @brief If a node is destroyed, all children are notified to remove this node as a parent.
            ~Node() { destroy(); }
            Node(const Node &) = delete;
            Node(Node &&other) noexcept : PnsNode<Game, ChildPtr>(std::move(other)), parents{std::move(other.parents)} { other.clear(); }
            Node &operator=(const Node &) = delete;
            Node &operator=(Node &&other) noexcept
            {
                if (this == &other)
                    return *this;

                destroy(); // remove old links to parents

                PnsNode<Game, ChildPtr>::operator=(std::move((PnsNode<Game, ChildPtr> &&)other));
                updateChildrenLinks();
                parents = std::move(other.parents);

                other.clear();
                return *this;
            };

            void clear() { parents.clear(); }
            void setToOverestimated() { this->info.overestimated = true; }

            void addParent(Node *parentPtr) { parents.push_back(parentPtr); }
            const std::vector<Node *> &getParents() const { return parents; }
            void removeParent(Node *parentPtr)
            {
                auto it = std::find(parents.begin(), parents.end(), parentPtr);
                if (it != parents.end())
                    parents.erase(it);
            }

            bool operator==(const Node &other) const { return this->state.str == other.state.str; }
            bool operator!=(const Node &other) const { return this->state.str != other.state.str; }
            bool operator<(const Node &other) const
            {
                // evals to true if the `other` node can be below this node in the tree
                return this->state.lives > other.state.lives || ((this->state.lives == other.state.lives) && this->state.compactCouple.nimber.value > other.state.compactCouple.nimber.value);
            }

            PN::value_type getSwitchingThreshold(size_t, size_t mpn2Idx) const { return this->getChildComplexity(mpn2Idx) + 1; }

        private:
            void destroy()
            {
                // close parent's children
                auto parentsToClose = parents;
                for (auto &&parentPtr : parentsToClose)
                    parentPtr->close();
            }

            void updateChildrenLinks()
            {
                for (auto &&child : this->children)
                    child.setParentPtr(this);
            }

            std::vector<Node *> parents;
            bool flag = false; // used for pruning unreachable nodes in a tree
        };

        using EstimatorPtr = std::shared_ptr<heuristics::ProofNumberEstimator<Game>>;
        PnsTree(EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create()) : rootPtr{nullptr}, estimator{estimator} {}
        PnsTree(const Couple<Game> &root, EstimatorPtr estimator = heuristics::DefaultEstimator<Game>::create()) : estimator{estimator} { setRoot(root); }
        PnsTree(PnsTree &&) = delete;

        void clear();
        size_t size() const { return nodesNumber; }
        size_t getLockedNodesNumber() const;

        bool isProved() const { return rootPtr ? rootPtr->isProved() : false; }
        void setRoot(const Couple<Game> &root) { this->rootPtr = createNode(root, {}); }
        Node *getRoot() { return rootPtr; }
        Node *getNode(const Couple<Game>::Compact &compactCouple);
        std::vector<Node *> getNodes(const typename Game::Compact &compactPosition);
        const std::unordered_map<typename Game::Compact, std::unordered_map<Nimber, Node>> &getNodes() const { return nodes; }
        /// @brief Selects an MPN node.
        /// @param landSwitching If true allows to choose the land with the lowest nimber number.
        /// @param logger A logger to trace the chosen path.
        Node *getMpn(std::mt19937 *rng, bool landSwitching, Logger *logger);
        /// @brief Updates all the paths in the tree from the root to the given node.
        void updatePaths(Node &node, NimberDatabase<Game> &nimberDatabase);
        /// @brief Updates the given node based on its children.
        void update(Node &node, NimberDatabase<Game> &nimberDatabase);
        /// @brief Expands the node using the nimberdatabase.
        void expand(Node &node, NimberDatabase<Game> &nimberDatabase) { node.expand(childFactory, nimberDatabase); }
        /// @brief Expands the node using the expansion info.
        void expand(Node &node, const PnsNodeExpansionInfo &expansionInfo);

        template <typename NodeInfo>
        void updatePnsDatabase(PnsDatabase<Game, NodeInfo> &pnsDatabase);
        size_t pruneUnreachable();

    private:
        Node *createNode(const Couple<Game> &couple, ProofNumbers proofNumbers);
        Node *createNode(const Couple<Game> &couple, ProofNumbers proofNumbers, size_t iterations);
        /// @brief Initializes `childFactory` that creates proxy instances to nodes in the `nodes` database.
        Node::ChildFactory initChildFactory();

        Node *rootPtr;
        std::unordered_map<typename Game::Compact, std::unordered_map<Nimber, Node>> nodes;
        size_t nodesNumber = 0;

        Node::ChildFactory childFactory = initChildFactory();
        EstimatorPtr estimator;
    };

    template <typename Game>
    void PnsTree<Game>::clear()
    {
        nodes.clear();
        nodesNumber = 0;
        rootPtr = nullptr;
    }

    template <typename Game>
    size_t PnsTree<Game>::getLockedNodesNumber() const
    {
        size_t locked = 0;
        for (auto &&[_, nimberNodes] : nodes)
        {
            for (auto &&[_, node] : nimberNodes)
            {
                if (node.isLocked())
                    locked++;
            }
        }

        return locked;
    }

    template <typename Game>
    PnsTree<Game>::Node *PnsTree<Game>::getMpn(std::mt19937 *rng, bool landSwitching, Logger *logger)
    {
        if (rootPtr == nullptr || rootPtr->isProved() || rootPtr->isLocked())
            return nullptr;

        Node *mpn = rootPtr;
        while (mpn->isExpanded())
        {
            mpn->addIterations(1);
            size_t mpnIdx = mpn->getMpnIdx(rng, landSwitching).first;

            if (logger)
                logger->addNode(mpnIdx, mpn->getChildren().size(), mpn->isMultiLandNode());

            mpn = &mpn->getChild(mpnIdx).getNode();
        }

        if (logger)
        {
            logger->log();
            logger->clearPath();
        }

        return mpn;
    }

    template <typename Game>
    void PnsTree<Game>::updatePaths(Node &mpn, NimberDatabase<Game> &nimberDatabase)
    {
        std::unordered_set<typename Couple<Game>::Compact, typename Couple<Game>::Compact::Hash> statesToUpdate = {mpn.getCompactState()};
        std::vector<Node *> heap = {&mpn};

        auto &&heapComparator = [](Node *n1, Node *n2)
        { return *n1 < *n2; };

        while (!heap.empty())
        {
            Node *current = heap.front();
            pop_heap(heap.begin(), heap.end(), heapComparator);
            heap.pop_back();
            statesToUpdate.erase(current->getCompactState());

            typename Node::Info previousInfo = current->getInfo();
            update(*current, nimberDatabase);
            if (current->hasUpdated(previousInfo) || &mpn == current)
            {
                for (auto &&parent : current->getParents())
                {
                    if (statesToUpdate.insert(parent->getCompactState()).second)
                    {
                        heap.push_back(parent);
                        push_heap(heap.begin(), heap.end(), heapComparator);
                    }
                }
            }
        }
    }

    template <typename Game>
    void PnsTree<Game>::expand(Node &node, const PnsNodeExpansionInfo &expansionInfo)
    {
        if (expansionInfo.proofNumbers.isWin())
            node.setToWin();
        else if (expansionInfo.proofNumbers.isLoss())
            node.setToLoss();
        else
        {
            std::vector<PnsTree::ChildPtr> children;
            for (auto &&[childStr, childProofNumbers] : expansionInfo.children)
            {
                Node *childPtr = getNode(typename Couple<Game>::Compact{childStr});
                if (childPtr == nullptr)
                    childPtr = createNode(Couple<Game>{childStr}, childProofNumbers);

                children.emplace_back(&node, childPtr);
            }

            node.expand(std::move(children), expansionInfo.mergedNimber);
        }
    }

    template <typename Game>
    void PnsTree<Game>::update(Node &node, NimberDatabase<Game> &nimberDatabase)
    {
        node.update(childFactory, nimberDatabase);
        if (node.getProofNumbers().isLoss())
        {
            auto compactCouple = node.getCompactState();
            if (!node.isMultiLandNode())
                nimberDatabase.insert(compactCouple.compactPosition, compactCouple.nimber);
        }
    }

    template <typename Game>
    template <typename NodeInfo>
    void PnsTree<Game>::updatePnsDatabase(PnsDatabase<Game, NodeInfo> &pnsDatabase)
    {
        for (auto &&[compactPosition, nimberNodes] : nodes)
        {
            for (auto &&[nimber, node] : nimberNodes)
            {
                if (node.isProved() || node.isExpanded())
                    pnsDatabase.insert(typename Couple<Game>::Compact{compactPosition, nimber}, NodeInfo{node.getProofNumbers(), node.getInfo().iterations});
            }
        }
    }

    template <typename Game>
    size_t PnsTree<Game>::pruneUnreachable()
    {
        // flag reachable nodes
        std::unordered_set<Node *> frontier = {rootPtr};
        while (!frontier.empty())
        {
            auto it = frontier.begin();
            Node *current = *it;
            frontier.erase(it);

            current->flag = true;
            for (auto &&c : current->getChildren())
            {
                auto &&childNode = c.childPtr;
                if (!childNode->flag && !frontier.contains(childNode))
                    frontier.insert(childNode);
            }
        }

        // remove unreachable (= unflagged) nodes
        size_t pruned = 0;
        for (auto it1 = nodes.begin(); it1 != nodes.end();)
        {
            for (auto it2 = it1->second.begin(); it2 != it1->second.end();)
            {
                if (it2->second.flag)
                {
                    // reachable
                    it2->second.flag = false; // unflag for a future pruning
                    ++it2;
                }
                else
                {
                    // unreachable
                    it2 = it1->second.erase(it2);
                    pruned++;
                }
            }

            if (!it1->second.empty())
                ++it1;
            else
                it1 = nodes.erase(it1);
        }

        nodesNumber -= pruned;
        return pruned;
    }

    template <typename Game>
    PnsTree<Game>::Node *PnsTree<Game>::getNode(const Couple<Game>::Compact &compactCouple)
    {
        auto it1 = nodes.find(compactCouple.compactPosition);
        if (it1 == nodes.end())
            return nullptr;

        auto it2 = it1->second.find(compactCouple.nimber);
        if (it2 == it1->second.end())
            return nullptr;

        return &it2->second;
    }

    template <typename Game>
    std::vector<typename PnsTree<Game>::Node *> PnsTree<Game>::getNodes(const typename Game::Compact &compactPosition)
    {
        std::vector<PnsTree::Node *> nodePtrs;
        auto it = nodes.find(compactPosition);
        if (it != nodes.end())
        {
            for (auto &&[_, node] : it->second)
                nodePtrs.push_back(&node);
        }

        return nodePtrs;
    }

    template <typename Game>
    PnsTree<Game>::Node::ChildFactory PnsTree<Game>::initChildFactory()
    {
        return [this](PnsNode<Game, PnsTree::ChildPtr> *parentPtr, const Couple<Game> &couple) -> Node::Child
        {
            Node *castedParentPtr = static_cast<PnsTree::Node *>(parentPtr);
            Node *childPtr = this->getNode(couple.to_compact());
            if (childPtr == nullptr)
            {
                auto estimatedProofNumbers = this->estimator->operator()(couple);
                childPtr = this->createNode(couple, estimatedProofNumbers);
            }

            return typename PnsTree<Game>::Node::Child{castedParentPtr, childPtr};
        };
    }

    template <typename Game>
    PnsTree<Game>::Node *PnsTree<Game>::createNode(const Couple<Game> &couple, ProofNumbers proofNumbers)
    {
        auto compactPosition = couple.position.to_compact();
        if (!nodes.contains(compactPosition))
            nodes.insert({compactPosition, std::unordered_map<Nimber, Node>{}});

        auto &&[it, inserted] = nodes[compactPosition].insert({couple.nimber, Node{couple, proofNumbers}});
        if (inserted)
            nodesNumber++;

        return &it->second;
    }

    template <typename Game>
    PnsTree<Game>::Node *PnsTree<Game>::createNode(const Couple<Game> &couple, ProofNumbers proofNumbers, size_t iterations)
    {
        auto compactPosition = couple.position.to_compact();
        if (!nodes.contains(compactPosition))
            nodes.insert({compactPosition, std::unordered_map<Nimber, Node>{}});

        auto &&[it, inserted] = nodes[compactPosition].insert({couple.nimber, Node{couple, proofNumbers, iterations}});
        if (inserted)
            nodesNumber++;

        return &it->second;
    }
}

#endif