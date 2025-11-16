#ifndef PNS_NODE_H
#define PNS_NODE_H

#include <random>
#include <functional>
#include <cassert>

#include "proof_numbers.hpp"
#include "couple.hpp"
#include "spots/solver/heuristics.hpp"

namespace spots
{
    /// @brief Information about expansion of a node, usually shared between two levels of PNS.
    struct PnsNodeExpansionInfo
    {
        using Children = std::vector<std::pair<std::string, ProofNumbers>>;
        PnsNodeExpansionInfo(const std::string &parentStr, ProofNumbers proofNumbers, Nimber mergedNimber, Children &&children) : parentStr{parentStr}, proofNumbers{proofNumbers}, mergedNimber{mergedNimber}, children{std::move(children)} {}

        std::string parentStr;
        ProofNumbers proofNumbers;
        Nimber mergedNimber;
        Children children;
    };

    /// @brief Struct representing a node of a NAND tree with nimbers. Used both for PNS tree and a path stored in df-pn.
    /// The type Child allows to specify where children of a node are stored combined with the usage of ChildFactory.
    template <typename Game, typename Child>
    struct PnsNode
    {
    public:
        /// @brief A generic factory for creating children based on their state.
        using ChildFactory = std::function<Child(PnsNode *parent, const Couple<Game> &)>;

        struct State
        {
            State(const Couple<Game> &c) : compactCouple{c.to_compact()}, lives{c.position.getLives()}, isMultiLand{c.position.isMultiLand()} {}

            Couple<Game>::Compact compactCouple;
            uint lives;
            bool isMultiLand;
        };

        struct Info
        {
            Info() : iterations{0}, locked{false} {}
            Info(ProofNumbers proofNumbers) : proofNumbers{proofNumbers}, iterations{0}, locked{false} {}
            Info(ProofNumbers proofNumbers, size_t iterations) : proofNumbers{proofNumbers}, iterations{iterations}, locked{false} {}
            Info(ProofNumbers proofNumbers, size_t iterations, bool locked) : proofNumbers{proofNumbers}, iterations{iterations}, locked{locked} {}

            ProofNumbers proofNumbers;
            size_t iterations;
            bool locked, expanded = false, overestimated = false;

            Nimber mergedNimber = 0; // used only for multi-land nodes
        };

        PnsNode(const Couple<Game> &c) : state{c}, info{{1, 1}} {}
        PnsNode(const Couple<Game> &c, ProofNumbers proofNumbers) : state{c}, info{proofNumbers} {}
        PnsNode(const Couple<Game> &c, ProofNumbers proofNumbers, size_t iterations) : state{c}, info{proofNumbers, iterations} {}
        PnsNode(const Couple<Game> &c, ProofNumbers proofNumbers, size_t iterations, bool locked) : state{c}, info{proofNumbers, iterations, locked} {}

        Couple<Game> getState() const { return Couple<Game>{state.compactCouple}; }
        const Couple<Game>::Compact &getCompactState() const { return state.compactCouple; }
        const Info &getInfo() const { return info; }
        ProofNumbers getProofNumbers() const { return info.proofNumbers; }
        /// @brief Returns disproof number for single-land node and nimber number for multi-land node.
        virtual PN::value_type getChildComplexity(size_t childIdx) const;

        Child &getChild(size_t idx);
        const Child &getChild(size_t idx) const;
        Child *getChild(const typename Game::Compact &compactChild);
        const Child *getChild(const typename Game::Compact &compactChild) const;
        const std::vector<Child> &getChildren() const { return children; }
        std::vector<Child> &getChildren() { return children; }
        PnsNodeExpansionInfo getExpansionInfo();

        bool isMultiLandNode() const { return state.isMultiLand; }
        bool isExpanded() const { return info.expanded; }
        bool isProved() const { return info.proofNumbers.isLoss() || info.proofNumbers.isWin(); }
        bool hasUpdated(const Info &previousInfo) { return info.proofNumbers.proof != previousInfo.proofNumbers.proof || info.proofNumbers.disproof != previousInfo.proofNumbers.disproof || info.locked != previousInfo.locked; }
        bool isLocked() const { return info.locked; }

