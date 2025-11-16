#ifndef REGION_H
#define REGION_H

#include "structure.hpp"
#include "boundary.hpp"

namespace sprouts
{
    struct Region : public Structure<Region, Boundary>
    {
    public:
        Region() : Structure{} {}
        Region(const std::vector<Boundary> &boundaries) : Structure(boundaries) {}
        Region(std::vector<Boundary> &&boundaries) : Structure(std::move(boundaries)) {}
        /// @brief Creates a region from its string representation.
        Region(const std::string &str) : Structure(str) {}
        /// @brief Creates a region from its string representation.
        Region(const char *str) : Structure(std::string{str}) {}

        std::vector<Boundary> &getBoundaries() { return children; }
        const std::vector<Boundary> &getBoundaries() const { return children; }

        static constexpr char getSeparatorChar() { return Vertex::getRegionEndChar(); }
        static const Vertex separator;

        /// @brief Returns true if the region is dead. Note that the region must be reduced
        /// before the call (no dead and adjacent vertices).
        bool isDead() const;

        /// @brief Deletes dead vertices (3) and 2Regs in a given vector.
        void deleteDeadVertices(const int occurrences[]);
        /// @brief Deletes empty boudaries in the region.
        void deleteEmptyBoundaries();
        /// @brief Merges boundaries into a single one if the region has at most 3 lives.
        /// For example: 1.2 -> 12, AB.2 -> AB2, 2a2a -> 222.
        void mergeBoundaries();

        /// @brief Reassigns names of 1Regs.
        void rename1Regs();
        /// @brief Sorts boundaries recursively together with an option to reverse
        /// the orientation of the region.
        void sort();

        /// @brief A single-boundary child of a region that constists of two newly created regions.
        struct SBChild;
        /// @brief A double-boundary child of a region.
        struct DBChild;
        /// @brief Represents a partition of a region boundaries into two parts.
        struct Partition;

        /// @brief Computes single-boundary children of the region. Note that they are not simplified.
        /// Note that the land must not contain 1Reg vertices.
        std::unordered_set<SBChild> computeSBChildren();
        /// @brief Computes double-boundary children of the region. Note that they are not simplified.
        /// Note that the land must not contain 1Reg vertices.
        std::unordered_set<DBChild> computeDBChildren();
        /// @brief Estimates number of children this region might have.
        size_t estimateChildrenNumber() const;

    private:
        /// @brief Reverses orientation of the region.
        void reverseOrientation();
        /// @brief Sorts boundaries recursively.
        void sortBoundaries();

        /// @brief Returns the index of the first non-singleton in the sequence. Assumes that the
        /// region is canonized and thus singletons are at the beginning of the sequence.
        size_t getFirstNonSingletonIndex();
        /// @brief Returns the first index of a child from to start while generating db-children.
        size_t getDBStart();
        /// @brief Returns the first index of a child from to start while generating sb-children.
        size_t getSBStart();

        /// @brief Returns the number of partitions that will result from given boundaries.
        static size_t getPartitionsNumber(size_t boundariesNumber);
        /// @brief Returns partitions of given singletons, which are indistinguishable.
        static std::vector<Partition> partitionSingletons(size_t singletonsNumber);
        /// @brief Returns partitions of given non-singleton boundaries. Only unique partitions are returned.
        static std::vector<Partition> partitionNonSingletons(const std::vector<const Boundary *> &boundaries);
        /// @brief Returns partitions of given boundaries. Note that singletons are indistinguishable.
        /// Only unique partitions are returned.
        static std::vector<Partition> partitionBoundaries(const std::vector<const Boundary *> &boundaries);
    };

    struct Region::SBChild
    {
        SBChild(const Boundary::SBChild &child, const Region::Partition &parition);
        bool operator==(const SBChild &other) const { return major == other.major && minor == other.minor; }

        Region major;
        Region minor;
    };

    struct Region::DBChild
    {
        /// @brief Creates a double-boundary child by connecting two given fragmets and adding unused boundaries.
        DBChild(const Boundary::DBChild &child1, const Boundary::DBChild &child2,
                const std::vector<const Boundary *> &unusedBoundaries);
        bool operator==(const DBChild &other) const { return region == other.region; }

        Region region;
    };

    struct Region::Partition
    {
        Partition() = default;
        Partition(const std::vector<Boundary> &firstPart, const std::vector<Boundary> &secondPart) : firstPart{firstPart},
                                                                                                     secondPart{secondPart} {}
        Partition(std::vector<Boundary> &&firstPart, std::vector<Boundary> &&secondPart) : firstPart{std::move(firstPart)},
                                                                                           secondPart{std::move(secondPart)} {}

        bool operator==(const Partition &other) const { return firstPart == other.firstPart && secondPart == other.secondPart; }

        Region firstPart;
        Region secondPart;
    };
}

template <>
struct std::hash<sprouts::Region>
{
    std::size_t operator()(const sprouts::Region &r) const { return r.getHash(); }
};

template <>
struct std::hash<sprouts::Region::SBChild>
{
    std::size_t operator()(const sprouts::Region::SBChild &child) const
    {
        size_t seed = 0;
        spots::utils::hash_combine(seed, child.major);
        spots::utils::hash_combine(seed, child.minor);
        return seed;
    }
};

template <>
struct std::hash<sprouts::Region::DBChild>
{
    std::size_t operator()(const sprouts::Region::DBChild &child) const
    {
        return std::hash<sprouts::Region>()(child.region);
    }
};

template <>
struct std::hash<sprouts::Region::Partition>
{
    std::size_t operator()(const sprouts::Region::Partition &p) const
    {
        size_t seed = 0;
        spots::utils::hash_combine(seed, p.firstPart);
        spots::utils::hash_combine(seed, p.secondPart);
        return seed;
    }
};

#endif