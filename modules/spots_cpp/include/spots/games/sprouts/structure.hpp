#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <vector>
#include <string>
#include <ostream>
#include <sstream>

#include "sequence.hpp"
#include "vertex.hpp"

namespace sprouts
{
    template <typename Parent, typename Child>
    struct Structure
    {

    public:
        Structure() : children{} {}
        Structure(const std::vector<Child> &children) : children{children} {}
        Structure(std::vector<Child> &&children) : children{std::move(children)} {}
        /// @brief Creates a structure from its string representation.
        Structure(const std::string &str) : children{} { addChildren(str); }

        /// @brief Returns true if the structure contains no children.
        bool empty() const { return children.empty(); }
        /// @brief Returns number of subgames.
        size_t size() const { return children.size(); }
        /// @brief Returns number of lives of the structure.
        uint getLives() const { return sequence::getLives(cbegin(), cend()); }

        /// @brief Applies a given function to all children.
        void apply(std::function<void(Child &)> f) { std::for_each(children.begin(), children.end(), f); }
        /// @brief Applies a given function to all children.
        void apply(std::function<void(const Child &)> f) const { std::for_each(children.cbegin(), children.cend(), f); }
        /// @brief Removes all children for which a given function returns true.
        void remove_all(std::function<bool(Child &)> f) { children.erase(remove_if(children.begin(), children.end(), f), children.end()); }

        /// @brief Removes all the elements in the structure recursively.
        void clear();
        /// @brief Merges adjacent occurrences of letter vertices into a single occurrence in the whole structure.
        void mergeAdjacentVertices();
        /// @brief Renames 2Regs and Temps occurring only in a single boundary to 1Regs.
        /// For correctness, dead vertices must be deleted and there must not be any 1Reg.
        void rename2RegsTo1Regs();

        /// @brief Adds representation of the structure to a given string.
        /// @param useExpanded_1Reg If true, m() notation will be used.
        /// @param useExpanded_2Reg If true, n() notation will be used.
        void addToString(std::string &result, bool useExpanded1Reg, bool useExpanded2Reg) const;
        std::string to_string() const;
        friend std::ostream &operator<<(std::ostream &o, const Structure<Parent, Child> &s) { return o << s.to_string(); }

        /// @brief Returns true if a given structure is strictly equal to this one.
        bool operator==(const Structure &other) const { return children == other.children; }
        /// @brief Returns true if a given structure is not equal to this one.
        bool operator!=(const Structure &other) const { return !operator==(other); }
        /// @brief Returns hash of this structure.
        size_t getHash() const { return sequence::getHash(cbeginSeps(), cendSeps()); }

        /// @brief Defines ordering of structures based on the function sequence::compare().
        bool operator<(const Structure &other) const { return sequence::compare(cbeginSeps(), cendSeps(), other.cbeginSeps(), other.cendSeps()); }
        /// @brief Sorts children based on ordering defined in operator<().
        void sort() { std::sort(children.begin(), children.end()); }

        /// @brief Returns runtime size of the structure in bytes.
        size_t getMemorySize() const;

    protected:
        std::vector<Child> children;

        /// @brief Adds new children to the structure given their string representation.
        void addChildren(const std::string &str);

    private:
        /// @brief Forward iterator for iterating through all the vertices in a structure
        /// not including separators.
        class Iterator
        {

        public:
            Iterator() : outerIt{}, outerEnd{}, innerIt{} {}
            Iterator(std::vector<Child>::iterator start, std::vector<Child>::iterator end) : outerIt{start},
                                                                                             outerEnd{end}
            {
                if (outerIt != outerEnd)
                {
                    innerIt = outerIt->begin();
                    moveToFirstValid();
                }
                else
                    innerIt = typename Child::iterator{};
            }

            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = Vertex;
            using pointer = value_type *;
            using reference = value_type &;

            reference operator*() { return *innerIt; }
            pointer operator->() { return &*innerIt; }
            Iterator &operator++()
            {
                ++innerIt;
                moveToFirstValid();
                return *this;
            }
            Iterator operator++(int)
            {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }
            friend bool operator!=(const Iterator &it1, const Iterator &it2)
            {
                return it1.outerIt != it2.outerIt || it1.innerIt != it2.innerIt;
            }
            friend bool operator==(const Iterator &it1, const Iterator &it2) { return !(it1 != it2); }

