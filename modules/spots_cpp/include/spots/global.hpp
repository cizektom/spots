#ifndef GLOBAL_H
#define GLOBAL_H

#include <unordered_set>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <limits>

namespace spots
{
    enum class Outcome
    {
        Win,
        Loss,
        Unknown
    };

    class utils
    {
    public:
        /// @brief Returns a hash of an integer.
        static size_t getHash(int i)
        {
            // size_t x = i;
            // x = ((x >> 16) ^ x) * 0x45d9f3b;
            // x = ((x >> 16) ^ x) * 0x45d9f3b;
            // x = (x >> 16) ^ x;

            return i;
        }

        /// @brief Combines the hash of `seed` with the hash of `t`.
        template <typename T>
        static void hash_combine(size_t &seed, const T &t)
        {
            seed ^= std::hash<T>()(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        /// @brief Transforms given unordered_set to a vector.
        template <typename T>
        static std::vector<T> to_vector(std::unordered_set<T> &&set)
        {
            std::vector<T> result;
            result.reserve(set.size());
            for (auto it = set.begin(); it != set.end();)
                result.push_back(std::move(set.extract(it++).value()));

            return result;
        }

        /// @brief Constructs a vector by first copying the elements that are referenced by
        /// the pointers in the first argument and then moves additional given arguments.
        template <typename T, typename... Args>
        static std::vector<T> to_vector(const std::vector<const T *> &ptrs, Args &&...args)
        {
            std::vector<T> result;
            result.reserve(ptrs.size() + sizeof...(Args));
            for (auto &&ptr : ptrs)
                result.push_back(*ptr);

            (result.push_back(std::forward<Args>(args)), ...);
            return result;
        }

        /// @brief Constructs a vector by first copying the elements that are referenced by
        /// the pointers in the first argument and then moves additional given arguments in
        /// the second vector.
        template <typename T>
        static std::vector<T> to_vector(const std::vector<const T *> &ptrs, std::vector<T> &&elements)
        {
            std::vector<T> result;
            result.reserve(ptrs.size() + elements.size());
            for (auto &&ptr : ptrs)
                result.push_back(*ptr);

            for (auto &&v : elements)
                result.push_back(std::move(v));

            return result;
        }

        /// @brief Constructs a vector by copying the first given element and then
        /// the elements in a given vector.
        template <typename T>
        static std::vector<T> to_vector(const T &t, const std::vector<T> &v)
        {
            std::vector<T> result;
            result.reserve(1 + v.size());

            result.push_back(t);
            for (auto &&x : v)
                result.push_back(x);

            return result;
        }

        static std::vector<std::string> split(const std::string &s, char delim)
        {
            std::vector<std::string> result;
            std::stringstream ss(s);
            std::string item;

            while (getline(ss, item, delim))
                result.push_back(item);

            return result;
        }
    };
}

#endif