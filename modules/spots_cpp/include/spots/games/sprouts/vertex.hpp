#ifndef VERTEX_H
#define VERTEX_H

#include <assert.h>
#include <string>
#include <functional>

#include "spots/global.hpp"

namespace sprouts
{
    struct Vertex
    {

    public:
        using indexType = short;
        Vertex() : value{invalidValue} {}

        static Vertex create0() { return Vertex{_0Value}; }
        static Vertex create1() { return Vertex{_1Value}; }
        static Vertex create2() { return Vertex{_2Value}; }
        static Vertex create3() { return Vertex{_3Value}; }
        static Vertex createConnected1() { return Vertex{connected1Value}; }
        static Vertex createConnected2() { return Vertex{connected2Value}; }
        static Vertex createNew() { return Vertex{newValue}; }
        /// @param index Zero-based index of 1 region vertex.
        static Vertex create1Reg(indexType index)
        {
            if (index >= maximum1Reg2RegNumber)
                throw std::overflow_error("1Reg vertex with index " + std::to_string(index) + " reaches the maximum limit " + std::to_string(Vertex::maximum1Reg2RegNumber));

            return Vertex{(indexType)(first1RegValue + index)};
        }
        /// @param index Zero-based index of 2 region vertex.
        static Vertex create2Reg(indexType index)
        {
            if (index >= maximum1Reg2RegNumber)
                throw std::overflow_error("2Reg vertex with index " + std::to_string(index) + " reaches the maximum limit " + std::to_string(Vertex::maximum1Reg2RegNumber));

            return Vertex{(indexType)(first2RegValue + index)};
        }
        static Vertex createBoundaryEnd() { return Vertex{boundaryEndValue}; }
        static Vertex createRegionEnd() { return Vertex{regionEndValue}; }
        static Vertex createLandEnd() { return Vertex{landEndValue}; }
        static Vertex createPositionEnd() { return Vertex{positionEndValue}; }

        static constexpr char getBoundaryEndChar() { return boundaryEndChar; }
        static constexpr char getRegionEndChar() { return regionEndChar; }
        static constexpr char getLandEndChar() { return landEndChar; }
        static constexpr char getPositionEndChar() { return positionEndChar; }

        bool isInvalid() const { return value == invalidValue; }
        bool is0() const { return value == _0Value; }
        bool is1() const { return value == _1Value; }
        bool is2() const { return value == _2Value; }
        bool is3() const { return value == _3Value; }
        bool is1Reg() const { return first1RegValue <= value && value <= last1RegValue; }
        bool is2Reg() const { return first2RegValue <= value && value <= last2RegValue; }
        bool isConnected1() const { return value == connected1Value; }
        bool isConnected2() const { return value == connected2Value; }
        bool isNew() const { return value == newValue; }
        /// @brief Temporary vertices are connected ones and a new one.
        bool isTemp() const { return value == connected1Value || value == connected2Value || value == newValue; }
        /// @brief All vertices except end ones are real.
        bool isReal() const { return value <= lastVertex; }
        /// @brief Letter vertices are region and temporary vertices.
        bool isLetter() const { return firstLetterVertexValue <= value && value <= lastVertex; }
        bool isBoundaryEnd() const { return value == boundaryEndValue; }
        bool isRegionEnd() const { return value == regionEndValue; }
        bool isLandEnd() const { return value == landEndValue; }
        bool isPositionEnd() const { return value == positionEndValue; }

        uint getLives() const;
        indexType get1RegIndex() const
        {
            assert(is1Reg());
            return value - first1RegValue;
        }
        /// @brief Returns an index of a 2Reg or Temps.
        indexType get2RegTempIndex() const
        {
            assert(is2Reg() || isTemp());
            return value - first2RegValue;
        }

        /// @brief Adds representation of the vertex to a given string.
        /// @param useExpanded_1Reg If true, m() notation will be used.
        /// @param useExpanded_2Reg If true, n() notation will be used.
        void addToString(std::string &str, bool useExpanded1Reg, bool useExpanded2Reg) const;
        /// @brief Returns true if this vertex must be represented by m(index + 1) notation.
        bool requiresExpanded1Reg() const { return value <= last1RegValue && value > first1RegValue + (last1RegChar - first1RegChar); }
        /// @brief Returns true if this vertex must be represented by n(index + 1) notation.
        bool requiresExpanded2Reg() const { return value <= last2RegValue && value > first2RegValue + (last2RegChar - first2RegChar); }

