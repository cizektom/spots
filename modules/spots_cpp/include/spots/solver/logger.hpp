#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <iomanip>

#include "data_structures/couple.hpp"

namespace spots
{
    /// @brief A basic for tracing currently explored branches on a terminal.
    class Logger
    {
    public:
        Logger() : maxOutputNodes{10}, updateFreq{10} {}
        Logger(size_t maxOutputNodes, long int updateFreq) : maxOutputNodes{maxOutputNodes}, updateFreq{updateFreq} {}

        void addNode() { path.emplace_back(0, 0, false); }
        void addNode(size_t currentIdx, size_t childrenNumber, bool isMultiLandNode) { path.emplace_back(currentIdx + 1, childrenNumber, isMultiLandNode); }
        void popNode() { path.pop_back(); }
        void updateLastNode(size_t currentIdx, size_t childrenNumber, bool isMultiLandNode) { path.back() = {currentIdx + 1, childrenNumber, isMultiLandNode}; }

        void log();
        void clearPath() { path.clear(); };
        void clearLog();

    private:
        size_t maxOutputNodes;
        long int updateFreq;

        bool cleared = false;
        std::vector<std::tuple<size_t, size_t, bool>> path;
        std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
    };
}

#endif