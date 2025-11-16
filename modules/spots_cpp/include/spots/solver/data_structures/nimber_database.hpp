
#ifndef NIMBER_DATABASE_H
#define NIMBER_DATABASE_H

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <exception>
#include <unordered_map>
#include <optional>
#include <algorithm>

#include "nimber.hpp"

namespace spots
{
    template <typename Game>
    class NimberDatabase
    {
    public:
        NimberDatabase(bool trackNew = false, bool threadSafe = false) : threadSafe{threadSafe}, trackNew{trackNew} {}
        NimberDatabase(const NimberDatabase<Game> &other);
        NimberDatabase(NimberDatabase<Game> &&other);
        NimberDatabase<Game> &operator=(const NimberDatabase<Game> &other);
        NimberDatabase<Game> &operator=(NimberDatabase<Game> &&other);

        size_t size() const;
        void clear();
        void clearTracked();

        void setTrackNew(bool trackNew);
        void setThreadSafety(bool threadSafe);

        void insert(const typename Game::Compact &compactPosition, Nimber nimber);
        void insert(typename Game::Compact &&compactPosition, Nimber nimber);
        void insert(const Game &position, Nimber nimber) { insert(position.to_compact(), nimber); }
        std::optional<Nimber> get(const typename Game::Compact &compactPosition) const;
        std::optional<Nimber> get(const Game &position) const { return get(position.to_compact()); }
        size_t addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers);

        const std::unordered_map<typename Game::Compact, Nimber> &getNimbers() const;
        const std::unordered_map<typename Game::Compact, Nimber> &getTrackedNimbers() const;
        std::unordered_map<typename Game::Compact, Nimber> getTrackedNimbers(bool clearTracked = false);

        /// @brief Stores the database into a given file.
        void store(const std::string &filePath, bool sort = true) const;
        /// @brief Loads new nimbers from a given file.
        size_t load(const std::string &filePath);
        /// @brief Loads the database from a given file.
        static NimberDatabase load(const std::string &filePath, bool trackNew, bool threadSafe);

    private:
        /// @brief Parses a std::string representation of a position and its nimber. If succeeds,
        /// returns true and fills given references; returns false otherwise.
        static bool parseLine(const std::string &line, typename Game::Compact &compactPosition, Nimber &nimber);

        void lock(std::shared_lock<std::shared_mutex> &lock) const;
        void lock(std::unique_lock<std::shared_mutex> &lock) const;

        bool threadSafe;
        mutable std::shared_mutex mutex;
        std::unordered_map<typename Game::Compact, Nimber> data;