        private:
            /// @brief Sets the value of innerIt to the next valid position including the current one.
            void moveToFirstValid()
            {
                // an inner part can be empty => they must be skipped over
                while (innerIt == outerIt->end())
                {
                    ++outerIt;
                    if (outerIt != outerEnd)
                    {
                        // move to the next inner part
                        innerIt = outerIt->begin();
                    }
                    else
                    {
                        // the end was reached; further iteration is forbidden
                        innerIt = typename Child::iterator{};
                        break;
                    }
                }
            }

            std::vector<Child>::iterator outerIt;
            std::vector<Child>::iterator outerEnd;
            typename Child::iterator innerIt;
        };

        /// @brief Forward const iterator for iterating through all the vertices in a structure
        /// not including separators.
        class ConstIterator
        {

        public:
            ConstIterator() : outerIt{}, outerEnd{}, innerIt{} {}
            ConstIterator(std::vector<Child>::const_iterator start, std::vector<Child>::const_iterator end) : outerIt{start},
                                                                                                              outerEnd{end}
            {
                if (outerIt != outerEnd)
                {
                    innerIt = outerIt->cbegin();
                    moveToFirstValid();
                }
                else
                    innerIt = typename Child::const_iterator{};
            }

            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = const Vertex;
            using pointer = const value_type *;
            using reference = const value_type &;

            reference operator*() { return *innerIt; }
            pointer operator->() { return &*innerIt; }
            ConstIterator &operator++()
            {
                ++innerIt;
                moveToFirstValid();
                return *this;
            }
            ConstIterator operator++(int)
            {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }
            friend bool operator!=(const ConstIterator &it1, const ConstIterator &it2)
            {
                return it1.outerIt != it2.outerIt || it1.innerIt != it2.innerIt;
            }
            friend bool operator==(const ConstIterator &it1, const ConstIterator &it2) { return !(it1 != it2); }

        private:
            /// @brief Sets the value of innerIt to the next valid position including the current one.
            void moveToFirstValid()
            {
                // some inner part can be empty => they must be skipped over
                while (innerIt == outerIt->cend())
                {
                    ++outerIt;
                    if (outerIt != outerEnd)
                    {
                        // move to the next inner part
                        innerIt = outerIt->cbegin();
                    }
                    else
                    {
                        // the end was reached; further iteration is forbidden
                        innerIt = typename Child::const_iterator{};
                        break;
                    }
                }
            }

            std::vector<Child>::const_iterator outerIt;
            std::vector<Child>::const_iterator outerEnd;
            typename Child::const_iterator innerIt;
        };

        /// @brief Forward iterator for iterating through all the vertices in a structure
        /// including separators.
        class ConstIteratorSeps
        {

        private:
            ConstIteratorSeps(std::vector<Child>::const_iterator it) : outerIt{it},
                                                                       innerIt{},
                                                                       remainingSize{(size_t)-1} {}

            ConstIteratorSeps(std::vector<Child>::const_iterator it, size_t remainingSize) : outerIt{it},
                                                                                             remainingSize{remainingSize}
            {
                innerIt = (remainingSize > 0) ? outerIt->cbeginSeps() : typename Child::const_iterator_seps{};
            }

        public:
            ConstIteratorSeps() : outerIt{}, innerIt{}, remainingSize{0} {}
            static ConstIteratorSeps cbegin(const Structure<Parent, Child> &s) { return ConstIteratorSeps{s.children.cbegin(), s.size()}; }
            static ConstIteratorSeps cend(const Structure<Parent, Child> &s) { return ConstIteratorSeps{s.children.cend()}; }

            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = const Vertex;
            using pointer = const value_type *;
            using reference = const value_type &;

            reference operator*()
            {
                if (remainingSize == 0)
                    return Parent::separator;
                else
                    return *innerIt;
            }
            pointer operator->() { return &(this->operator*()); }
            ConstIteratorSeps &operator++()
            {
                if (remainingSize == 0)
                    remainingSize--;
                else
                {
                    ++innerIt;
                    if (innerIt == outerIt->cendSeps())
                    {
                        ++outerIt;
                        remainingSize--;
                        innerIt = (remainingSize > 0) ? outerIt->cbeginSeps() : typename Child::const_iterator_seps{};
                    }
                }

                return *this;
            }
            ConstIteratorSeps operator++(int)
            {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }
            friend bool operator!=(const ConstIteratorSeps &it1, const ConstIteratorSeps &it2)
            {
                return it1.remainingSize != it2.remainingSize || it1.outerIt != it2.outerIt || it1.innerIt != it2.innerIt;
            }
            friend bool operator==(const ConstIteratorSeps &it1, const ConstIteratorSeps &it2) { return !(it1 != it2); }

