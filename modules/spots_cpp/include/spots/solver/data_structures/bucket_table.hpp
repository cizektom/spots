#ifndef BUCKET_TABLE_H
#define BUCKET_TABLE_H

#include <vector>
#include <optional>
#include "spots/games/sprouts/position.hpp"

namespace spots
{
    template <typename Key, typename Value, typename Hash>
    class BucketTable
    {

    public:
        static constexpr size_t BUCKET_SIZE = 4;

        struct TTEntry
        {
            TTEntry() : key{}, value{}, occupied{false} {}
            template <typename Key_, typename Value_>
            TTEntry(Key_ &&key, Value_ &&value) : key{std::forward<Key_>(key)}, value{std::forward<Value_>(value)}, occupied{true} {}

            Key key;
            Value value;
            bool occupied;
        };

        struct Bucket
        {
            Bucket() = default;

            // Copy constructor
            Bucket(const Bucket &other)
            {
                for (size_t i = 0; i < BUCKET_SIZE; ++i)
                    entries[i] = other.entries[i];
            }

            // Copy assignment
            Bucket &operator=(const Bucket &other)
            {
                if (this != &other)
                {
                    for (size_t i = 0; i < BUCKET_SIZE; ++i)
                        entries[i] = other.entries[i];
                }

                return *this;
            }

            // Move constructor
            Bucket(Bucket &&other) noexcept
            {
                for (size_t i = 0; i < BUCKET_SIZE; ++i)
                    entries[i] = std::move(other.entries[i]);
            }

            // Move assignment
            Bucket &operator=(Bucket &&other) noexcept
            {
                if (this != &other)
                {
                    for (size_t i = 0; i < BUCKET_SIZE; ++i)
                        entries[i] = std::move(other.entries[i]);
                }

                return *this;
            }

            TTEntry entries[BUCKET_SIZE];
            mutable std::shared_mutex mutex;
        };

        BucketTable(size_t capacity = 0, bool threadSafe = false) : threadSafe{threadSafe} { data.resize(capacity / BUCKET_SIZE); }

        // Copy constructor
        BucketTable(const BucketTable &other) : data(other.data), _size(other._size.load()), threadSafe(other.threadSafe) {}

        // Copy assignment
        BucketTable &operator=(const BucketTable &other)
        {
            if (this != &other)
            {
                data = other.data;
                _size.store(other._size.load());
                threadSafe = other.threadSafe;
            }

            return *this;
        }

        // Move constructor
        BucketTable(BucketTable &&other) noexcept : data(std::move(other.data)), _size(other._size.load()), threadSafe(other.threadSafe)
        {
            other._size.store(0);
        }

        // Move assignment
        BucketTable &operator=(BucketTable &&other) noexcept
        {
            if (this != &other)
            {
                data = std::move(other.data);
                _size.store(other._size.load());
                threadSafe = other.threadSafe;
                other._size.store(0);
            }

            return *this;
        }

        void setThreadSafety(bool threadSafe) { this->threadSafe = threadSafe; }
        void lock(std::shared_lock<std::shared_mutex> &lock) const;
        void lock(std::unique_lock<std::shared_mutex> &lock) const;

        size_t size() const { return _size.load(); }

        void clear();
        std::optional<TTEntry> find(const Key &key) const;

        /// @brief Returns original Value that was updated, or std::nullopt if the entry was not found.
        template <typename Key_, typename Value_>
        std::optional<Value> insert(Key_ &&key, Value_ &&value)
        {
            if (data.empty())
                return {};

            Bucket &bucket = data[Hash{}(key) % data.size()];
            std::unique_lock lock{bucket.mutex, std::defer_lock};
            this->lock(lock);

            size_t replaceIdx = 0;
            for (size_t i = 0; i < BUCKET_SIZE; i++)
            {
                if (!bucket.entries[i].occupied || bucket.entries[i].key == key)
                {
                    replaceIdx = i;
                    break;
                }

                if (i != 0 && bucket.entries[i].value < bucket.entries[replaceIdx].value)
                    replaceIdx = i;
            }

            std::optional<Value> originalValue = std::nullopt;
            if (!bucket.entries[replaceIdx].occupied)
            {
                _size.fetch_add(1, std::memory_order_relaxed);
                bucket.entries[replaceIdx] = TTEntry{std::forward<Key_>(key), std::forward<Value_>(value)};
            }
            else if (bucket.entries[replaceIdx].key == key)
            {
                originalValue = bucket.entries[replaceIdx].value;
                bucket.entries[replaceIdx].value.update(value);
            }
            else
            {
                bucket.entries[replaceIdx] = TTEntry{std::forward<Key_>(key), std::forward<Value_>(value)};
            }

            return originalValue;
        }

        template <typename Key_>
        void mark(Key_ &&key, int threadId)
        {
            if (data.empty())
                return;

            Bucket &bucket = data[Hash{}(key) % data.size()];
            std::unique_lock lock{bucket.mutex, std::defer_lock};
            this->lock(lock);

            for (size_t i = 0; i < BUCKET_SIZE; i++)
            {
                if (bucket.entries[i].occupied && bucket.entries[i].key == key)
                {
                    bucket.entries[i].value.mark(threadId);
                    return;
                }
            }
        }

