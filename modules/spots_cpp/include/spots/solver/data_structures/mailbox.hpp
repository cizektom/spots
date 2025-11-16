#ifndef MAILBOX_H
#define MAILBOX_H

#include <unordered_set>
#include <mutex>

#include "spots/solver/data_structures/couple.hpp"

namespace spots
{
    template <typename Game>
    class Mailbox
    {

    public:
        void notify(const Couple<Game>::Compact &position)
        {
            std::lock_guard<std::mutex> lock(mutex);
            messages.insert(position);
        }
        void notify(Couple<Game>::Compact &&position)
        {
            std::lock_guard<std::mutex> lock(mutex);
            messages.insert(std::move(position));
        }

        std::unordered_set<typename Couple<Game>::Compact, typename Couple<Game>::Compact::Hash> extract_all()
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (messages.empty())
                return {};

            auto result = std::exchange(messages, {});
            return result;
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex);
            messages.clear();
        }

    private:
        std::unordered_set<typename Couple<Game>::Compact, typename Couple<Game>::Compact::Hash> messages;
        mutable std::mutex mutex;
    };
}

#endif