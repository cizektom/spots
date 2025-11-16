#ifndef LAND_H
#define LAND_H

#include "structure.hpp"
#include "region.hpp"

namespace sprouts
{
    struct Land : public Structure<Land, Region>
    {

    public:
        Land() : Structure() {}
        Land(const std::vector<Region> &regions) : Structure{regions} {}
        Land(std::vector<Region> &&regions) : Structure{std::move(regions)} {}
        /// @brief Creates a land from its string representation.
        explicit Land(const std::string &str) : Structure(str) {}
        /// @brief Creates a land from its string representation.
        Land(const char *str) : Structure(std::string{str}) {}

        std::vector<Region> &getRegions() { return children; }
        const std::vector<Region> &getRegions() const { return children; }

        static constexpr char getSeparatorChar() { return Vertex::getLandEndChar(); }
        static const Vertex separator;

        /// @brief Returns true if the land is dead. Note that the land must be reduced
        /// before a call.
        bool isDead() const { return empty(); }

        /// @brief Returns a vector of independent lands consisting of regions that were
        /// moved from this land. If no split occurred, std::nullopt is returned and the
        /// original structure is preserved.
        std::optional<std::vector<Land>> split();

        /// @brief Deletes dead vertices from the land.
        void deleteDeadVertices();
        /// @brief Deletes empty boudaries in the land.
        void deleteEmptyBoudaries();
        /// @brief Deletes dead regions in the land.
        void deleteDeadRegions();
        /// @brief Merges trivial regions into a single boundary.
        void mergeBoundaries();
        /// @brief Converts 2Regs that occurs only once in the land into 2 (a generic vertex).
        void rename2RegsTo2();
        /// @brief Simplifies the land using the reduction algorithm. Note that all 1Regs must
        /// be renamed to 2Regs before a call.
        void reduce();

        /// @brief Reassigns names of 1Regs.
        void rename1Regs();
        /// @brief Reassigns names of 2Regs.
        void rename2Regs();
        /// @brief Renames all 1Regs to 2Regs.
        void rename1RegsTo2Regs();

        /// @brief Sorts regions recursively.
        void sort();
        /// @brief Canonizes the land which is an combination of renamings and sorts.
        void canonize();

        /// @brief Computes children of the land. Note that they are not simplified.
        /// Note that the land must not contain 1Reg vertices.
        std::unordered_set<Land> computeChildren();
        /// @brief Estimates number of children this land might have.
        size_t estimateChildrenNumber() const;

    private:
        /// @brief Moves all the regions in a given land into this one.
        void insertLand(Land &&land);

        /// @brief Returns next free 2Reg index in the land.
        Vertex::indexType findFree2RegIndex() const;
    };
}

template <>
struct std::hash<sprouts::Land>
{
    std::size_t operator()(const sprouts::Land &l) const { return l.getHash(); }
};

#endif