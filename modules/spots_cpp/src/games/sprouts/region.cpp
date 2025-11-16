#include "spots/games/sprouts/region.hpp"

#include <exception>

using namespace std;

namespace sprouts
{
    const Vertex Region::separator = Vertex::createRegionEnd();

    bool Region::isDead() const
    {
        int lives = 0;

        for (auto &&vertex : *this)
        {
            if (vertex.isLetter())
                lives += 1;
            else
                lives += vertex.getLives();

            if (lives >= 2)
                break;
        }

        return lives < 2;
    }

    void Region::deleteDeadVertices(const int occurrences[])
    {
        apply([occurrences](Boundary &b)
              { b.deleteDeadVertices(occurrences); });
    }

    void Region::deleteEmptyBoundaries()
    {
        remove_all([](const Boundary &b)
                   { return b.empty(); });
    }

    void Region::mergeBoundaries()
    {
        int halfLives = 0;
        for (auto &&vertex : *this)
        {
            if (vertex.is1Reg())
                halfLives++;
            else
                halfLives += 2 * vertex.getLives();

            if (halfLives > 6)
                break;
        }

        if (halfLives == 4 || halfLives == 6)
        {
            vector<Vertex> mergedBoundaryVertices;
            mergedBoundaryVertices.reserve(3);

            int half_2 = 0;
            for (auto &&vertex : *this)
            {
                if (vertex.is1Reg())
                    half_2++;
                else if (vertex.is2())
                    half_2 += 2;
                else if (vertex.isReal())
                    mergedBoundaryVertices.push_back(vertex);
            }

            for (int i = 0; i < half_2 / 2; i++)
                mergedBoundaryVertices.push_back(Vertex::create2());

            children.clear();
            children.emplace_back(move(mergedBoundaryVertices));
        }
    }

    void Region::rename1Regs()
    {
        apply([](Boundary &b)
              { b.rename1Regs(); });
    }

    void Region::reverseOrientation()
    {
        apply([](Boundary &b)
              { b.reverseOrientation(); });
    }

    void Region::sortBoundaries()
    {
        apply([](Boundary &b)
              { b.sort(); });

        Structure::sort();
    }

    void Region::sort()
    {
        sortBoundaries();
        Region saved = Region{children};

        reverseOrientation();
        sortBoundaries();

        if (sequence::compare(saved.cbeginSeps(), saved.cendSeps(), cbeginSeps(), cendSeps()))
            children = move(saved.children); // unreversed sort was better => revert back
    }

    Region::SBChild::SBChild(const Boundary::SBChild &child,
                             const Partition &partition) : major{spots::utils::to_vector(child.major, partition.firstPart.children)},
                                                           minor{spots::utils::to_vector(child.minor, partition.secondPart.children)} {}

    Region::DBChild::DBChild(const Boundary::DBChild &child1, const Boundary::DBChild &child2,
                             const std::vector<const Boundary *> &unusedBoundaries)
    {
        auto &fragment1 = child1.fragment.getVertices();
        auto &fragment2 = child2.fragment.getVertices();

        // create new boundary by connecting fragments by their first occurrences
        vector<Vertex> connectedBoundary;
        connectedBoundary.reserve(fragment1.size() + fragment2.size() + 2);

        for (auto &&vertex : fragment1)
            connectedBoundary.push_back(vertex);
        connectedBoundary.push_back(Vertex::createNew());

        for (auto &&vertex : fragment2)
            connectedBoundary.push_back(vertex);
        connectedBoundary.push_back(Vertex::createNew());

        if (fragment1[0].isConnected1() && fragment2[0].isConnected1())
        {
            // rename Connected1 to Connected2 in the part of fragment2
            connectedBoundary[connectedBoundary.size() - 1 - fragment2.size()] = Vertex::createConnected2();
            connectedBoundary[connectedBoundary.size() - 2] = Vertex::createConnected2();
        }
        //

        region = Region{spots::utils::to_vector(unusedBoundaries, std::move(connectedBoundary))};
    }

    unordered_set<Region::SBChild> Region::computeSBChildren()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        unordered_set<Region::SBChild> regionsChildren;
        vector<unordered_set<Boundary::SBChild>> boundariesChildren;
        boundariesChildren.reserve(size() - getSBStart());

        size_t start = getSBStart();
        for (size_t i = start; i < size(); i++)
            boundariesChildren.push_back(children[i].computeSBChildren());

        vector<const Boundary *> unusedBoundaries;
        unusedBoundaries.reserve(size());
        for (size_t i = start; i < size(); i++)
        {
            // fill unusedBoundaries
            unusedBoundaries.clear();
            for (size_t j = 0; j < size(); j++)
                if (j != i)
                    unusedBoundaries.push_back(&children[j]);
            //

            auto partitions = partitionBoundaries(unusedBoundaries);
            for (auto &&child : boundariesChildren[i - start])
                for (auto &&partition : partitions)
                    regionsChildren.emplace(child, partition);
        }