        private:
            std::vector<Child>::const_iterator outerIt;
            typename Child::const_iterator_seps innerIt;
            size_t remainingSize;
        };

    public:
        /// @brief Forward iterator for iterating through all the vertices in a structure not including separators
        using iterator = Iterator;
        /// @brief Forward const iterator for iterating through all the vertices in a structure not including separators
        using const_iterator = ConstIterator;
        /// @brief Forward const iterator for iterating through all the vertices in a structure including separators.
        using const_iterator_seps = ConstIteratorSeps;

        /// @brief Returns an iterator pointing to the first vertex in the whole structure not including separators.
        iterator begin() { return Iterator{children.begin(), children.end()}; }
        /// @brief Returns an iterator pointing one past the last vertex in the whole structure not including separators.
        iterator end() { return Iterator{children.end(), children.end()}; }

        /// @brief Returns a const iterator pointing to the first vertex in the whole structure not including separators.
        const_iterator begin() const { return ConstIterator{children.cbegin(), children.cend()}; }
        /// @brief Returns a const iterator pointing one past the last vertex in the whole structure not including separators.
        const_iterator end() const { return ConstIterator{children.cend(), children.cend()}; }

        /// @brief Returns a const iterator pointing to the first vertex in the whole structure not including separators.
        const_iterator cbegin() const { return ConstIterator{children.cbegin(), children.cend()}; }
        /// @brief Returns a const iterator pointing one past the last vertex in the whole structure not including separators.
        const_iterator cend() const { return ConstIterator{children.cend(), children.cend()}; }

        /// @brief Returns a const iterator pointing to the first vertex in the whole structure including separators.
        const_iterator_seps cbeginSeps() const { return ConstIteratorSeps::cbegin(*this); }
        /// @brief Returns a const iterator pointing one past the last vertex in the whole structure including separators.
        const_iterator_seps cendSeps() const { return ConstIteratorSeps::cend(*this); }
    };

    template <typename Parent, typename Child>
    void Structure<Parent, Child>::clear()
    {
        apply([](Child &c)
              { c.clear(); });

        children.clear();
    }

    template <typename Parent, typename Child>
    void Structure<Parent, Child>::mergeAdjacentVertices()
    {
        apply([](Child &c)
              { c.mergeAdjacentVertices(); });
    }

    template <typename Parent, typename Child>
    void Structure<Parent, Child>::rename2RegsTo1Regs()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));
        apply([](Child &c)
              { c.rename2RegsTo1Regs(); });
    }

    template <typename Parent, typename Child>
    void Structure<Parent, Child>::addChildren(const std::string &str)
    {
        std::stringstream ss(Vertex::expandSingletons(str));
        std::string parsed;

        while (true)
        {
            getline(ss, parsed, Child::getSeparatorChar());
            if (ss.fail())
                break;

            if (parsed.size() > 0)
            {
                Child child{parsed};
                if (!child.empty())
                    children.push_back(std::move(child));
            }
        }
    }

    template <typename Parent, typename Child>
    void Structure<Parent, Child>::addToString(std::string &str, bool useExpanded1Reg, bool useExpanded2Reg) const
    {
        for (auto &&child : children)
        {
            child.addToString(str, useExpanded1Reg, useExpanded2Reg);
            str += Child::getSeparatorChar();
        }

        if (str.size() > 0)
            str.pop_back(); // remove the last separator
    }

    template <typename Parent, typename Child>
    std::string Structure<Parent, Child>::to_string() const
    {
        bool useExpanded1Reg = false;
        bool useExpanded2Reg = false;

        // determine wheter extended names need to be used
        for (auto &&vertex : *this)
        {
            if (vertex.requiresExpanded1Reg())
                useExpanded1Reg = true;
            else if (vertex.requiresExpanded2Reg())
                useExpanded2Reg = true;
        }

        std::string result;
        addToString(result, useExpanded1Reg, useExpanded2Reg);
        return Vertex::shortenSingletons(result);
    }

    template <typename Parent, typename Child>
    size_t Structure<Parent, Child>::getMemorySize() const
    {
        size_t size = sizeof(children);
        for (auto &&child : children)
            size += child.getMemorySize();

        return size;
    }
}

#endif