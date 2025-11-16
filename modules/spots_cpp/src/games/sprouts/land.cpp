#include "spots/games/sprouts/land.hpp"

using namespace std;

namespace sprouts
{
    const Vertex Land::separator = Vertex::createLandEnd();

    void Land::insertLand(Land &&land)
    {
        for (auto &&region : land.children)
            children.push_back(std::move(region));
    }

    optional<vector<Land>> Land::split()
    {
        Land copy = *this;

        vector<Land> splitLands;
        vector<Land> tempSplitLands;

        Land mergedLand;

        for (auto &&region : children)
        {
            for (auto &&land : splitLands)
            {
                if (sequence::areLinked(region.cbegin(), region.cend(), land.cbegin(), land.cend()))
                    mergedLand.insertLand(std::move(land));
                else
                    tempSplitLands.push_back(std::move(land));
            }

            if (mergedLand.size() > 0)
            {
                mergedLand.children.push_back(std::move(region));
                tempSplitLands.push_back(std::move(mergedLand));
            }
            else
                tempSplitLands.emplace_back(vector<Region>{std::move(region)});

            splitLands = std::move(tempSplitLands);
        }

        children.clear();
        if (splitLands.size() > 1)
            return splitLands;
        else
        {
            *this = std::move(copy);
            return {};
        }
    }

    void Land::deleteDeadVertices()
    {
        int occurrences[Vertex::_2RegsTempNumber] = {0};
        sequence::fill2RegTempOccurrences(occurrences, cbegin(), cend());

        apply([occurrences](Region &r)
              { r.deleteDeadVertices(occurrences); });
    }

    void Land::deleteEmptyBoudaries()
    {
        apply([](Region &r)
              { r.deleteEmptyBoundaries(); });
    }

    void Land::deleteDeadRegions()
    {
        remove_all([](const Region &r)
                   { return r.isDead(); });
    }

    void Land::mergeBoundaries()
    {
        apply([](Region &r)
              { r.mergeBoundaries(); });
    }

    void Land::rename2RegsTo2()
    {
        int occurrences[Vertex::_2RegsTempNumber] = {0};
        sequence::fill2RegTempOccurrences(occurrences, cbegin(), cend());

        for (auto &&vertex : *this)
        {
            if (vertex.is2Reg() || vertex.isTemp())
            {
                if (occurrences[vertex.get2RegTempIndex()] == 1)
                    vertex = Vertex::create2();
            }
        }
    }

    void Land::reduce()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        deleteDeadVertices();
        mergeAdjacentVertices();
        deleteEmptyBoudaries();
        deleteDeadRegions();

        rename2RegsTo1Regs();
        mergeBoundaries();
        rename2RegsTo2();
    }

    void Land::rename1Regs()
    {
        if (!sequence::contains1Reg(cbegin(), cend()))
            return;

        apply([](Region &r)
              { r.rename1Regs(); });
    }

    void Land::rename2Regs()
    {
        Vertex::indexType indexMapping[Vertex::_2RegsTempNumber];
        std::fill_n(indexMapping, Vertex::_2RegsTempNumber, (Vertex::indexType)-1);
        Vertex::indexType nextFreeIndex = 0;

        for (auto &&region : children)
            for (auto &&boundary : region.getBoundaries())
                boundary.renameRegs(Boundary::RenamingMode::_2RegsTemp, indexMapping, nextFreeIndex);
    }

    Vertex::indexType Land::findFree2RegIndex() const
    {
        Vertex::indexType nextFreeIndex = 0;
        for (auto &&vertex : *this)
            if (vertex.is2Reg() && vertex.get2RegTempIndex() >= nextFreeIndex)
                nextFreeIndex = vertex.get2RegTempIndex() + 1;

        return nextFreeIndex;
    }

    void Land::rename1RegsTo2Regs()
    {
        if (!sequence::contains1Reg(cbegin(), cend()))
            return;

        Vertex::indexType indexMapping[Vertex::_1RegsNumber];
        Vertex::indexType nextFreeIndex = findFree2RegIndex();
        for (auto &&region : children)
        {
            for (auto &&boundary : region.getBoundaries())
            {
                std::fill_n(indexMapping, Vertex::_1RegsNumber, (Vertex::indexType)-1);
                boundary.renameRegs(Boundary::RenamingMode::_1RegsTo2Regs, indexMapping, nextFreeIndex);
            }
        }
    }

    void Land::sort()
    {
        apply([](Region &r)
              { r.sort(); });

        Structure::sort();
    }

    void Land::canonize()
    {
        rename2Regs();

        sort();

        Land copy = *this;

        rename1Regs();
        rename2Regs();

        if (copy != *this)
            sort();
    }

    unordered_set<Land> Land::computeChildren()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        unordered_set<Land> landsChildren;
        vector<unordered_set<Region::SBChild>> regionsSBChildren;
        vector<unordered_set<Region::DBChild>> regionsDBChildren;
        regionsSBChildren.reserve(size());
        regionsDBChildren.reserve(size());

        for (auto &&region : children)
        {
            regionsSBChildren.push_back(region.computeSBChildren());
            regionsDBChildren.push_back(region.computeDBChildren());
        }

        vector<const Region *> unusedRegions;
        unusedRegions.reserve(size());
        for (size_t i = 0; i < size(); i++)
        {
            // add unused regions
            unusedRegions.clear();
            for (size_t j = 0; j < size(); j++)
                if (j != i)
                    unusedRegions.push_back(&children[j]);

            // add single-boundary children
            for (auto it = regionsSBChildren[i].begin(); it != regionsSBChildren[i].end();)
            {
                Region::SBChild regionChild = std::move(regionsSBChildren[i].extract(it++).value());
                landsChildren.emplace(spots::utils::to_vector(unusedRegions, std::move(regionChild.minor), std::move(regionChild.major)));
            }

            // add double-boundary children
            for (auto it = regionsDBChildren[i].begin(); it != regionsDBChildren[i].end();)
            {
                Region::DBChild regionChild = std::move(regionsDBChildren[i].extract(it++).value());
                landsChildren.emplace(spots::utils::to_vector(unusedRegions, std::move(regionChild.region)));
            }
        }

        return landsChildren;
    }

    size_t Land::estimateChildrenNumber() const
    {
        size_t estimation = 0;
        for (auto &&region : children)
            estimation += region.estimateChildrenNumber();

        return estimation;
    }
}