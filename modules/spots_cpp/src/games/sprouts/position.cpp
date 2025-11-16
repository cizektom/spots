
#include "spots/games/sprouts/position.hpp"

using namespace std;

namespace sprouts
{
    const Vertex Position::separator = Vertex::createPositionEnd();

    Position::Position(const std::vector<Position> &positions) : Structure{}
    {
        for (auto &&position : positions)
            for (auto &&child : position.children)
                children.push_back(child);
    }
    Position::Position(std::vector<Position> &&positions) : Structure{}
    {
        for (auto &&position : positions)
            for (auto &&child : position.children)
                children.push_back(std::move(child));
    }

    Position::Position(const std::string &positionStr)
    {
        // remove the separator before parsing
        std::string copy = positionStr;
        copy.erase(std::remove(copy.begin(), copy.end(), getSeparatorChar()), copy.end());
        addChildren(copy);
    }

    std::vector<Position> Position::getSubgames() const
    {
        std::vector<Position> result;
        result.reserve(children.size());
        std::transform(children.cbegin(), children.cend(), std::back_inserter(result),
                       [](const Land &l)
                       { return Position{l}; });

        return result;
    }

    std::string Position::to_string() const
    {
        std::string result = Structure::to_string();
        if (result.empty())
            result += getSeparatorChar(); // append the separator only if the position is empty

        return result;
    }

    void Position::splitLands()
    {
        vector<Land> lands;
        for (auto &&land : children)
        {
            if (auto splittedLands = land.split())
                lands.insert(lands.end(), std::make_move_iterator(splittedLands->begin()),
                             std::make_move_iterator(splittedLands->end()));
            else
                lands.push_back(std::move(land));
        }

        children = std::move(lands);
    }

    void Position::simplify()
    {
        apply([](Land &l)
              { l.reduce(); });

        remove_all([](Land &l)
                   { return l.isDead(); });

        splitLands();

        apply([](Land &l)
              { l.canonize(); });

        Structure::sort();
    }

    void Position::rename1RegsTo2Regs()
    {
        apply([](Land &l)
              { l.rename1RegsTo2Regs(); });
    }

    vector<Position> Position::computeChildren() const
    {
        Position copy = *this; // generate children from a copy so that the position will not
                               // be desimplified
        copy.rename1RegsTo2Regs();

        unordered_set<Position> positionsChildren;
        vector<unordered_set<Land>> landsChildren;
        landsChildren.reserve(copy.size());

        for (auto &&land : copy.children)
            landsChildren.push_back(land.computeChildren());

        vector<const Land *> unusedLands;
        unusedLands.reserve(copy.size());
        for (size_t i = 0; i < copy.size(); i++)
        {
            // add unused lands
            unusedLands.clear();
            for (size_t j = 0; j < copy.size(); j++)
                if (j != i)
                    unusedLands.push_back(&copy.children[j]);

            for (auto it = landsChildren[i].begin(); it != landsChildren[i].end();)
            {
                Land landChild = std::move(landsChildren[i].extract(it++).value());
                Position child{spots::utils::to_vector(unusedLands, std::move(landChild))};
                child.simplify();
                positionsChildren.insert(move(child));
            }
        }

        return spots::utils::to_vector(std::move(positionsChildren));
    }

    size_t Position::estimateChildrenNumber() const
    {

        size_t estimation = 0;
        for (auto &&land : children)
            estimation += land.estimateChildrenNumber();

        return estimation;
    }

