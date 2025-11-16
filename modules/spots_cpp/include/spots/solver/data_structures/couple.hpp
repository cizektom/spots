#ifndef COUPLE_H
#define COUPLE_H

#include "spots/global.hpp"
#include "spots/solver/heuristics.hpp"
#include "nimber_database.hpp"

namespace spots
{
    namespace heuristics
    {
        template <typename Game>
        struct DefaultCoupleComparer;
    }

    template <typename Game>
    struct Couple
    {
    public:
        struct Compact
        {
            Compact() = default;
            explicit Compact(const std::string &coupleStr);
            explicit Compact(const typename Game::Compact &compactPosition, Nimber nimber) : compactPosition{compactPosition}, nimber{nimber} {}
            explicit Compact(typename Game::Compact &&compactPosition, Nimber nimber) : compactPosition{std::move(compactPosition)}, nimber{nimber} {}

            std::string to_string() const { return Couple::to_string(compactPosition.to_string(), nimber.to_string()); }
            bool operator==(const Compact &other) const { return nimber == other.nimber && compactPosition == other.compactPosition; }

            typename Game::Compact compactPosition;
            Nimber nimber;

            struct Hash
            {
                std::size_t operator()(const typename Couple<Game>::Compact &c) const
                {
                    size_t seed = 0;
                    spots::utils::hash_combine(seed, c.compactPosition);
                    spots::utils::hash_combine(seed, c.nimber);

                    return seed;
                }
            };
        };

        Couple() = default;
        Couple(const Game &position, Nimber nimber) : position{position}, nimber{nimber} {}
        Couple(Game &&position, Nimber nimber) : position{std::move(position)}, nimber{nimber} {}
        Couple(const typename Game::Compact &compactPosition, Nimber nimber) : position{compactPosition}, nimber{nimber} {}
        Couple(typename Game::Compact &&compactPosition, Nimber nimber) : position{std::move(compactPosition)}, nimber{nimber} {}
        explicit Couple(const Compact &c) : position{c.compactPosition}, nimber{c.nimber} {}
        explicit Couple(Compact &&c) : position{std::move(c.compactPosition)}, nimber{c.nimber} {}
        explicit Couple(const std::string &coupleStr);

        /// @brief Computes children of the couple and inserts them into a given vector.
        /// @return Returns outcome of this couple. If it could not have been determined,
        /// returns Outcome::Unknown instead.
        Outcome computeChildren(const NimberDatabase<Game> &database, std::vector<Couple<Game>> &children) const { return computeChildren(&database, children); }
        /// @brief Computes children of the couple using computed children of the Sprouts part and inserts them into a given vector.
        /// @return Returns outcome of this couple. If it could not have been determined,
        /// returns Outcome::Unknown instead.
        Outcome computeChildren(const NimberDatabase<Game> &database, std::vector<Couple<Game>> &children, const std::vector<Game> &positionChildren) const;
        /// @brief Computes children of the couple.
        std::vector<Couple<Game>> computeChildren() const;

        /// @brief Merges subgames whose nimber is computed in the database into the nimber part.
        /// @return Returns true if modified.
        bool mergeComputedLands(const NimberDatabase<Game> &database);

        size_t estimateProofDepth() const { return position.estimateProofDepth() + nimber.value; }
        size_t estimateDisproofDepth() const { return position.estimateDisproofDepth() + nimber.value; }

        /// @brief Tries to get an immediate outcome of the couple.
        Outcome getOutcome() const
        {
            if (position.isTerminal())
            {
                if (Game::isNormalImpartial)
                    return nimber.isWin() ? Outcome::Win : Outcome::Loss;
                else
                    return position.getOutcome();
            }

            return Outcome::Unknown;
        }

        bool operator==(const Couple &other) const { return nimber == other.nimber && position == other.position; }
        bool operator!=(const Couple &other) const { return !(*this == other); }

        Compact to_compact() const { return Compact{position.to_compact(), nimber}; }

        static std::string to_string(const std::string &positionStr, const std::string &nimberStr) { return positionStr + positionNimberSeparator + nimberStr; }
        std::string to_string() const { return Couple::to_string(position.to_string(), nimber.to_string()); }
        friend std::ostream &operator<<(std::ostream &o, const Couple<Game> &c) { return o << c.to_string(); }

        Game position;
        Nimber nimber;

    private:
        static constexpr char positionNimberSeparator = ' ';

        Outcome computeChildren(const NimberDatabase<Game> *database, std::vector<Couple<Game>> &children) const;
    };

