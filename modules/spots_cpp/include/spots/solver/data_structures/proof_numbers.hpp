#ifndef PROOF_NUMBERS_H
#define PROOF_NUMBERS_H

#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include "spots/global.hpp"

namespace spots
{
    struct ProofNumbers
    {
        template <typename T>
        struct Value
        {
            static_assert(std::is_unsigned<T>::value, "Value can only be used with unsigned integral types.");

            constexpr Value(T v = 0) : value(v) {}

            static constexpr Value Inf() { return Value(std::numeric_limits<T>::max()); }

            bool is_inf() const { return value == std::numeric_limits<T>::max(); }

            static bool will_addition_overflow(T a, T b) { return a >= std::numeric_limits<T>::max() - b; }

            static bool will_multiplication_overflow(T a, T b) { return (a != 0 && b >= std::numeric_limits<T>::max() / a); }

            Value operator+(const Value &other) const
            {
                if (is_inf() || other.is_inf())
                    return Inf();

                if (will_addition_overflow(value, other.value))
                    throw std::overflow_error("Integer overflow in addition.");

                return Value(value + other.value);
            }

            Value operator-(const Value &other) const
            {
                if (is_inf())
                {
                    if (other.is_inf())
                        throw std::underflow_error("Undefined subtraction.");

                    return Inf();
                }

                if (value < other.value)
                    throw std::underflow_error("Integer underflow in subtraction.");

                return Value(value - other.value);
            }

            Value operator*(const Value &other) const
            {
                if (is_inf() || other.is_inf())
                    return Inf();

                if (will_multiplication_overflow(value, other.value))
                    throw std::overflow_error("Integer overflow in multiplication.");

                return Value(value * other.value);
            }

            Value operator/(const Value &other) const
            {
                if (other.value == 0)
                    throw std::overflow_error("Division by zero.");

                if (other.is_inf())
                    throw std::overflow_error("Integer underflow in division.");

                if (is_inf())
                    return Inf();

                return Value(value / other.value);
            }

            Value &operator+=(const Value &other)
            {
                if (is_inf() || other.is_inf())
                {
                    value = std::numeric_limits<T>::max();
                    return *this;
                }

                if (will_addition_overflow(value, other.value))
                    throw std::overflow_error("Integer overflow in addition.");

                value += other.value;
                return *this;
            }

            Value &operator-=(const Value &other)
            {
                if (is_inf())
                {
                    if (other.is_inf())
                        throw std::underflow_error("Undefined subtraction.");

                    value = std::numeric_limits<T>::max();
                    return *this;
                }

                if (value < other.value)
                    throw std::underflow_error("Integer underflow in subtraction.");

                value -= other.value;
                return *this;
            }

            Value &operator*=(const Value &other)
            {
                if (is_inf() || other.is_inf())
                {
                    value = std::numeric_limits<T>::max();
                    return *this;
                }

                if (will_multiplication_overflow(value, other.value))
                    throw std::overflow_error("Integer overflow in multiplication.");

                value *= other.value;
                return *this;
            }

            Value &operator/=(const Value &other)
            {
                if (other.value == 0)
                    throw std::overflow_error("Division by zero.");

                if (other.is_inf())
                    throw std::overflow_error("Integer underflow in division.");

                if (is_inf())
                {
                    value = std::numeric_limits<T>::max();
                    return *this;
                }

                value /= other.value;
                return *this;
            }

            bool operator==(const Value &other) const { return (is_inf() && other.is_inf()) || (value == other.value); }

            bool operator!=(const Value &other) const { return !(*this == other); }

            bool operator<(const Value &other) const
            {
                if (is_inf())
                    return false;

                if (other.is_inf())
                    return true;

                return value < other.value;
            }

            bool operator>(const Value &other) const { return other < *this; }

            bool operator<=(const Value &other) const { return !(other < *this); }

            bool operator>=(const Value &other) const { return !(*this < other); }

            std::string to_string() const
            {
                if (is_inf())
                    return "INF";

                std::stringstream ss;
                ss << value;
                return ss.str();
            }

            friend std::ostream &operator<<(std::ostream &os, const Value &obj)
            {
                os << obj.to_string();
                return os;
            }

            T getValue() const { return value; }

        private:
            T value;
        };

        using simple_value_type = uint64_t;
        using value_type = Value<simple_value_type>;

        ProofNumbers() : proof{1}, disproof{1} {}
        ProofNumbers(value_type proof, value_type disproof) : proof{proof}, disproof{disproof} {}

        bool isWin() const { return proof == 0; }
        bool isLoss() const { return disproof == 0; }
        bool isProved() const { return isWin() || isLoss(); }
        Outcome toOutcome() const { return (proof == 0) ? Outcome::Win : ((disproof == 0) ? Outcome::Loss : Outcome::Unknown); }

        std::pair<simple_value_type, simple_value_type> getValues() const { return {proof.getValue(), disproof.getValue()}; }
        std::pair<value_type, value_type> operator*() const { return {proof, disproof}; }
        std::string to_string() const { return "{" + proof.to_string() + ", " + disproof.to_string() + "}"; }

        value_type proof;
        value_type disproof;
    };

    namespace PN
    {
        using simple_value_type = ProofNumbers::simple_value_type;
        using value_type = ProofNumbers::value_type;
        static constexpr ProofNumbers::value_type INF = value_type::Inf();
    }
}

#endif