    Position::Stats Position::getStats() const
    {
        Stats stats;

        for (auto &&land : getLands())
        {
            stats.lands++;
            int landLives = 0;
            int landLen = 0;
            for (auto &&region : land.getRegions())
            {
                stats.regs++;
                int regionFullLives = 0;
                int regionHalfLives = 0;
                int regionLen = 0;
                for (auto &&boundary : region.getBoundaries())
                {
                    stats.bounds++;
                    int boundaryFullLives = 0;
                    int boundaryHalfLives = 0;
                    int boundaryLen = 0;
                    for (auto &&vertex : boundary.getVertices())
                    {
                        if (vertex.is0())
                        {
                            stats._0++;
                        }
                        else if (vertex.is1())
                        {
                            stats._1++;
                        }
                        else if (vertex.is2())
                        {
                            stats._2++;
                        }
                        else if (vertex.is1Reg())
                        {
                            stats._1Regs++;
                            stats.max1Reg = max(stats.max1Reg, (float)vertex.get1RegIndex());
                        }
                        else if (vertex.is2Reg())
                        {
                            stats._2Regs++;
                            stats.max2Reg = max(stats.max2Reg, (float)vertex.get2RegTempIndex());
                        }

                        if (vertex.isLetter())
                        {
                            boundaryHalfLives++;
                        }
                        else
                        {
                            boundaryFullLives += vertex.getLives();
                        }
                        boundaryLen++;
                    }

                    stats.boundariesLives.emplace_back(boundaryFullLives + (float)boundaryHalfLives / 2);
                    regionFullLives += boundaryFullLives;
                    regionHalfLives += boundaryHalfLives;
                    regionLen += boundaryLen;

                    stats.minBoundLen = min(stats.minBoundLen, (float)boundaryLen);
                    stats.maxBoundLen = max(stats.maxBoundLen, (float)boundaryLen);
                    stats.minBoundLives = min(stats.minBoundLives, boundaryFullLives + (float)boundaryHalfLives / 2);
                    stats.maxBoundLives = max(stats.maxBoundLives, boundaryFullLives + (float)boundaryHalfLives / 2);
                }

                stats.regionLives.emplace_back(regionFullLives + (float)regionHalfLives / 2);
                landLives += regionFullLives + (float)regionHalfLives / 2;
                landLen += regionLen;

                stats.minRegLen = min(stats.minRegLen, (float)regionLen);
                stats.maxRegLen = max(stats.maxRegLen, (float)regionLen);
                stats.minRegLives = min(stats.minRegLives, regionFullLives + (float)regionHalfLives / 2);
                stats.maxRegLives = max(stats.maxRegLives, regionFullLives + (float)regionHalfLives / 2);
            }

            stats.lives += landLives;
            stats.len += landLen;

            stats.minLandLen = min(stats.minLandLen, (float)landLen);
            stats.maxLandLen = max(stats.maxLandLen, (float)landLen);
            stats.minLandLives = min(stats.minLandLives, (float)landLives);
            stats.maxLandLives = max(stats.maxLandLives, (float)landLives);
        }

        stats.avgLandLen = (float)stats.len / stats.lands;
        stats.avgLandLives = (float)stats.lives / stats.lands;
        stats.avgRegLen = (float)stats.len / stats.regs;
        stats.avgRegLives = (float)stats.lives / stats.regs;
        stats.avgBoundLen = (float)stats.len / stats.bounds;
        stats.avgBoundLives = (float)stats.lives / stats.bounds;

        stats._1Regs /= 2;
        stats._2Regs /= 2;

        stats.isMultiLand = stats.lands > 1;

        return stats;
    }

    void Position::Stats::toRelative(int rootLives)
    {
        lives /= rootLives;
        len /= rootLives;
        _0 /= rootLives;
        _1 /= rootLives;
        _2 /= rootLives;
        _1Regs /= rootLives;
        max1Reg /= rootLives;
        _2Regs /= rootLives;
        max2Reg /= rootLives;

        bounds /= rootLives;
        avgBoundLen /= rootLives;
        minBoundLen /= rootLives;
        maxBoundLen /= rootLives;
        avgBoundLives /= rootLives;
        minBoundLives /= rootLives;
        maxBoundLives /= rootLives;

        regs /= rootLives;
        avgRegLen /= rootLives;
        minRegLen /= rootLives;
        maxRegLen /= rootLives;
        avgRegLives /= rootLives;
        minRegLives /= rootLives;
        maxRegLives /= rootLives;

        lands /= rootLives;
        avgLandLen /= rootLives;
        minLandLen /= rootLives;
        maxLandLen /= rootLives;
        avgLandLives /= rootLives;
        minLandLives /= rootLives;
        maxLandLives /= rootLives;

        for (auto &&lives : boundariesLives)
        {
            lives /= rootLives;
        }

        for (auto &&lives : regionLives)
        {
            lives /= rootLives;
        }
    }
}