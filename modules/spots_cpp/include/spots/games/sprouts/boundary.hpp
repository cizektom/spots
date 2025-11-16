#ifndef BOUNDARY_H
#define BOUNDARY_H

#include <vector>
#include <unordered_set>
#include <optional>

#include "vertex.hpp"
#include "sequence.hpp"

namespace sprouts
{
    struct Boundary
    {

    public:
        Boundary() : vertices{} {}
        Boundary(const std::vector<Vertex> &vertices) : vertices{vertices} {}
        Boundary(std::vector<Vertex> &&vertices) : vertices{std::move(vertices)} {}
        /// @brief Creates a boundary from its string representation.
        Boundary(const std::string &seq) : vertices{Vertex::parseString(seq)} {}
        /// @brief Creates a boundary from its string representation.
        Boundary(const char *seq) : Boundary{std::string{seq}} {}

        static Boundary createSingleton() { return Boundary{std::vector<Vertex>{Vertex::create0()}}; }

        std::vector<Vertex> &getVertices() { return vertices; }
        const std::vector<Vertex> &getVertices() const { return vertices; }
        /// @brief Returns number of vertices inside the boundary.
        size_t size() const { return vertices.size(); }

        static constexpr char getSeparatorChar() { return Vertex::getBoundaryEndChar(); }
        static const Vertex separator;

        bool empty() const { return vertices.empty(); }
        bool isSingleton() const { return vertices.size() == 1 && vertices[0].is0(); }

        /// @brief Removes all the vertices from the boundary.
        void clear() { vertices.clear(); }
        /// @brief Deletes dead vertices (3) and 2Regs in a given vector.
        void deleteDeadVertices(const int occurrences[]);
        /// @brief Merges adjacent occurrences of letter vertices into a single occurrence.
        void mergeAdjacentVertices();
        /// @brief Renames 2Regs and Temps occurring only in this boundary to 1Regs.
        /// For correctness, dead vertices must be deleted and there are no 1Reg vertices.
        void rename2RegsTo1Regs();

        enum class RenamingMode
        {
            /// @brief Mode renaming 1Regs.
            _1Regs,
            /// @brief Mode renaming 2Regs and Temps.
            _2RegsTemp,
            /// @brief Mode renaming 1Regs to 2Regs.
            _1RegsTo2Regs
        };
        /// @brief Reassigns names of 1Regs, 2Regs or Temps depending on a given mode.
        /// @param mode A renaming mode.
        /// @param indexMapping Mapping of already reassigned vertices to their new indices.
        /// @param nextFreeIndex Next index that is free to use. It is increased after it is used.
        void renameRegs(RenamingMode mode, Vertex::indexType indexMapping[], Vertex::indexType &nextFreeIndex);
        /// @brief Reassigns names of 1Regs.
        void rename1Regs();
        void reverseOrientation() { std::reverse(vertices.begin(), vertices.end()); }
        /// @brief Finds and sets a minimal rotation of vertices.
        void sort();
        /// @brief Defines ordering of boundaries based on the function sequence::compare().
        bool operator<(const Boundary &other) const { return sequence::compare(cbeginSeps(), cendSeps(), other.cbeginSeps(), other.cendSeps()); }
        /// @brief Returns true if a given boundary is strictly equal to this one.
        bool operator==(const Boundary &other) const { return vertices == other.vertices; }
        /// @brief Returns true if a given boundary is not equal to this one.
        bool operator!=(const Boundary &other) const { return !operator==(other); }

        /// @brief Adds representation of the boundary to a given string.
        /// @param useExpanded_1Reg If true, m() notation will be used.
        /// @param useExpanded_2Reg If true, n() notation will be used.
        void addToString(std::string &str, bool useExpanded1Reg, bool useExpanded2Reg) const;
        /// @brief Returns a string representation of the boundary.
        std::string to_string() const { return sequence::to_string(cbegin(), cend()); }
        friend std::ostream &operator<<(std::ostream &o, const Boundary &b) { return o << b.to_string(); }

        /// @brief Returns runtime size of the structure in bytes.
        size_t getMemorySize() const { return sizeof(vertices) + vertices.size() * sizeof(Vertex); }

