#ifndef NIMBER_H
#define NIMBER_H

#include "spots/global.hpp"

namespace spots
{
    struct Nimber
    {
    public:
        using value_type = uint8_t;

        Nimber() : value{0} {}
        Nimber(value_type value) : value{value} {}

        bool isLoss() const { return value == 0; }
        bool isWin() const { return value != 0; }
        static const Nimber LOSS;
        static const Nimber WIN;

        Nimber operator+(Nimber other) const { return Nimber{static_cast<value_type>(value + other.value)}; }
        Nimber operator+(value_type v) const { return Nimber{static_cast<value_type>(value + v)}; }
        Nimber &operator++()
        {
            value++;
            return *this;
        }
        bool operator<(Nimber other) const { return value < other.value; }
        bool operator==(Nimber other) const { return value == other.value; }
        bool operator!=(Nimber other) const { return value != other.value; }

        std::string to_string() const { return std::to_string(value); }

        static Nimber mergeNimbers(Nimber x, Nimber y) { return Nimber{static_cast<value_type>(x.value ^ y.value)}; }

        value_type value;
    };
}

template <>
struct std::hash<spots::Nimber>
{
    std::size_t operator()(spots::Nimber n) const { return spots::utils::getHash(n.value); }
};

#endif