#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <functional>
#include <algorithm>

#include "vertex.hpp"

namespace sprouts
{
    namespace sequence
    {
        /// @brief Returns a number of lives in a given sequence.
        template <typename It>
        static uint getLives(It &&first, It &&last);

        /// @brief Returns true if given sequence contains a 1Reg.
        template <typename It>
        static bool contains1Reg(It &&first, It &&last);

        /// @brief Returns number of occurrences of a given vertex in a given sequence.
        template <typename It>
        static int getOccurrences(It &&first, It &&last, Vertex v);

        template <typename It>
        static void fill2RegTempOccurrences(int occurrences[], It &&first, It &&last);

        /// @brief Returns true if given sequences have common 2Reg or Temp vertex.
        template <typename It1, typename It2>
        static bool areLinked(It1 &&first1, It1 &&last1, It2 &&first2, It2 &&last2);

        /// @brief Returns true if the sequence of vertices defined by the range [first1,last1) is
        /// less than the sequence of vertices defined by the range [first2,last2).
        /// First, the pseudo-comparison is used and then the lexicographical comparison if needed.
        template <typename It>
        static bool compare(It &&first1, It &&last1, It &&first2, It &&last2);

        /// @brief Converts a given sequence of vertices to its string representation.
        template <typename It>
        static std::string to_string(It &&first, It &&last);

        /// @brief Computes a hash of a given sequence of vertices.
        template <typename It>
        static size_t getHash(It &&first, It &&last);

        /// @brief Returns true if the sequence of vertices defined by the range [first1,last1) is
        /// strictly equal to the sequence of vertices defined by the range [first2, last2).
        template <typename It>
        static bool areEqual(It &&first1, It &&last1, It &&first2, It &&last2);
    }

    /// @brief Returns a number of lives in a given sequence.
    template <typename It>
    static uint sequence::getLives(It &&first, It &&last)
    {
        uint lives = 0;
        int letters = 0;

        for (auto it = std::move(first); it != last; ++it)
        {
            if (it->isLetter())
                letters++;
            else
                lives += it->getLives();
        }

        lives += letters / 2; // count half as each letter has 1 life and occurs twice
        return lives;
    }

    template <typename It>
    static bool sequence::contains1Reg(It &&first, It &&last)
    {
        for (auto it = std::move(first); it != last; ++it)
            if (it->is1Reg())
                return true;

        return false;
    }

    /// @brief Returns number of occurrences of a given vertex in a given sequence.
    template <typename It>
    static int sequence::getOccurrences(It &&first, It &&last, Vertex v)
    {
        int occurrances = 0;
        for (auto it = std::move(first); it != last; ++it)
            if (*it == v)
                occurrances++;

        return occurrances;
    }

    template <typename It>
    static void sequence::fill2RegTempOccurrences(int occurrences[], It &&first, It &&last)
    {
        for (auto it = std::move(first); it != last; ++it)
            if (it->is2Reg() || it->isTemp())
                occurrences[it->get2RegTempIndex()] += 1;
    }

    /// @brief Returns true if given sequences have common 2-reg or temp vertex.
    template <typename It1, typename It2>
    static bool sequence::areLinked(It1 &&first1, It1 &&last1, It2 &&first2, It2 &&last2)
    {
        for (auto it1 = first1; it1 != last1; ++it1)
        {
            Vertex v1 = *it1;
            if (v1.is2Reg() || v1.isTemp())
            {
                for (auto it2 = first2; it2 != last2; ++it2)
                {
                    Vertex v2 = *it2;
                    if (v1 == v2)
                        return true;
                }
            }
        }

        return false;
    }

    /// @brief Returns true if the sequence of elements defined by the range [first1,last1) is
    /// less than the sequence of elements defined by the range [first2,last2). Returns false otherwise.
    template <typename It>
    static bool sequence::compare(It &&first1, It &&last1, It &&first2, It &&last2)
    {
        // First, try to decide by pseudo-order (0<1<2<a=b=c=...<A=B=C=...)
        for (auto it1 = first1, it2 = first2; it1 != last1 && it2 != last2; ++it1, ++it2)
        {
            Vertex v1 = *it1, v2 = *it2;
            if (v1 != v2)
                if (!(v1.is1Reg() && v2.is1Reg()) && !(v1.is2Reg() && v2.is2Reg()))
                    return v1 < v2;
        }

        // If not decided, use full lexicographical comparison.
        return std::lexicographical_compare(std::move(first1), std::move(last1),
                                            std::move(first2), std::move(last2));
    }

    /// @brief Converts a given sequence of vertices to its string representation.
    template <typename It>
    static std::string sequence::to_string(It &&first, It &&last)
    {
        bool useExpanded1Reg = false;
        bool useExpanded2Reg = false;
        for (auto it = first; it != last; ++it)
        {
            useExpanded1Reg |= it->requiresExpanded1Reg();
            useExpanded2Reg |= it->requiresExpanded2Reg();
        }

        std::string result;
        for (auto it = std::move(first); it != last; ++it)
            it->addToString(result, useExpanded1Reg, useExpanded2Reg);

        return result;
    }

    /// @brief Computes a hash of a given sequence of vertices.
    template <typename It>
    static size_t sequence::getHash(It &&first, It &&last)
    {
        size_t seed = 0;
        for (auto it = std::move(first); it != last; ++it)
            spots::utils::hash_combine(seed, *it);

        return seed;
    }

    template <typename It>
    static bool sequence::areEqual(It &&first1, It &&last1, It &&first2, It &&last2)
    {
        auto it1 = std::move(first1);
        auto it2 = std::move(first2);
        for (; it1 != last1 && it2 != last2; ++it1, ++it2)
            if (*it1 != *it2)
                return false;

        return it1 == last1 && it2 == last2;
    }
}

#endif