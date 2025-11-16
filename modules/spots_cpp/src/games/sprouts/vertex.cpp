#include "spots/games/sprouts/vertex.hpp"

#include <exception>

using namespace std;

namespace sprouts
{
    uint Vertex::getLives() const
    {
        if (isLetter())
            return 1;
        else if (value <= _3Value)
            return 3 - value;
        else
            return 0;
    }

    void Vertex::addToString(string &str, bool useExpanded1Reg, bool useExpanded2Reg) const
    {
        if (value == _0Value)
            str += _0Char;
        else if (value == _1Value)
            str += _1Char;
        else if (value == _2Value)
            str += _2Char;
        else if (value == _3Value)
            str += _3Char;
        else if (is1Reg())
        {
            if (useExpanded1Reg)
            {
                str += expanded1RegChar;
                str += expansionLPar;
                str += to_string(get1RegIndex() + 1);
                str += expansionRPar;
            }
            else
                str += (char)(value - first1RegValue + first1RegChar);
        }
        else if (is2Reg())
        {
            if (useExpanded2Reg)
            {
                str += expanded2RegChar;
                str += expansionLPar;
                str += to_string(get2RegTempIndex() + 1);
                str += expansionRPar;
            }
            else
                str += (char)(value - first2RegValue + first2RegChar);
        }
        else if (value == boundaryEndValue)
            str += boundaryEndChar;
        else if (value == regionEndValue)
            str += regionEndChar;
        else if (value == landEndValue)
            str += landEndChar;
        else if (value == positionEndValue)
            str += positionEndChar;
        else if (isConnected1())
            str += connected1Char;
        else if (isConnected2())
            str += connected2Char;
        else if (isNew())
            str += newChar;
        else
            str += invalidChar;
    }

    vector<Vertex> Vertex::parseString(const string &seq)
    {
        vector<Vertex> vertices;
        string expanded_seq = expandSingletons(seq);

        bool expanded1Reg = false;
        bool expanded2Reg = false;
        indexType expandedIndex = 0;

        for (auto &&c : expanded_seq)
        {
            if (expanded1Reg || expanded2Reg)
            {
                switch (c)
                {
                case expansionLPar:
                    break;
                case expansionRPar:
                {
                    if (expandedIndex <= 0)
                        throw domain_error("Invalid value of an expanded vertex.");

                    if (expanded1Reg)
                    {
                        vertices.push_back(create1Reg(expandedIndex - 1));
                        expanded1Reg = false;
                    }
                    else if (expanded2Reg)
                    {
                        vertices.push_back(create2Reg(expandedIndex - 1));
                        expanded2Reg = false;
                    }

                    expandedIndex = 0;
                    break;
                }
                default:
                {
                    if (c >= '0' && c <= '9')
                        expandedIndex = 10 * expandedIndex + (c - '0');
                    else
                        throw domain_error("Invalid expansion of a vertex.");

                    break;
                }
                }
            }
            else
            {
                Vertex v;
                switch (c)
                {
                case boundaryEndChar:
                {
                    v = createBoundaryEnd();
                    break;
                }
                case regionEndChar:
                {
                    v = createRegionEnd();
                    break;
                }
                case landEndChar:
                {
                    v = createLandEnd();
                    break;
                }
                case positionEndChar:
                {
                    v = createPositionEnd();
                    break;
                }
                case _0Char:
                {
                    v = create0();
                    break;
                }
                case _1Char:
                {
                    v = create1();
                    break;
                }
                case _2Char:
                {
                    v = create2();
                    break;
                }
                case _3Char:
                {
                    v = create3();
                    break;
                }
                case connected1Char:
                {
                    v = createConnected1();
                    break;
                }
                case connected2Char:
                {
                    v = createConnected2();
                    break;
                }
                case newChar:
                {
                    v = createNew();
                    break;
                }
                default:
                {
                    if (c >= first1RegChar && c <= last1RegChar)
                        v = create1Reg(c - first1RegChar);
                    else if (c >= first2RegChar && c <= last2RegChar)
                        v = create2Reg(c - first2RegChar);
                    else if (c == expanded1RegChar)
                        expanded1Reg = true;
                    else if (c == expanded2RegChar)
                        expanded2Reg = true;

                    break;
                }
                }

                if (!v.isInvalid())
                    vertices.push_back(v);
            }
        }

        return vertices;
    }