        template <typename Key_>
        void unmark(Key_ &&key, int threadId)
        {
            if (data.empty())
                return;

            Bucket &bucket = data[Hash{}(key) % data.size()];
            std::unique_lock lock{bucket.mutex, std::defer_lock};
            this->lock(lock);

            for (size_t i = 0; i < BUCKET_SIZE; i++)
            {
                if (bucket.entries[i].occupied && bucket.entries[i].key == key)
                {
                    bucket.entries[i].value.unmark(threadId);
                    return;
                }
            }
        }

    private:
        std::vector<Bucket> data;
        std::atomic<size_t> _size{0};
        bool threadSafe;

    public:
        class iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = TTEntry;
            using pointer = value_type *;
            using reference = value_type &;
            using difference_type = std::ptrdiff_t;

            iterator(BucketTable &table, size_t bucket_idx, size_t entry_idx) : table{table}, bucket_idx{bucket_idx}, entry_idx{entry_idx} {}

            reference operator*() const { return table.data[bucket_idx].entries[entry_idx]; }

            pointer operator->() const { return &table.data[bucket_idx].entries[entry_idx]; }

            iterator &operator++()
            {
                do
                {
                    if (++entry_idx >= BUCKET_SIZE)
                    {
                        entry_idx = 0;
                        ++bucket_idx;
                    }
                } while (bucket_idx < table.data.size() && !table.data[bucket_idx].entries[entry_idx].occupied);
                return *this;
            }

            bool operator!=(const iterator &other) const { return bucket_idx != other.bucket_idx || entry_idx != other.entry_idx; }

        private:
            BucketTable &table;
            size_t bucket_idx;
            size_t entry_idx;
        };

        class const_iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = const TTEntry;
            using pointer = const value_type *;
            using reference = const value_type &;
            using difference_type = std::ptrdiff_t;

            const_iterator(const BucketTable &table, size_t bucket_idx, size_t entry_idx) : table{table}, bucket_idx{bucket_idx}, entry_idx{entry_idx} {}

            reference operator*() const { return table.data[bucket_idx].entries[entry_idx]; }

            pointer operator->() const { return &table.data[bucket_idx].entries[entry_idx]; }

            const_iterator &operator++()
            {
                do
                {
                    if (++entry_idx >= BUCKET_SIZE)
                    {
                        entry_idx = 0;
                        ++bucket_idx;
                    }
                } while (bucket_idx < table.data.size() && !table.data[bucket_idx].entries[entry_idx].occupied);
                return *this;
            }

            bool operator!=(const const_iterator &other) const { return bucket_idx != other.bucket_idx || entry_idx != other.entry_idx; }

        private:
            const BucketTable &table;
            size_t bucket_idx;
            size_t entry_idx;
        };

        iterator begin()
        {
            size_t bucket_idx = 0;
            size_t entry_idx = 0;

            while (bucket_idx < data.size() && !data[bucket_idx].entries[entry_idx].occupied)
            {
                if (++entry_idx >= BUCKET_SIZE)
                {
                    entry_idx = 0;
                    ++bucket_idx;
                }
            }

            return iterator(*this, bucket_idx, entry_idx);
        }

        const_iterator begin() const
        {
            size_t bucket_idx = 0;
            size_t entry_idx = 0;

            while (bucket_idx < data.size() && !data[bucket_idx].entries[entry_idx].occupied)
            {
                if (++entry_idx >= BUCKET_SIZE)
                {
                    entry_idx = 0;
                    ++bucket_idx;
                }
            }

            return const_iterator(*this, bucket_idx, entry_idx);
        }

        const_iterator end() const { return const_iterator(*this, data.size(), 0); }
        iterator end() { return iterator(*this, data.size(), 0); }
    };

    template <typename Key, typename Value, typename Hash>
    void BucketTable<Key, Value, Hash>::clear()
    {
        for (auto &&bucket : data)
        {
            std::unique_lock lock{bucket.mutex, std::defer_lock};
            this->lock(lock);

            for (size_t i = 0; i < BUCKET_SIZE; i++)
                bucket.entries[i] = {};
        }

        _size.store(0);
    }

    template <typename Key, typename Value, typename Hash>
    std::optional<typename BucketTable<Key, Value, Hash>::TTEntry> BucketTable<Key, Value, Hash>::find(const Key &key) const
    {
        if (data.empty())
            return std::nullopt;

        const Bucket &bucket = data[Hash{}(key) % data.size()];
        std::shared_lock lock{bucket.mutex, std::defer_lock};
        this->lock(lock);

        for (size_t i = 0; i < BUCKET_SIZE; i++)
        {
            if (bucket.entries[i].key == key)
                return bucket.entries[i];
        }

        return std::nullopt;
    }

    template <typename Key, typename Value, typename Hash>
    void BucketTable<Key, Value, Hash>::lock(std::shared_lock<std::shared_mutex> &lock) const
    {
        if (threadSafe)
            lock.lock();
    }

    template <typename Key, typename Value, typename Hash>
    void BucketTable<Key, Value, Hash>::lock(std::unique_lock<std::shared_mutex> &lock) const
    {
        if (threadSafe)
            lock.lock();
    }
}

#endif