        /// @brief Expands the node using given children factory, nimber database, and the pre-computed set of children.
        void expand(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase, const std::vector<Couple<Game>> &children) { _expand(factory, nimberDatabase, &children); }
        /// @brief Expands the node using given children factory and nimber database.
        void expand(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase) { _expand(factory, nimberDatabase, nullptr); }
        /// @brief Expands the node using the pre-computed set of children and corresponding nimber value in the Nim part of the couple.
        void expand(std::vector<Child> &&children, Nimber mergedNimber);
        /// @brief  Clears all the children.
        void close();

        void setToWin();
        void setToLoss();
        void setProofNumbers(ProofNumbers proofNumbers);
        /// @brief Updates the set of children based on currently computed results
        /// and propagates the information from them to update the state of this node.
        void update(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase);
        void lock() { info.locked = true; }
        void unlock() { info.locked = false; }
        void addIterations(size_t newIterations) { info.iterations += newIterations; }

        /// @brief Chooses the next node on the path to MPN.
        /// @param landSwitching If true allows to choose the land with the lowest nimber number.
        std::pair<size_t, std::optional<size_t>> getMpnIdx(std::mt19937 *rng, bool landSwitching = false) const;

    protected:
        /// @brief Propagates the information from children to update the state of this node
        void updateInfo();

        State state;
        Info info;
        std::vector<Child> children;

    private:
        void _expand(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase, const std::vector<Couple<Game>> *children);
        void expandLands(const ChildFactory &factory);
        void expandSingleLandChildren(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase, const std::vector<Couple<Game>> *children);

        void updateChildren(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase);
        void updateLands(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase);
        void updateSingleLandChildren();
        void updateLock();
        void updateMultiLandProofNumbers();
        void updateSingleLandProofNumbers();
    };

    template <typename Game, typename Child>
    Child &PnsNode<Game, Child>::getChild(size_t idx)
    {
        assert(idx < children.size());
        return children[idx];
    }

    template <typename Game, typename Child>
    const Child &PnsNode<Game, Child>::getChild(size_t idx) const
    {
        assert(idx < children.size());
        return children[idx];
    }

    template <typename Game, typename Child>
    Child *PnsNode<Game, Child>::getChild(const typename Game::Compact &compactChild)
    {
        for (auto &&child : children)
            if (child.getCompactState() == compactChild)
                return &child;

        return nullptr;
    }

    template <typename Game, typename Child>
    const Child *PnsNode<Game, Child>::getChild(const typename Game::Compact &compactChild) const
    {
        for (auto &&child : children)
            if (child.getCompactState() == compactChild)
                return &child;

        return nullptr;
    }

    template <typename Game, typename Child>
    PnsNodeExpansionInfo PnsNode<Game, Child>::getExpansionInfo()
    {
        PnsNodeExpansionInfo::Children children;
        children.reserve(children.size());
        for (auto &&child : this->children)
            children.emplace_back(child.getCompactState().to_string(), child.getProofNumbers());

        return PnsNodeExpansionInfo{state.compactCouple.to_string(), info.proofNumbers, info.mergedNimber, std::move(children)};
    }

    template <typename Game, typename Child>
    PN::value_type PnsNode<Game, Child>::getChildComplexity(size_t childIdx) const
    {
        auto childProofNumbers = children[childIdx].getProofNumbers();
        if (isMultiLandNode())
            return std::min(childProofNumbers.proof, childProofNumbers.disproof);
        else
            return childProofNumbers.disproof;
    }