        /// A single-boundary child of a boundary. A sb-child of a boundary consits of a major
        /// and a minor boundary that need to be completed later with other boundaries to
        /// create a major and a minor region.
        struct SBChild;

        /// @brief A double-boundary child of a boundary. A db-child of a boundary is only
        /// its fragment that must be connected with other fragment later.
        struct DBChild;

        /// @brief Creates a single-boundary child from a given boundary. A sb-child of a boundary
        /// consits of a major and a minor boundary that need to be completed later with other boundaries
        /// to create a major and a minor region.
        /// Note that the land must not contain 1Reg vertices.
        std::unordered_set<Boundary::SBChild> computeSBChildren();
        /// @brief Returns double-boundary children of the boundary. A db-child of a boundary is only
        /// its fragment that must be connected with other fragment later.
        /// Note that the land must not contain 1Reg vertices.
        std::unordered_set<Boundary::DBChild> computeDBChildren();

    private:
        std::vector<Vertex> vertices;

        /// @brief Forward iterator for iterating through all the vertices in a boundary
        /// including the last separator.
        class ConstIteratorSeps
        {
        private:
            ConstIteratorSeps(std::vector<Vertex>::const_iterator it) : it{it},
                                                                        remainingSize{(size_t)-1} {}
            ConstIteratorSeps(std::vector<Vertex>::const_iterator it, size_t remainingSize) : it{it},
                                                                                              remainingSize{remainingSize} {}

        public:
            ConstIteratorSeps() : it{}, remainingSize{0} {}
            static ConstIteratorSeps cbegin(const Boundary &b) { return ConstIteratorSeps{b.vertices.cbegin(), b.size()}; }
            static ConstIteratorSeps cend(const Boundary &b) { return ConstIteratorSeps{b.vertices.cend()}; }

            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = const Vertex;
            using pointer = const value_type *;
            using reference = const value_type &;

            reference operator*()
            {
                if (remainingSize > 0)
                    return *it;
                else
                    return Boundary::separator;
            }
            pointer operator->() { return &(this->operator*()); }
            ConstIteratorSeps &operator++()
            {
                if (remainingSize > 0)
                    ++it;

                remainingSize--;
                return *this;
            }
            ConstIteratorSeps operator++(int)
            {
                auto &&tmp = *this;
                ++(*this);
                return tmp;
            }
            friend bool operator!=(const ConstIteratorSeps &it1, const ConstIteratorSeps &it2)
            {
                return it1.remainingSize != it2.remainingSize || it1.it != it2.it;
            }
            friend bool operator==(const ConstIteratorSeps &it1, const ConstIteratorSeps &it2) { return !(it1 != it2); }

        private:
            std::vector<Vertex>::const_iterator it;
            size_t remainingSize;
        };

        class RotationIterator
        {

        private:
            RotationIterator(const Boundary *b, size_t rotation) : boundary{b}, index(rotation) {}

        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = const Vertex;
            using pointer = const value_type *;
            using reference = const value_type &;

            static RotationIterator cbegin(const Boundary *b, size_t rotation) { return RotationIterator{b, rotation}; }
            static RotationIterator cend(const Boundary *b, size_t rotation) { return RotationIterator{b, rotation + b->vertices.size()}; }

            reference operator*() { return boundary->vertices[index % boundary->vertices.size()]; }
            pointer operator->() { return &boundary->vertices[index % boundary->vertices.size()]; }

            RotationIterator &operator++()
            {
                ++index;
                return *this;
            }
            RotationIterator operator++(int)
            {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator!=(const RotationIterator &it1, const RotationIterator &it2) { return it1.index != it2.index || it1.boundary != it2.boundary; }
            friend bool operator==(const RotationIterator &it1, const RotationIterator &it2) { return !(it1 != it2); }

        private:
            const Boundary *boundary;
            size_t index;
        };

        struct Rotation
        {
            Rotation(const Boundary *boundary, size_t rotation) : first{RotationIterator::cbegin(boundary, rotation)},
                                                                  last{RotationIterator::cend(boundary, rotation)} {}