    string Vertex::shortenSingletons(const string &seq)
    {
        enum class ShorteningPattern
        {
            noPattern,
            firstSingleton,
            firstBoundaryEnd,
            anotherSingleton,
            anotherBoundaryEnd
        };

        string result;
        ShorteningPattern currentPattern = ShorteningPattern::noPattern;
        int currentZeroesNumber = 0;

        for (auto &&c : seq)
        {
            if (currentPattern == ShorteningPattern::noPattern && c == _0Char)
            {
                currentPattern = ShorteningPattern::firstSingleton;
                currentZeroesNumber = 1;
                result += c;
            }
            else if (currentPattern == ShorteningPattern::noPattern && c != _0Char)
            {
                result += c;
            }
            else if (currentPattern == ShorteningPattern::firstSingleton && c == boundaryEndChar)
            {
                currentPattern = ShorteningPattern::firstBoundaryEnd;
            }
            else if (currentPattern == ShorteningPattern::firstSingleton && c != boundaryEndChar)
            {
                currentPattern = ShorteningPattern::noPattern;
                currentZeroesNumber = 0;
                result += c;
            }
            else if (currentPattern == ShorteningPattern::firstBoundaryEnd && c == _0Char)
            {
                // this is a new 0 in the pattern
                currentPattern = ShorteningPattern::anotherSingleton;
                currentZeroesNumber = 2;
                result += multipleSingletonsChar;
            }
            else if (currentPattern == ShorteningPattern::firstBoundaryEnd && c != _0Char)
            {
                currentPattern = ShorteningPattern::noPattern;
                currentZeroesNumber = 0;
                result += boundaryEndChar;
                result += c;
            }
            else if (currentPattern == ShorteningPattern::anotherSingleton && c == boundaryEndChar)
            {
                currentPattern = ShorteningPattern::anotherBoundaryEnd;
            }
            else if (currentPattern == ShorteningPattern::anotherSingleton && c != boundaryEndChar)
            {
                // end of the pattern
                currentPattern = ShorteningPattern::noPattern;
                result += to_string(currentZeroesNumber);
                currentZeroesNumber = 0;
                result += c;
            }
            else if (currentPattern == ShorteningPattern::anotherBoundaryEnd && c == _0Char)
            {
                currentPattern = ShorteningPattern::anotherSingleton;
                currentZeroesNumber++;
            }
            else if (currentPattern == ShorteningPattern::anotherBoundaryEnd && c != _0Char)
            {
                // end of the pattern
                currentPattern = ShorteningPattern::noPattern;
                result += to_string(currentZeroesNumber);
                result += boundaryEndChar;
                result += c;
            }
        }

        if (currentPattern == ShorteningPattern::anotherSingleton)
            result += to_string(currentZeroesNumber);

        return result;
    }

    string Vertex::expandSingletons(const string &seq)
    {
        enum class ExpandingPattern
        {
            noPattern,
            firstSingletonFound,
            asteriskFound
        };

        string result;
        ExpandingPattern currentPattern = ExpandingPattern::noPattern;
        int singletonsNumber = 0;

        for (auto &&c : seq)
        {
            if (currentPattern == ExpandingPattern::noPattern && c == _0Char)
                currentPattern = ExpandingPattern::firstSingletonFound;
            else if (currentPattern == ExpandingPattern::noPattern && c != _0Char)
                result += c;
            else if (currentPattern == ExpandingPattern::firstSingletonFound && c != multipleSingletonsChar)
            {
                result += _0Char;
                result += c;
                currentPattern = ExpandingPattern::noPattern;
            }
            else if (currentPattern == ExpandingPattern::firstSingletonFound && c == multipleSingletonsChar)
                currentPattern = ExpandingPattern::asteriskFound;
            else if (currentPattern == ExpandingPattern::asteriskFound && c >= '0' && c <= '9')
                singletonsNumber = singletonsNumber * 10 + (c - '0');
            else
            {
                expandZeroes(result, singletonsNumber);
                result += c;
                currentPattern = ExpandingPattern::noPattern;
                singletonsNumber = 0;
            }
        }

        if (currentPattern == ExpandingPattern::firstSingletonFound)
            result += _0Char;
        else if (currentPattern == ExpandingPattern::asteriskFound)
            expandZeroes(result, singletonsNumber);

        return result;
    }

    void Vertex::expandZeroes(string &seq, int zerosNumber)
    {
        if (zerosNumber <= 0)
            return;

        seq += _0Char;
        for (int i = 1; i < zerosNumber; ++i)
        {
            seq += boundaryEndChar;
            seq += _0Char;
        }
    }
}