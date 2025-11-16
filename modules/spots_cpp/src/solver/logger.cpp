#include "spots/solver/logger.hpp"

using namespace spots;
using namespace std;

void Logger::log()
{
    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastUpdate).count() > updateFreq)
    {
        cleared = false;
        lastUpdate = std::chrono::steady_clock::now();

        string logStr = "";
        size_t nodes = 0;
        for (auto &&info : path)
        {
            if (nodes >= maxOutputNodes)
                break;

            logStr += "(";
            logStr += to_string(std::get<0>(info));
            logStr += "/";
            logStr += to_string(std::get<1>(info));
            logStr += (std::get<2>(info) ? "L" : "");
            logStr += ")  ";

            nodes++;
        }

        cout << "\r" << std::setw(11 * maxOutputNodes) << std::left << logStr << std::flush;
    }
}

void Logger::clearLog()
{
    clearPath();
    if (!cleared)
    {
        std::cout << "\r" << std::setw(11 * maxOutputNodes) << "" << std::endl;
        cleared = true;
    }
}