            RotationIterator first;
            RotationIterator last;
        };

        using rotation = Rotation;
        /// @brief Returns an iterator that iterates over vertices in the boundary but rotated
        /// by a given size.
        rotation getRotation(size_t rotation) const { return Rotation(this, rotation); }

    public:
        /// @brief Forward iterator for iterating through vertices of a boundary.
        using iterator = std::vector<Vertex>::iterator;
        /// @brief Forward const iterator for iterating through vertices of a boundary.
        using const_iterator = std::vector<Vertex>::const_iterator;
        /// @brief Forward const iterator for iterating through vertices of a boundary.
        /// Including separators the last separator.
        using const_iterator_seps = ConstIteratorSeps;

        /// @brief Returns an iterator pointing to the first vertex in the boundary.
        iterator begin() { return vertices.begin(); }
        /// @brief Returns an iterator pointing one past the last vertex in the boundary.
        iterator end() { return vertices.end(); }

        /// @brief Returns a const iterator pointing to the first vertex in the boundary.
        const_iterator begin() const { return vertices.cbegin(); }
        /// @brief Returns a const iterator pointing one past the last Vertex in the boundary.
        const_iterator end() const { return vertices.cend(); }

        /// @brief Returns a const iterator pointing to the first vertex in the boundary.
        const_iterator cbegin() const { return vertices.cbegin(); }
        /// @brief Returns a const iterator pointing one past the last vertex in the boundary.
        const_iterator cend() const { return vertices.cend(); }

        /// @brief Returns a const iterator pointing to the first vertex in the boundary
        /// including the last separator.
        const_iterator_seps cbeginSeps() const { return ConstIteratorSeps::cbegin(*this); }
        /// @brief Returns a const iterator pointing one past the last vertex in the boundary
        /// including the last separator.
        const_iterator_seps cendSeps() const { return ConstIteratorSeps::cend(*this); }
    };

    struct Boundary::SBChild
    {
        /// @brief Creates a simple single-boundary child from a given boundary. That is a child from
        /// a boundary containing a single vertex. Returns std::nullopt if given parameters does not generate a child.
        static std::optional<SBChild> createSimpleChild(const Boundary &b);
        /// @brief Creates a single-boundary child from a given boundary resulting from a connection of the first
        /// and second occurence of vertices. Returns std::nullopt if given parameters does not generate a child.
        static std::optional<SBChild> createChild(const Boundary &b, size_t firstVertexIndex, size_t secondVertexIndex);
        bool operator==(const SBChild &other) const { return major == other.major && minor == other.minor; }

        Boundary major;
        Boundary minor;

    private:
        SBChild(std::vector<Vertex> &&simple) : major{simple}, minor{std::move(simple)} {}
        SBChild(std::vector<Vertex> &&major, std::vector<Vertex> &&minor) : major{std::move(major)},
                                                                            minor{std::move(minor)} {}

        static void initConnectedVertices(Vertex &first, Vertex &second, size_t firstVertexIndex, size_t secondVertexIndex);
    };

    struct Boundary::DBChild
    {
        /// Creates a double-boundary child (a fragment) from a given rotation of a boundary.
        /// The first occurrence of a vertex in the rotation will be later connected.
        DBChild(const rotation &r);
        bool operator==(const DBChild &other) const { return fragment == other.fragment; }

        Boundary fragment;
    };
}

template <>
struct std::hash<sprouts::Boundary>
{
    std::size_t operator()(const sprouts::Boundary &b) const
    {
        return sprouts::sequence::getHash(b.cbeginSeps(), b.cendSeps());
    }
};

template <>
struct std::hash<sprouts::Boundary::SBChild>
{
    std::size_t operator()(const sprouts::Boundary::SBChild &child) const
    {
        size_t seed = 0;
        spots::utils::hash_combine(seed, child.major);
        spots::utils::hash_combine(seed, child.minor);
        return seed;
    }
};

template <>
struct std::hash<sprouts::Boundary::DBChild>
{
    std::size_t operator()(const sprouts::Boundary::DBChild &c) const
    {
        return std::hash<sprouts::Boundary>()(c.fragment);
    }
};

#endif