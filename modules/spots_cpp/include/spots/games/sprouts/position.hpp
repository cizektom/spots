#ifndef SPROUTS_POSITION_H
#define SPROUTS_POSITION_H

#include <algorithm>
#include <limits>

#include "structure.hpp"
#include "land.hpp"
#include "spots/global.hpp"

namespace sprouts
{
    struct Position : public Structure<Position, Land>
    {

    public:
        static constexpr bool isNormalImpartial = true;
        struct Compact
        {
            Compact() = default;
            explicit Compact(const std::string &str) : str{str} {}
            explicit Compact(std::string &&str) : str{std::move(str)} {}

            const std::string &to_string() const { return str; }
            bool operator==(const Compact &other) const { return str == other.str; }

            std::string str;
        };

        Position() : Structure() {}
        Position(const Land &land) : Structure(std::vector<Land>{land}) {}
        Position(Land &&land) : Structure(std::vector<Land>{std::move(land)}) {}
        Position(const std::vector<Land> &lands) : Structure(lands) {}
        Position(std::vector<Land> &&lands) : Structure(std::move(lands)) {}
        Position(const std::vector<Position> &positions);
        Position(std::vector<Position> &&positions);
        /// @brief Creates a position from its string representation.
        explicit Position(const std::string &positionStr);
        /// @brief Creates a position from its string representation.
        Position(const char *str) : Position{std::string{str}} {}
        explicit Position(size_t singletons) : Position{Vertex::create0String(singletons)} {}
        /// @brief Creates a position from its string representation.
        explicit Position(const Compact &compact) : Position{compact.str} {}

        std::vector<Position> getSubgames() const;
        size_t getSubgamesNumber() const { return children.size(); }
        std::vector<Land> &getLands() { return children; }
        const std::vector<Land> &getLands() const { return children; }
        /// @brief Returns true if the position contains more than one land.
        bool isMultiLand() const { return children.size() > 1; }
        bool isTerminal() const { return children.size() == 0; }
        spots::Outcome getOutcome() const { return (isTerminal()) ? spots::Outcome::Loss : spots::Outcome::Unknown; }
        size_t estimateProofDepth() const { return getLives(); }
        size_t estimateDisproofDepth() const { return getLives(); }

        static constexpr char getSeparatorChar() { return Vertex::getPositionEndChar(); }
        static const Vertex separator;

        std::string to_string() const;
        Compact to_compact() const { return Compact(to_string()); }

        friend std::ostream &operator<<(std::ostream &o, const Position &p) { return o << p.to_string(); }

        /// @brief Splits the position into independent lands.
        void splitLands();

        /// @brief Reduces and canonizes the position including splitting of the lands.
        void simplify();

        /// @brief Reassigns names of 1Regs.
        void rename1RegsTo2Regs();

        /// @brief Computes unique children of the position.
        std::vector<Position> computeChildren() const;
        /// @brief Estimates number of children this position might have.
        size_t estimateChildrenNumber() const;

        struct Stats
        {
            void toRelative(int lives);

            float lives = 0;
            float len = 0;
            float _0 = 0;
            float _1 = 0;
            float _2 = 0;
            float _1Regs = 0;
            float max1Reg = 0;
            float _2Regs = 0;
            float max2Reg = 0;

            float bounds = 0;
            float avgBoundLen = 0;
            float minBoundLen = std::numeric_limits<float>::infinity();
            float maxBoundLen = 0;
            float avgBoundLives = 0;
            float minBoundLives = std::numeric_limits<float>::infinity();
            float maxBoundLives = 0;

            float regs = 0;
            float avgRegLen = 0;
            float minRegLen = std::numeric_limits<float>::infinity();
            float maxRegLen = 0;
            float avgRegLives = 0;
            float minRegLives = std::numeric_limits<float>::infinity();
            float maxRegLives = 0;

            float lands = 0;
            float avgLandLen = 0;
            float minLandLen = std::numeric_limits<float>::infinity();
            float maxLandLen = 0;
            float avgLandLives = 0;
            float minLandLives = std::numeric_limits<float>::infinity();
            float maxLandLives = 0;

            bool isMultiLand = false;

            std::vector<float> boundariesLives;
            std::vector<float> regionLives;
        };

        /// @brief Returns basic statistics of the position.
        Stats getStats() const;
    };
}

template <>
struct std::hash<sprouts::Position>
{
    std::size_t operator()(const sprouts::Position &p) const { return p.getHash(); }
};

template <>
struct std::hash<sprouts::Position::Compact>
{
    std::size_t operator()(const sprouts::Position::Compact &c) const { return std::hash<std::string>{}(c.str); }
};

#endif