    template <typename Game, typename Child>
    std::pair<size_t, std::optional<size_t>> PnsNode<Game, Child>::getMpnIdx(std::mt19937 *rng, bool landSwitching) const
    {
        std::vector<size_t> bestIndices;
        std::optional<size_t> mpn2Idx = std::nullopt;
        PN::value_type bestComplexity = PN::INF;

        for (size_t i = 0; i < children.size(); i++)
        {
            if (!children[i].isLocked())
            {
                PN::value_type childComplexity = getChildComplexity(i);
                if (bestIndices.empty() || childComplexity < bestComplexity)
                {
                    if (bestIndices.size() > 0)
                        mpn2Idx = bestIndices[0];

                    bestComplexity = childComplexity;
                    bestIndices = {i};
                }
                else if (childComplexity == bestComplexity)
                {
                    bestIndices.push_back(i);
                }
                else if (!mpn2Idx.has_value() || childComplexity < getChildComplexity(*mpn2Idx))
                {
                    mpn2Idx = i;
                }

                if (!landSwitching && isMultiLandNode())
                    break;
            }
        }

        assert(bestIndices.size() > 0);

        size_t mpnIdx;
        if (rng != nullptr && bestIndices.size() > 1)
        {
            std::uniform_int_distribution<size_t> dist(0, bestIndices.size() - 1);
            mpnIdx = bestIndices[dist(*rng)];
        }
        else
        {
            mpnIdx = bestIndices[0];
        }

        // Second-best index calculation (re-evaluate if tied values exist)
        if (bestIndices.size() > 1)
        {
            for (size_t idx : bestIndices)
            {
                if (idx != mpnIdx)
                {
                    mpn2Idx = idx;
                    break;
                }
            }
        }

        return {mpnIdx, mpn2Idx};
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::_expand(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase, const std::vector<Couple<Game>> *children)
    {
        assert(!info.expanded);

        info.expanded = true;
        if (isMultiLandNode())
            expandLands(factory);
        else
            expandSingleLandChildren(factory, nimberDatabase, children);
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::expand(std::vector<Child> &&children, Nimber mergedNimber)
    {
        assert(!info.expanded);

        info.expanded = true;
        this->info.mergedNimber = mergedNimber;
        this->children = std::move(children);
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::close()
    {
        info.expanded = false;
        children.clear();
        info.mergedNimber = 0;
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::expandLands(const ChildFactory &factory)
    {
        Couple<Game> state = getState();
        info.mergedNimber = state.nimber;

        auto subgames = state.position.getSubgames();
        sort(subgames.begin(), subgames.end(), heuristics::DefaultGameComparer<Game>{});

        for (auto &&subgame : subgames)
            children.push_back(factory(this, Couple<Game>{std::move(subgame), 0}));
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::expandSingleLandChildren(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase, const std::vector<Couple<Game>> *children)
    {
        std::vector<Couple<Game>> computedChildren;
        if (children == nullptr)
        {
            children = &computedChildren;
            auto outcome = getState().computeChildren(nimberDatabase, computedChildren);
            if (outcome != Outcome::Unknown)
            {
                if (outcome == Outcome::Win)
                    setToWin();
                else
                    setToLoss();

                return;
            }
        }

        for (auto &&coupleChild : *children)
            this->children.push_back(factory(this, coupleChild));
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateInfo()
    {
        if (isProved() || !isExpanded())
            return;

        updateLock();
        if (isMultiLandNode())
            updateMultiLandProofNumbers();
        else
            updateSingleLandProofNumbers();
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateLock()
    {
        for (auto &&child : children)
        {
            if (!child.isLocked())
            {
                info.locked = false;
                return;
            }
        }

        info.locked = true;
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateMultiLandProofNumbers()
    {
        if (children.size() == 1)
        {
            info.proofNumbers = children[0].getProofNumbers();
            return;
        }

        info.proofNumbers.proof = 0;
        for (size_t i = 0; i < children.size(); i++)
        {
            auto complexity = getChildComplexity(i);
            if (!info.overestimated)
                info.proofNumbers.proof += complexity;
            else
                info.proofNumbers.proof = std::max(info.proofNumbers.proof, complexity);
        }
        if (info.overestimated)
            info.proofNumbers.proof += children.size() - 1;

        info.proofNumbers.disproof = info.proofNumbers.proof;
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateSingleLandProofNumbers()
    {
        info.proofNumbers.proof = (!isLocked()) ? PN::INF : 0;
        info.proofNumbers.disproof = 0;
        for (auto &&child : children)
        {
            if (!info.overestimated)
                info.proofNumbers.disproof += child.getProofNumbers().proof;
            else
                info.proofNumbers.disproof = std::max(info.proofNumbers.disproof, child.getProofNumbers().proof);

            if (isLocked())
                info.proofNumbers.proof = std::max(info.proofNumbers.proof, child.getProofNumbers().disproof);
            else if (!child.isLocked())
                info.proofNumbers.proof = std::min(info.proofNumbers.proof, child.getProofNumbers().disproof);
        }

        if (info.overestimated)
            info.proofNumbers.disproof += children.size() - 1;
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::setToWin()
    {
        close();
        info.locked = false;
        info.proofNumbers = {0, PN::INF};
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::setToLoss()
    {
        close();
        info.locked = false;
        info.proofNumbers = {PN::INF, 0};
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::setProofNumbers(ProofNumbers proofNumbers)
    {
        if (proofNumbers.isWin())
            setToWin();
        else if (proofNumbers.isLoss())
            setToLoss();
        else
            info.proofNumbers = proofNumbers;
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::update(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase)
    {
        updateChildren(factory, nimberDatabase);
        updateInfo();
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateLands(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase)
    {
        if (children.size() > 1)
        {
            auto it = children.begin();
            while (it != children.end())
            {
                auto &&landCompactCouple = it->getCompactState();
                auto &&landProofNumbers = it->getProofNumbers();

                std::optional<Nimber> storedNimber = nimberDatabase.get(landCompactCouple.compactPosition);
                if (storedNimber)
                {
                    info.mergedNimber = Nimber::mergeNimbers(info.mergedNimber, *storedNimber);
                    it = children.erase(it);
                    continue;
                }

                if (landProofNumbers.isLoss())
                {
                    info.mergedNimber = Nimber::mergeNimbers(info.mergedNimber, landCompactCouple.nimber);
                    it = children.erase(it);
                    continue;
                }

                if (landProofNumbers.isWin())
                {
                    // this land has different nimber than in the couple part => iterate next one
                    (*it) = factory(this, Couple<Game>{landCompactCouple.compactPosition, landCompactCouple.nimber + 1});
                    continue;
                }

                ++it;
            }
        }

        if (children.size() == 1)
        {
            auto &lastChild = children[0];
            if (lastChild.getCompactState().nimber != info.mergedNimber)
                lastChild = factory(this, Couple<Game>{lastChild.getState().position, info.mergedNimber});

            if (lastChild.getProofNumbers().isWin())
                setToWin();
            else if (lastChild.getProofNumbers().isLoss())
                setToLoss();
        }
        else if (children.size() == 0)
        {
            if (info.mergedNimber.isWin())
                setToWin();
            else
                setToLoss();
        }
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateSingleLandChildren()
    {
        auto it = children.begin();
        while (it != children.end())
        {
            if (it->getProofNumbers().isLoss())
            {
                setToWin();
                return;
            }

            if (it->getProofNumbers().isWin())
            {
                it = children.erase(it);
                continue;
            }

            ++it;
        }

        if (children.empty())
            setToLoss();
    }

    template <typename Game, typename Child>
    void PnsNode<Game, Child>::updateChildren(const ChildFactory &factory, const NimberDatabase<Game> &nimberDatabase)
    {
        if (isProved() || !isExpanded())
            return;

        if (isMultiLandNode())
            updateLands(factory, nimberDatabase);
        else
            updateSingleLandChildren();
    }
}

#endif