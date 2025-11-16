
#include "spots/games/sprouts/boundary.hpp"
#include "spots/games/sprouts/sequence.hpp"

using namespace std;

namespace sprouts
{
    const Vertex Boundary::separator = Vertex::createBoundaryEnd();

    void Boundary::deleteDeadVertices(const int occurrences[])
    {
        vertices.erase(remove_if(vertices.begin(), vertices.end(),
                                 [&occurrences](Vertex v)
                                 {
                                     return v.is3() || (v.is2Reg() && occurrences[v.get2RegTempIndex()] >= Vertex::maxLetterDegree);
                                 }),
                       vertices.end());
    }

    void Boundary::mergeAdjacentVertices()
    {
        if (vertices.size() <= 1)
            return;

        for (auto it = vertices.begin(); (it + 1) != vertices.end(); ++it)
        {
            if (it->isLetter() && *it == *(it + 1))
            {
                it = vertices.erase(it);
                if ((it + 1) == vertices.end())
                    break;
            }
        }

        if (vertices.size() > 1 && vertices[0].isLetter() && vertices[0] == vertices[vertices.size() - 1])
            vertices.pop_back();
    }

    void Boundary::rename2RegsTo1Regs()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        Vertex::indexType next1RegIndex = 0;
        for (auto it = vertices.begin(); it != vertices.end(); ++it)
        {
            Vertex v = *it;
            Boundary::iterator itCopy = it;
            if ((v.is2Reg() || v.isTemp()) && sequence::getOccurrences(std::move(itCopy), vertices.end(), v) == 2)
            {
                auto renameTo = Vertex::create1Reg(next1RegIndex);
                replace(it, vertices.end(), v, renameTo);
                next1RegIndex++;
            }
        }
    }

    void Boundary::renameRegs(RenamingMode mode, Vertex::indexType indexMapping[], Vertex::indexType &nextFreeIndex)
    {
        for (auto &&vertex : vertices)
        {
            if (!vertex.isLetter())
                continue;

            if (((mode == RenamingMode::_1Regs || mode == RenamingMode::_1RegsTo2Regs) && vertex.is1Reg()) ||
                (mode == RenamingMode::_2RegsTemp && (vertex.is2Reg() || vertex.isTemp())))
            {
                // RenamingMode::_1Regs => get1RegIndex()
                // RenamingMode::_2RegsTemps => get2RegTempIndex()
                // RenamingMode::_1RegsTo2Regs => get1RegIndex()
                Vertex::indexType index = (mode == RenamingMode::_2RegsTemp) ? vertex.get2RegTempIndex() : vertex.get1RegIndex();
                Vertex::indexType renameToIndex;

                if (indexMapping[index] != -1)
                    renameToIndex = indexMapping[index];
                else
                {
                    renameToIndex = nextFreeIndex;
                    indexMapping[index] = nextFreeIndex;
                    nextFreeIndex++;
                }

                // RenamingMode::_1Regs => create1Reg()
                // RenamingMode::_2RegsTemps => create2Reg()
                // RenamingMode::_1RegsTo2Regs => create2Reg()
                vertex = (mode == RenamingMode::_1Regs) ? Vertex::create1Reg(renameToIndex) : Vertex::create2Reg(renameToIndex);
            }
        }
    }

    void Boundary::rename1Regs()
    {
        Vertex::indexType indexMapping[Vertex::_1RegsNumber];
        std::fill_n(indexMapping, Vertex::_1RegsNumber, -1);
        Vertex::indexType nextFreeIndex = 0;

        renameRegs(RenamingMode::_1Regs, indexMapping, nextFreeIndex);
    }

    void Boundary::sort()
    {
        if (vertices.size() <= 1)
            return;

        auto bestRotation = getRotation(0);
        size_t bestRotationSize = 0;

        for (size_t rotationSize = 1; rotationSize < vertices.size(); rotationSize++)
        {
            auto currentRotation = getRotation(rotationSize);
            if (sequence::compare(currentRotation.first, currentRotation.last, bestRotation.first, bestRotation.last))
            {
                bestRotation = std::move(currentRotation);
                bestRotationSize = rotationSize;
            }
        }

        if (bestRotationSize != 0)
        {
            auto first = vertices.begin();
            auto middle = first + bestRotationSize;
            auto last = vertices.end();

            rotate(first, middle, last);
        }
    }

    void Boundary::addToString(string &str, bool useExpanded1Reg, bool useExpanded2Reg) const
    {
        for (auto &&vertex : vertices)
            vertex.addToString(str, useExpanded1Reg, useExpanded2Reg);
    }

    optional<Boundary::SBChild> Boundary::SBChild::createSimpleChild(const Boundary &b)
    {
        Vertex v = b.vertices[0];
        if (!(v.is0() || v.is1()))
            return {}; // no child can be generated

        vector<Vertex> newVertices;
        newVertices.reserve(2);
        if (v.is0())
            newVertices.push_back(Vertex::createConnected1());
        else
            newVertices.push_back(Vertex::create3());

        newVertices.push_back(Vertex::createNew());

        return SBChild{std::move(newVertices)};
    }

    void Boundary::SBChild::initConnectedVertices(Vertex &first, Vertex &second, size_t firstVertexIndex, size_t secondVertexIndex)
    {
        if (firstVertexIndex != secondVertexIndex)
        {
            // 1Regs and 2Regs are not renamed as their new additional occurrence
            // will cause their deletion in the next reduction
            if (first.is1())
                first = Vertex::createConnected1();
            else if (first.is2())
                first = Vertex::create3();

            if (second.is1())
                second = Vertex::createConnected2();
            else if (second.is2())
                second = Vertex::create3();
        }
        else
        {
            // both are the same 1Vertex
            first = Vertex::create3();
            second = Vertex::create3();
        }
    }

    optional<Boundary::SBChild> Boundary::SBChild::createChild(const Boundary &b, size_t firstVertexIndex, size_t secondVertexIndex)
    {
        Vertex first = b.vertices[firstVertexIndex];
        Vertex second = b.vertices[secondVertexIndex];

        if ((first == second && first.isLetter()) || (first.is2() && firstVertexIndex == secondVertexIndex))
            return {}; // no child can be generated

        vector<Vertex> majorVertices;
        majorVertices.reserve(secondVertexIndex - firstVertexIndex + 1);
        vector<Vertex> minorVertices;
        minorVertices.reserve(b.size() - secondVertexIndex + firstVertexIndex + 1);

        initConnectedVertices(first, second, firstVertexIndex, secondVertexIndex);

        // init major vertices: ai ... aj # .
        if (firstVertexIndex != secondVertexIndex)
            majorVertices.push_back(first);

        for (size_t k = firstVertexIndex + 1; k < secondVertexIndex; k++)
            majorVertices.push_back(b.vertices[k]);

        majorVertices.push_back(second);
        majorVertices.push_back(Vertex::createNew());
        //

        // init minor vertices: aj ... ar a1 ... ai # .
        minorVertices.push_back(second);

        for (size_t k = secondVertexIndex + 1; k < b.size(); k++)
            minorVertices.push_back(b.vertices[k]);

        for (size_t k = 0; k < firstVertexIndex; k++)
            minorVertices.push_back(b.vertices[k]);

        minorVertices.push_back(first);
        minorVertices.push_back(Vertex::createNew());
        //

        return SBChild{std::move(majorVertices), std::move(minorVertices)};
    }

    Boundary::DBChild::DBChild(const rotation &r) : fragment{vector<Vertex>{r.first, r.last}}
    {
        Vertex &first = fragment.vertices[0];
        if (first.is0())
        {
            first = Vertex::create1();
        }
        else if (first.is1())
        {
            first = Vertex::createConnected1();
            fragment.vertices.push_back(first);
        }
        else if (first.is2())
        {
            first = Vertex::create3();
        }
        else
        {
            // 1Regs and 2Regs will be removed in the next reduction step
            fragment.vertices.push_back(first);
        }
    }

    unordered_set<Boundary::SBChild> Boundary::computeSBChildren()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        unordered_set<Boundary::SBChild> children;
        if (size() == 1)
        {
            if (auto child = SBChild::createSimpleChild(*this))
                children.insert(std::move(*child));

            return children;
        }

        for (size_t i = 0; i < size(); i++)
            for (size_t j = i; j < size(); j++)
                if (auto child = SBChild::createChild(*this, i, j))
                    children.insert(std::move(*child));

        return children;
    }

    unordered_set<Boundary::DBChild> Boundary::computeDBChildren()
    {
        assert(!sequence::contains1Reg(cbegin(), cend()));

        unordered_set<Boundary::DBChild> children;
        for (size_t rotationSize = 0; rotationSize < size(); rotationSize++)
            children.emplace(getRotation(rotationSize));

        return children;
    }
}