    template <typename Game>
    Couple<Game>::Compact::Compact(const std::string &coupleStr)
    {
        compactPosition = typename Game::Compact{coupleStr.substr(0, coupleStr.find(positionNimberSeparator))};
        nimber = Nimber{(Nimber::value_type)stoul(coupleStr.substr(coupleStr.find(positionNimberSeparator) + 1))};
    }

    template <typename Game>
    Couple<Game>::Couple(const std::string &coupleStr)
    {
        position = Game{coupleStr.substr(0, coupleStr.find(positionNimberSeparator))};
        nimber = Nimber{(Nimber::value_type)stoul(coupleStr.substr(coupleStr.find(positionNimberSeparator) + 1))};
    }

    template <typename Game>
    Outcome Couple<Game>::computeChildren(const NimberDatabase<Game> &database, std::vector<Couple<Game>> &children, const std::vector<Game> &positionChildren) const
    {
        children.clear();
        auto outcome = getOutcome();
        if (outcome != Outcome::Unknown)
            return outcome;

        // Add Nim children
        for (Nimber nimberChild = 0; nimberChild < nimber; ++nimberChild)
            children.push_back(Couple{position, nimberChild});

        // Add Position children
        for (auto &&positionChild : positionChildren)
        {
            if (!Game::isNormalImpartial && !positionChild.isTerminal() && database.get(positionChild))
                return Outcome::Win;

            auto coupleChild = Couple{positionChild, nimber};
            coupleChild.mergeComputedLands(database);
            if (coupleChild.position.isTerminal())
            {
                if (coupleChild.getOutcome() == Outcome::Loss)
                    return Outcome::Win;
            }
            else
                children.push_back(std::move(coupleChild));
        }

        if (children.size() == 0)
        {
            return Outcome::Loss;
        }
        else
        {
            sort(children.begin(), children.end(), heuristics::DefaultCoupleComparer<Game>{});
            return Outcome::Unknown;
        }
    }

    template <typename Game>
    Outcome Couple<Game>::computeChildren(const NimberDatabase<Game> *database, std::vector<Couple<Game>> &children) const
    {
        children.clear();
        auto outcome = getOutcome();
        if (outcome != Outcome::Unknown)
            return outcome;

        // Add Nim children
        for (Nimber nimberChild = 0; nimberChild < nimber; ++nimberChild)
            children.push_back(Couple{position, nimberChild});

        // Add Position children
        for (auto &&positionChild : position.computeChildren())
        {
            if (database && !Game::isNormalImpartial && !positionChild.isTerminal() && database->get(positionChild))
                return Outcome::Win;

            auto coupleChild = Couple{std::move(positionChild), nimber};
            if (database)
            {
                coupleChild.mergeComputedLands(*database);
                if (coupleChild.position.isTerminal())
                {
                    if (coupleChild.getOutcome() == Outcome::Loss)
                        return Outcome::Win;
                }
                else
                    children.push_back(std::move(coupleChild));
            }
            else
                children.push_back(std::move(coupleChild));
        }

        if (children.size() == 0)
        {
            return Outcome::Loss;
        }
        else
        {
            sort(children.begin(), children.end(), heuristics::DefaultCoupleComparer<Game>{});
            return Outcome::Unknown;
        }
    }

    template <typename Game>
    std::vector<Couple<Game>> Couple<Game>::computeChildren() const
    {
        std::vector<Couple<Game>> children;
        computeChildren(nullptr, children);
        return children;
    }

    template <typename Game>
    bool Couple<Game>::mergeComputedLands(const NimberDatabase<Game> &database)
    {
        if (!Game::isNormalImpartial || position.empty())
            return false;

        bool modified = false;
        std::vector<Game> uncomputedSubgames;
        uncomputedSubgames.reserve(position.getSubgamesNumber());

        for (auto &&subgame : position.getSubgames())
        {
            std::optional<Nimber> storedNimber = database.get(subgame);
            if (storedNimber)
            {
                nimber = Nimber::mergeNimbers(nimber, *storedNimber);
                modified = true;
            }
            else
                uncomputedSubgames.push_back(std::move(subgame));
        }

        position = Game{std::move(uncomputedSubgames)};
        return modified;
    }
}

template <typename Game>
struct std::hash<spots::Couple<Game>>
{
    std::size_t operator()(const spots::Couple<Game> &c) const
    {
        size_t seed = 0;
        spots::utils::hash_combine(seed, c.position);
        spots::utils::hash_combine(seed, c.nimber);

        return seed;
    }
};

#endif