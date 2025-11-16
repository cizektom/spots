#ifndef PNS_DATABASE_H
#define PNS_DATABASE_H

#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>

#include "pns_node.hpp"
#include "bucket_table.hpp"

namespace spots
{
    /// @brief A transposition table for storing proof and disproof numbers of df-pn.
    template <typename Game, typename NodeInfo>
    class PnsDatabase
    {
    public:
        // DEFAULT_TABLE_CAPACITY
        static constexpr size_t DEFAULT_TABLE_CAPACITY = 50'000'000l;

        PnsDatabase(size_t capacity, bool threadSafe = false) : table{capacity, threadSafe} {}

        size_t size() const { return table.size(); }
        void clear() { table.clear(); }

        const BucketTable<typename Couple<Game>::Compact, NodeInfo, typename Couple<Game>::Compact::Hash> &getTable() const { return table; }

        std::optional<NodeInfo> find(const Couple<Game>::Compact &compactCouple) const;
        std::optional<NodeInfo> find(const Couple<Game> &couple) const { return find(couple.to_compact()); }

        void mark(const Couple<Game>::Compact &compactCouple, int threadId) { table.mark(compactCouple, threadId); }
        void mark(const Couple<Game> &couple, int threadId) { mark(couple.to_compact(), threadId); }
        void unmark(const Couple<Game>::Compact &compactCouple, int threadId) { table.unmark(compactCouple, threadId); }
        void unmark(const Couple<Game> &couple, int threadId) { unmark(couple.to_compact(), threadId); }

        /// @brief Returns original NodeInfo that was updated, or std::nullopt if the entry was not found.
        std::optional<NodeInfo> insert(Couple<Game>::Compact &&compactCouple, const NodeInfo &nodeInfo) { return table.insert(std::move(compactCouple), nodeInfo); }
        /// @brief Returns original NodeInfo that was updated, or std::nullopt if the entry was not found.
        std::optional<NodeInfo> insert(const Couple<Game>::Compact &compactCouple, const NodeInfo &nodeInfo) { return table.insert(compactCouple, nodeInfo); }
        /// @brief Returns original NodeInfo that was updated, or std::nullopt if the entry was not found.
        std::optional<NodeInfo> insert(const Couple<Game> &couple, const NodeInfo &nodeInfo) { return insert(couple.to_compact(), nodeInfo); }

        void setThreadSafety(bool threadSafe) { table.setThreadSafety(threadSafe); }

    private:
        Outcome getOutcome(const Couple<Game> &c, const NimberDatabase<Game> &nimberDatabase) const;
        NodeInfo computeProofNumbers(const Couple<Game> &root, const PnsDatabase<Game, NodeInfo> *computedNodes, const NimberDatabase<Game> *computedNimbers);

        BucketTable<typename Couple<Game>::Compact, NodeInfo, typename Couple<Game>::Compact::Hash> table;
    };

    template <typename Game, typename NodeInfo>
    std::optional<NodeInfo> PnsDatabase<Game, NodeInfo>::find(const Couple<Game>::Compact &compactCouple) const
    {
        auto &&entryPtr = table.find(compactCouple);
        if (entryPtr.has_value())
            return entryPtr->value;
        else
            return std::nullopt;
    }

    template <typename Game, typename NodeInfo>
    Outcome PnsDatabase<Game, NodeInfo>::getOutcome(const Couple<Game> &c, const NimberDatabase<Game> &nimberDatabase) const
    {
        Couple copy = c;
        copy.mergeComputedLands(nimberDatabase);

        auto outcome = copy.getOutcome();
        if (outcome != Outcome::Unknown)
            return outcome;

        std::optional<NodeInfo> info = find(copy);
        if (info)
        {
            if (info->proofNumbers.isWin())
                return Outcome::Win;

            if (info->proofNumbers.isLoss())
                return Outcome::Loss;
        }

        return Outcome::Unknown;
    }
}

#endif