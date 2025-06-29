#pragma once

namespace ct
{
namespace lob
{

const static size_t R_ = 50;
const static size_t C_ = 2;

template < size_t ROWS, size_t COLS >
struct LimitOrderbook
{
    std::array< std::array< double, ROWS >, COLS > data;

    // Default constructor
    LimitOrderbook() = default;

    // Copy constructor
    LimitOrderbook(const LimitOrderbook& other) noexcept : data(other.data) {}

    // Move constructor
    LimitOrderbook(LimitOrderbook&& other) noexcept : data(std::move(other.data)) {}

    // Copy assignment operator
    LimitOrderbook& operator=(const LimitOrderbook& other) noexcept
    {
        if (this != &other)
        {
            data = other.data;
        }
        return *this;
    }

    // Move assignment operator
    LimitOrderbook& operator=(LimitOrderbook&& other) noexcept
    {
        if (this != &other)
        {
            data = std::move(other.data);
        }
        return *this;
    }

    // Equality comparison operator
    bool operator==(const LimitOrderbook& other) const { return data == other.data; }

    // Inequality comparison operator
    bool operator!=(const LimitOrderbook& other) const { return !(*this == other); }

    // Addition operator
    LimitOrderbook operator+(const LimitOrderbook& other) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] + other.data[i][j];
            }
        }
        return result;
    }

    // Subtraction operator
    LimitOrderbook operator-(const LimitOrderbook& other) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] - other.data[i][j];
            }
        }
        return result;
    }

    // Multiplication operator (element-wise)
    LimitOrderbook operator*(const LimitOrderbook& other) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] * other.data[i][j];
            }
        }
        return result;
    }

    // Scalar multiplication
    LimitOrderbook operator*(double scalar) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] * scalar;
            }
        }
        return result;
    }

    // Division operator (element-wise)
    LimitOrderbook operator/(const LimitOrderbook& other) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] / other.data[i][j];
            }
        }
        return result;
    }

    // Scalar division
    LimitOrderbook operator/(double scalar) const
    {
        LimitOrderbook result;
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                result.data[i][j] = data[i][j] / scalar;
            }
        }
        return result;
    }

    // Assignment operators
    LimitOrderbook& operator+=(const LimitOrderbook& other)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] += other.data[i][j];
            }
        }
        return *this;
    }

    LimitOrderbook& operator-=(const LimitOrderbook& other)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] -= other.data[i][j];
            }
        }
        return *this;
    }

    LimitOrderbook& operator*=(const LimitOrderbook& other)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] *= other.data[i][j];
            }
        }
        return *this;
    }

    LimitOrderbook& operator*=(double scalar)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] *= scalar;
            }
        }
        return *this;
    }

    LimitOrderbook& operator/=(const LimitOrderbook& other)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] /= other.data[i][j];
            }
        }
        return *this;
    }

    LimitOrderbook& operator/=(double scalar)
    {
        for (size_t i = 0; i < COLS; ++i)
        {
            for (size_t j = 0; j < ROWS; ++j)
            {
                data[i][j] /= scalar;
            }
        }
        return *this;
    }

    // Element access operator
    std::array< double, ROWS >& operator[](size_t index) { return data[index]; }

    const std::array< double, ROWS >& operator[](size_t index) const { return data[index]; }

    // Constructor from a 2D array
    explicit LimitOrderbook(const std::array< std::array< double, ROWS >, COLS >& arr) : data(arr) {}
};

template < size_t ROWS, size_t COLS >
std::ostream& operator<<(std::ostream& os, const LimitOrderbook< ROWS, COLS >& lob)
{
    os << "Limit Orderbook<" << ROWS << ", " << COLS << "> {\n";
    for (size_t i = 0; i < COLS; ++i)
    {
        os << "  [" << i << "]: ";
        for (size_t j = 0; j < ROWS; ++j)
        {
            os << lob.data[i][j];
            if (j < ROWS - 1)
            {
                os << ", ";
            }
        }
        os << "\n";
    }
    os << "}";
    return os;
}

} // namespace lob
} // namespace ct