        bool trackNew;
        std::unordered_map<typename Game::Compact, Nimber> trackedData;
    };

    template <typename Game>
    NimberDatabase<Game>::NimberDatabase(const NimberDatabase<Game> &other) : threadSafe{other.threadSafe}, trackNew{other.trackNew}
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        data = other.data;
        trackedData = other.trackedData;
    }

    template <typename Game>
    NimberDatabase<Game>::NimberDatabase(NimberDatabase<Game> &&other) : threadSafe{other.threadSafe}, trackNew{other.trackNew}
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        data = std::move(other.data);
        trackedData = std::move(other.trackedData);
    }

    template <typename Game>
    NimberDatabase<Game> &NimberDatabase<Game>::operator=(const NimberDatabase<Game> &other)
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        threadSafe = other.threadSafe;
        data = other.data;
        trackNew = other.trackNew;
        trackedData = other.trackedData;

        return *this;
    }

    template <typename Game>
    NimberDatabase<Game> &NimberDatabase<Game>::operator=(NimberDatabase<Game> &&other)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        threadSafe = other.threadSafe;
        data = std::move(other.data);
        trackNew = other.trackNew;
        trackedData = std::move(other.trackedData);

        return *this;
    }

    template <typename Game>
    size_t NimberDatabase<Game>::size() const
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        return data.size();
    }

    template <typename Game>
    void NimberDatabase<Game>::clear()
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        data.clear();
        trackedData.clear();
    }

    template <typename Game>
    void NimberDatabase<Game>::clearTracked()
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        trackedData.clear();
    }

    template <typename Game>
    void NimberDatabase<Game>::setTrackNew(bool trackNew)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        this->trackNew = trackNew;
    }

    template <typename Game>
    void NimberDatabase<Game>::setThreadSafety(bool threadSafe)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        this->threadSafe = threadSafe;
    }

    template <typename Game>
    const std::unordered_map<typename Game::Compact, Nimber> &NimberDatabase<Game>::getNimbers() const
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        return data;
    }
    template <typename Game>
    const std::unordered_map<typename Game::Compact, Nimber> &NimberDatabase<Game>::getTrackedNimbers() const
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        return trackedData;
    }
    template <typename Game>
    std::unordered_map<typename Game::Compact, Nimber> NimberDatabase<Game>::getTrackedNimbers(bool clearTracked)
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        std::unordered_map<typename Game::Compact, Nimber> copy = trackedData;
        if (clearTracked)
            trackedData.clear();

        return copy;
    }

    template <typename Game>
    std::optional<Nimber> NimberDatabase<Game>::get(const typename Game::Compact &compactPosition) const
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        auto it = data.find(compactPosition);
        return it != data.end() ? std::optional<Nimber>{it->second} : std::nullopt;
    }

    template <typename Game>
    void NimberDatabase<Game>::insert(const typename Game::Compact &compactPosition, Nimber nimber)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        if (trackNew)
            trackedData[compactPosition] = nimber;

        data[compactPosition] = nimber;
    }

    template <typename Game>
    void NimberDatabase<Game>::insert(typename Game::Compact &&compactPosition, Nimber nimber)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        if (trackNew)
            trackedData[compactPosition] = nimber;

        data[std::move(compactPosition)] = nimber;
    }

    template <typename Game>
    size_t NimberDatabase<Game>::addNimbers(std::unordered_map<typename Game::Compact, Nimber> &&nimbers)
    {
        std::unique_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        size_t inserted = 0;
        for (auto &&[str, nim] : nimbers)
            if (data.insert({std::move(str), std::move(nim)}).second)
                inserted++;

        return inserted;
    }

    template <typename Game>
    void NimberDatabase<Game>::store(const std::string &filePath, bool sort) const
    {
        std::shared_lock lock{mutex, std::defer_lock};
        this->lock(lock);

        std::ofstream f{filePath};
        if (!f.is_open())
            throw std::ios_base::failure("File \"" + filePath + "\" could not have been opened.");

        f << ((Game::isNormalImpartial) ? "[Positions+Nimber]" : "[WinLoss_Misere:Losing_Position]") << std::endl;
        std::vector<std::string> outputStrings;
        for (auto &&[compactPosition, nimber] : data)
        {
            std::string line = compactPosition.to_string();
            if (Game::isNormalImpartial)
                line += " " + std::to_string(nimber.value);

            if (sort)
                outputStrings.push_back(line);
            else
                f << line << std::endl;
        }

        if (sort)
        {
            std::sort(outputStrings.begin(), outputStrings.end());
            for (size_t i = 0; i < outputStrings.size(); i++)
                f << outputStrings[i] << std::endl;
        }
    }

    template <typename Game>
    bool NimberDatabase<Game>::parseLine(const std::string &line, typename Game::Compact &compactPosition, Nimber &nimber)
    {
        try
        {
            size_t separatorIndex = line.find_first_of(" ");
            std::string positionStr = line.substr(0, separatorIndex);
            std::string nimberStr = (Game::isNormalImpartial) ? line.substr(separatorIndex + 1) : "0";

            compactPosition = typename Game::Compact{positionStr};

            nimber = (Nimber::value_type)std::stoul(nimberStr);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid line in the database: " << line << ", with error: " << e.what() << std::endl;
            return false;
        }
    }

    template <typename Game>
    size_t NimberDatabase<Game>::load(const std::string &filePath)
    {
        std::ifstream f{filePath};
        if (!f.is_open())
            throw std::ios_base::failure("File \"" + filePath + "\" could not be opened.");

        size_t inserted = 0;
        std::string line;
        while (getline(f, line))
        {
            if (line == "[Positions+Nimber]" || line == "[WinLoss_Misere:Losing_Position]" || line == "")
                continue;

            typename Game::Compact compactPosition;
            Nimber nimber;
            if (parseLine(line, compactPosition, nimber))
            {
                if (data.insert({compactPosition, nimber}).second)
                    inserted++;
            }
        }

        return inserted;
    }

    template <typename Game>
    NimberDatabase<Game> NimberDatabase<Game>::load(const std::string &filePath, bool trackNew, bool threadSafe)
    {
        NimberDatabase database{trackNew, threadSafe};
        database.load(filePath);
        return database;
    }

    template <typename Game>
    void NimberDatabase<Game>::lock(std::shared_lock<std::shared_mutex> &lock) const
    {
        if (threadSafe)
            lock.lock();
    }

    template <typename Game>
    void NimberDatabase<Game>::lock(std::unique_lock<std::shared_mutex> &lock) const
    {
        if (threadSafe)
            lock.lock();
    }
}

#endif