        return regionsChildren;
    }

    unordered_set<Region::DBChild> Region::computeDBChildren()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));
        if (size() < 2)
            return {};

        unordered_set<Region::DBChild> regionsChildren;
        vector<unordered_set<Boundary::DBChild>> boundariesChildren;
        boundariesChildren.reserve(size() - getDBStart());

        size_t start = getDBStart();
        for (size_t i = start; i < size(); i++)
            boundariesChildren.push_back(children[i].computeDBChildren());

        vector<const Boundary *> unusedBoundaries;
        unusedBoundaries.reserve(size());
        for (size_t i = start; i < size(); i++)
        {
            for (size_t j = i + 1; j < size(); j++)
            {
                // fill unusedBoundaries
                unusedBoundaries.clear();
                for (size_t k = 0; k < size(); k++)
                    if (k != i && k != j)
                        unusedBoundaries.push_back(&children[k]);
                //

                for (auto &&child1 : boundariesChildren[i - start])
                    for (auto &&child2 : boundariesChildren[j - start])
                        regionsChildren.emplace(child1, child2, unusedBoundaries);
            }
        }

        return regionsChildren;
    }

    size_t Region::getFirstNonSingletonIndex()
    {
        size_t firstNonSingletonIndex = 0;
        while (firstNonSingletonIndex < size() && children[firstNonSingletonIndex].isSingleton())
            firstNonSingletonIndex++;

        return firstNonSingletonIndex;
    }

    size_t Region::getSBStart()
    {
        size_t firstNonSingletonIndex = getFirstNonSingletonIndex();
        return (firstNonSingletonIndex > 0) ? firstNonSingletonIndex - 1 : 0;
    }

    size_t Region::getDBStart()
    {
        size_t firstNonSingletonIndex = getFirstNonSingletonIndex();
        return (firstNonSingletonIndex > 1) ? firstNonSingletonIndex - 2 : 0;
    }

    size_t Region::getPartitionsNumber(size_t boundariesNumber)
    {
        size_t avaiableBits = sizeof(size_t) * 8;
        if (avaiableBits < boundariesNumber + 1)
            throw overflow_error("Too many boundaries to be partitioned using size_t.");

        return ((size_t)1) << boundariesNumber;
    }

    vector<Region::Partition> Region::partitionSingletons(size_t singletonsNumber)
    {
        if (singletonsNumber == 0)
            return {};

        vector<Region::Partition> partitions;
        vector<Boundary> firstPart;
        vector<Boundary> secondPart{singletonsNumber, Boundary::createSingleton()};
        partitions.reserve(singletonsNumber + 1);
        firstPart.reserve(singletonsNumber);

        partitions.emplace_back(firstPart, secondPart);
        while (!secondPart.empty())
        {
            firstPart.push_back(Boundary::createSingleton());
            secondPart.pop_back();

            partitions.emplace_back(firstPart, secondPart);
        }

        return partitions;
    }

    vector<Region::Partition> Region::partitionNonSingletons(const std::vector<const Boundary *> &boundaries)
    {
        if (boundaries.empty())
            return {};

        size_t partitionsNumber = getPartitionsNumber(boundaries.size());

        unordered_set<Partition> partitions;
        vector<Boundary> firstPart;
        vector<Boundary> secondPart;

        partitions.reserve(partitionsNumber);
        firstPart.reserve(boundaries.size());
        secondPart.reserve(boundaries.size());

        for (size_t binaryPartition = 0; binaryPartition < partitionsNumber; binaryPartition++)
        {
            // the binary represenation of the variable `binaryPartition` correspond to a single partition
            // of boundaries
            // if x-th position in binary `i` equals 0, the boundary will be assigned to the first part;
            // otherwise to the second part
            firstPart.clear();
            secondPart.clear();

            size_t currentSplitIndex = 1;
            for (size_t u = 0; u < boundaries.size(); u++)
            {
                bool isFirst = (binaryPartition & currentSplitIndex) == currentSplitIndex;
                if (isFirst)
                    firstPart.push_back(*boundaries[u]);
                else
                    secondPart.push_back(*boundaries[u]);

                currentSplitIndex <<= 1;
            }

            partitions.emplace(std::move(firstPart), std::move(secondPart));
        }

        return spots::utils::to_vector(std::move(partitions));
    }

    vector<Region::Partition> Region::partitionBoundaries(const std::vector<const Boundary *> &boundaries)
    {
        if (boundaries.empty())
            return vector<Partition>{Partition{}};

        size_t singletonsNumber = 0;
        vector<const Boundary *> nonSingletons;

        for (auto &&boundaryPtr : boundaries)
        {
            if (boundaryPtr->isSingleton())
                singletonsNumber++;
            else
                nonSingletons.push_back(boundaryPtr);
        }

        auto singletonPartitions = partitionSingletons(singletonsNumber);
        auto nonSingletonPartitions = partitionNonSingletons(nonSingletons);

        if (singletonPartitions.empty())
            return nonSingletonPartitions;

        if (nonSingletonPartitions.empty())
            return singletonPartitions;

        vector<Partition> partitions;
        partitions.reserve(singletonPartitions.size() * nonSingletonPartitions.size());

        for (auto &&singletonPartition : singletonPartitions)
        {
            for (auto &&nonSingletonPartition : nonSingletonPartitions)
            {
                vector<Boundary> firstPartition{singletonPartition.firstPart.children};
                firstPartition.insert(firstPartition.end(),
                                      nonSingletonPartition.firstPart.children.begin(),
                                      nonSingletonPartition.firstPart.children.end());

                vector<Boundary> secondPartition{singletonPartition.secondPart.children};
                secondPartition.insert(secondPartition.end(),
                                       nonSingletonPartition.secondPart.children.begin(),
                                       nonSingletonPartition.secondPart.children.end());

                partitions.emplace_back(std::move(firstPartition), std::move(secondPartition));
            }
        }

        return partitions;
    }

    size_t Region::estimateChildrenNumber() const
    {
        size_t estimation = 0;

        // DB children estimate
        for (size_t i = 0; i < children.size(); i++)
            for (size_t j = i + 1; j < children.size(); j++)
                estimation += children[i].size() * children[j].size();

        // SB children estimate
        if (children.size() != 0)
        {
            size_t partionsNumber = ((size_t)1) << (children.size() - 1);
            for (auto &&boundary : children)
            {
                size_t vertexNumber = boundary.size();
                estimation += vertexNumber * vertexNumber * partionsNumber / 2;
            }
        }

        return estimation;
    }
}