        static std::string create0String(int singletons)
        {
            assert(singletons >= 0);
            return expandSingletons(std::string{"0"} + multipleSingletonsChar + std::to_string(singletons));
        }

        /// @brief Parses a given string into a sequnce of vertices.
        static std::vector<Vertex> parseString(const std::string &seq);

        /// @brief Shortens consecutive singletons (0) in a given string using * notation.
        static std::string shortenSingletons(const std::string &seq);
        /// @brief Expands a given string in * notation into consecutive singletons (0).
        static std::string expandSingletons(const std::string &seq);

        /// @brief A basic comparison with a given vertex (order: 0<1<2<a<b<c<...<A<B<C<...<.<|<+<!).
        bool operator<(Vertex other) const { return value < other.value; }
        bool operator==(Vertex v) const { return value == v.value; }
        bool operator!=(Vertex v) const { return value != v.value; }

        friend struct std::hash<Vertex>;

    private:
        Vertex(indexType value) : value{value} {}

        /// @brief Integer representation of the vertex.
        indexType value;

        /// @brief Adds a given number of zeroes into a given string separeted by boundary ends.
        static void expandZeroes(std::string &seq, int zerosNumber);

        // --value representation--
        // generic vertices
        static constexpr indexType _0Value = 0;
        static constexpr indexType _1Value = 1;
        static constexpr indexType _2Value = 2;
        static constexpr indexType _3Value = 3;
        // region vertices
        static constexpr indexType maximum1Reg2RegNumber = 200;
        static constexpr indexType first1RegValue = 4;
        static constexpr indexType last1RegValue = first1RegValue + maximum1Reg2RegNumber - 1;
        static constexpr indexType first2RegValue = last1RegValue + 1;
        static constexpr indexType last2RegValue = first2RegValue + maximum1Reg2RegNumber - 1;
        // temporary vertices
        static constexpr indexType connected1Value = last2RegValue + 1;
        static constexpr indexType connected2Value = last2RegValue + 2;
        static constexpr indexType newValue = last2RegValue + 3;
        // vertices ranges
        static constexpr indexType firstLetterVertexValue = first1RegValue;
        static constexpr indexType lastVertex = newValue;
        // end vertices
        static constexpr indexType boundaryEndValue = last2RegValue + 4;
        static constexpr indexType regionEndValue = last2RegValue + 5;
        static constexpr indexType landEndValue = last2RegValue + 6;
        static constexpr indexType positionEndValue = last2RegValue + 7;

        static constexpr indexType invalidValue = -1;

        // --string representation--
        // generic vertices
        static constexpr char _1Char = '1';
        static constexpr char _2Char = '2';
        static constexpr char _0Char = '0';
        static constexpr char _3Char = '3';
        // region vertices
        static constexpr char first1RegChar = 'a';
        static constexpr char last1RegChar = 'l';
        static constexpr char first2RegChar = 'A';
        static constexpr char last2RegChar = 'Y';
        // expansion characters
        static constexpr char expanded1RegChar = 'm';
        static constexpr char expanded2RegChar = 'n';
        static constexpr char expansionLPar = '(';
        static constexpr char expansionRPar = ')';
        // temporary vertices
        static constexpr char connected1Char = '-';
        static constexpr char connected2Char = '=';
        static constexpr char newChar = '#';

        // end vertices
        static constexpr char boundaryEndChar = '.';
        static constexpr char regionEndChar = '|';
        static constexpr char landEndChar = '+';
        static constexpr char positionEndChar = '!';

        // special symbols
        static constexpr char multipleSingletonsChar = '*';
        static constexpr char invalidChar = '?';

    public:
        static constexpr indexType _1RegsNumber = maximum1Reg2RegNumber;
        static constexpr indexType _2RegsTempNumber = lastVertex - first2RegValue + 1;
        static constexpr indexType maxLetterDegree = 3;
    };
}

template <>
struct std::hash<sprouts::Vertex>
{
    std::size_t operator()(sprouts::Vertex v) const { return spots::utils::getHash(v.value); }
};

#endif