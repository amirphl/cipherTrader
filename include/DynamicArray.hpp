#ifndef CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
#define CIPHER_DYNAMIC_BLAZE_ARRAY_HPP

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <blaze/Math.h>

namespace CipherDynamicArray
{

/**
 * @brief A dynamic array implementation using Blaze library
 *
 * This class provides a dynamically resizing array with Blaze matrices
 * as the underlying storage.
 *
 * @tparam T The element type (e.g., double, float, int)
 * @tparam TM The matrix type (default: blaze::DynamicMatrix)
 */

template < typename T, class TM = blaze::DynamicMatrix< T, blaze::rowMajor > >
class DynamicBlazeArray
{
   private:
    int index_ = -1;                  // Current index (points to the last valid element)
    TM data_;                         // Now includes all four parameters
    size_t bucket_size_;              // Size of each allocation bucket
    std::array< size_t, 2 > shape_;   // Shape of the matrix [rows, cols]
    std::optional< size_t > drop_at_; // When to drop elements

    // Helper to create zero-filled matrix of the given shape
    TM createZeroMatrix(size_t rows, size_t cols) const
    {
        TM result(rows, cols);
        reset(result, T{});
        return result;
    }

    // Helper to reset a matrix to a specific value
    template < typename Matrix >
    void reset(Matrix& matrix, T value) const
    {
        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            for (size_t j = 0; j < matrix.columns(); ++j)
            {
                matrix(i, j) = value;
            }
        }
    }

    // TODO: Helper::shift
    // Shift matrix elements by n positions
    void shiftMatrix(TM& matrix, int n)
    {
        if (n == 0)
            return;

        if (n > 0)
        {
            // Shift right
            throw std::runtime_error("Right shift not implemented");
        }
        else
        {
            // Shift left (drop from beginning)
            size_t abs_n          = static_cast< size_t >(std::abs(n));
            size_t remaining_rows = matrix.rows() - abs_n;

            // Create a temporary matrix for the remaining rows
            TM temp(remaining_rows, matrix.columns());

            // Copy the elements that we're keeping
            for (size_t i = 0; i < remaining_rows; ++i)
            {
                for (size_t j = 0; j < matrix.columns(); ++j)
                {
                    temp(i, j) = matrix(i + abs_n, j);
                }
            }

            // Copy the data back to the original matrix, starting from the beginning
            for (size_t i = 0; i < remaining_rows; ++i)
            {
                for (size_t j = 0; j < matrix.columns(); ++j)
                {
                    matrix(i, j) = temp(i, j);
                }
            }

            // Fill the rest with zeros
            for (size_t i = remaining_rows; i < matrix.rows(); ++i)
            {
                for (size_t j = 0; j < matrix.columns(); ++j)
                {
                    matrix(i, j) = T{};
                }
            }
        }
    }

   public:
    /**
     * @brief Construct a new Dynamic Blaze Array
     *
     * @param shape The shape of the array [rows, cols]
     * @param drop_at Optional parameter to specify when to drop elements
     */
    DynamicBlazeArray(const std::array< size_t, 2 >& shape, std::optional< size_t > drop_at = std::nullopt)
        : bucket_size_(shape[0]), shape_(shape), drop_at_(drop_at)
    {
        // Initialize the matrix with zeros
        data_ = createZeroMatrix(shape[0], shape[1]);
    }

    /**
     * @brief String representation of the array
     */
    std::string toString() const
    {
        std::ostringstream oss;
        oss << "DynamicBlazeArray(shape=[" << data_.rows() << ", " << data_.columns() << "], size=" << (index_ + 1)
            << ")\n";

        // Add content for debugging
        if (index_ >= 0)
        {
            oss << "[\n";
            for (size_t i = 0; i <= static_cast< size_t >(index_); ++i)
            {
                oss << "  [";
                for (size_t j = 0; j < data_.columns(); ++j)
                {
                    oss << data_(i, j);
                    if (j < data_.columns() - 1)
                        oss << ", ";
                }
                oss << "]\n";
            }
            oss << "]";
        }
        else
        {
            oss << "[]";
        }

        return oss.str();
    }

    /**
     * @brief Get the number of elements in the array
     */
    size_t size() const { return static_cast< size_t >(index_ + 1); }

    /**
     * @brief Get the capacity of the array
     */
    size_t capacity() const { return data_.rows(); }

    /**
     * @brief Access a specific row
     *
     * @param i Row index, can be negative to index from the end
     * @return A blaze::Row view of the matrix
     */
    auto operator[](int i)
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        if (i < 0)
        {
            i = (index_ + 1) - std::abs(i); // Convert negative index
        }

        if (i < 0 || i > index_)
        {
            throw std::out_of_range("Index out of range");
        }

        return blaze::row(data_, static_cast< size_t >(i));
    }

    /**
     * @brief Const version of row access
     */
    auto operator[](int i) const
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        if (i < 0)
        {
            i = (index_ + 1) - std::abs(i); // Convert negative index
        }

        if (i < 0 || i > index_)
        {
            throw std::out_of_range("Index out of range");
        }

        return blaze::row(data_, static_cast< size_t >(i));
    }

    /**
     * @brief Get a slice of the array
     *
     * @param start Start index (inclusive)
     * @param stop Stop index (exclusive)
     * @return A copy of the specified slice
     */
    TM slice(int start, int stop) const
    {
        if (index_ == -1)
        {
            return createZeroMatrix(0, shape_[1]);
        }

        // Handle negative indices
        if (start < 0)
        {
            start = (index_ + 1) - std::abs(start);
        }

        if (stop == 0)
        {
            stop = index_ + 1;
        }
        else if (stop < 0)
        {
            stop = (index_ + 1) - std::abs(stop);
        }

        // Bounds checking
        start = std::max(0, start);
        stop  = std::min(stop, index_ + 1);

        if (start >= stop)
        {
            return createZeroMatrix(0, shape_[1]);
        }

        // Create result matrix
        size_t slice_rows = static_cast< size_t >(stop - start);
        TM result(slice_rows, shape_[1]);

        // Copy data
        for (size_t i = 0; i < slice_rows; ++i)
        {
            for (size_t j = 0; j < shape_[1]; ++j)
            {
                result(i, j) = data_(static_cast< size_t >(start) + i, j);
            }
        }

        return result;
    }

    /**
     * @brief Append a row to the array
     *
     * @param item The row to append
     */
    template < typename VecType >
    void append(const VecType& item)
    {
        // First increment the index
        index_++;

        // Check if we need to expand
        if (index_ >= static_cast< int >(data_.rows()))
        {
            // Calculate new size - if bucket_size_ is 0, use 1 as minimum
            size_t new_bucket_size = bucket_size_ == 0 ? 1 : bucket_size_;
            size_t new_rows        = data_.rows() + new_bucket_size;

            // Create a new bucket
            TM new_data(new_rows, shape_[1]);

            // Copy existing data
            for (size_t i = 0; i < data_.rows(); ++i)
            {
                for (size_t j = 0; j < data_.columns(); ++j)
                {
                    new_data(i, j) = data_(i, j);
                }
            }

            // Initialize new portion to zeros
            for (size_t i = data_.rows(); i < new_data.rows(); ++i)
            {
                for (size_t j = 0; j < new_data.columns(); ++j)
                {
                    new_data(i, j) = T{};
                }
            }

            // Swap the data
            data_ = std::move(new_data);

            // Update bucket_size_ if it was zero
            if (bucket_size_ == 0)
            {
                bucket_size_ = 1;
            }
        }

        // Add the new item first
        for (size_t j = 0; j < std::min(shape_[1], static_cast< size_t >(item.size())); ++j)
        {
            data_(static_cast< size_t >(index_), j) = item[j];
        }

        // Drop logic if specified - AFTER adding the new item
        if (drop_at_.has_value() && (index_ + 1) == static_cast< int >(drop_at_.value()))
        {
            size_t to_drop = drop_at_.value() / 2;
            shiftMatrix(data_, -static_cast< int >(to_drop));
            index_ -= static_cast< int >(to_drop);
        }
        // if (drop_at_.has_value() && (index_ + 1) > static_cast< int >(drop_at_.value()))
        // {
        //     size_t current_size = static_cast< size_t >(index_ + 1);
        //     size_t to_keep      = drop_at_.value();
        //     size_t to_drop      = current_size - to_keep;
        //     shiftMatrix(data_, -static_cast< int >(to_drop));
        //     index_ = static_cast< int >(to_keep) - 1;
        // }
    }
    /**
     * @brief Get the last item in the array
     */
    auto getLastItem() const
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        return blaze::row(data_, static_cast< size_t >(index_));
    }

    /**
     * @brief Get an item relative to the current position
     *
     * @param past_index Number of positions back from current
     */
    auto getPastItem(size_t past_index) const
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        if (static_cast< size_t >(index_) < past_index)
        {
            throw std::out_of_range("Past index exceeds array bounds");
        }

        return blaze::row(data_, static_cast< size_t >(index_ - past_index));
    }

    /**
     * @brief Clear the array
     */
    void flush()
    {
        index_ = -1;
        data_  = createZeroMatrix(shape_[0], shape_[1]);
    }

    /**
     * @brief Append multiple rows at once
     *
     * @param items Matrix of rows to append
     */
    template < typename MatType >
    void appendMultiple(const MatType& items)
    {
        size_t num_items = items.rows();

        // If nothing to append, return early
        if (num_items == 0)
            return;

        // Calculate new index
        int new_index = index_ + static_cast< int >(num_items);

        // Check if we need to expand
        if (new_index >= static_cast< int >(data_.rows()))
        {
            // Calculate how many new buckets we need
            size_t required_rows      = static_cast< size_t >(new_index + 1);
            size_t current_rows       = data_.rows();
            size_t additional_buckets = (required_rows - current_rows + bucket_size_ - 1) / bucket_size_;
            size_t new_rows           = current_rows + additional_buckets * bucket_size_;

            // Create expanded matrix
            TM new_data(new_rows, shape_[1]);

            // Copy existing data
            for (size_t i = 0; i < data_.rows(); ++i)
            {
                for (size_t j = 0; j < data_.columns(); ++j)
                {
                    new_data(i, j) = data_(i, j);
                }
            }

            // Initialize new portion to zeros
            for (size_t i = data_.rows(); i < new_data.rows(); ++i)
            {
                for (size_t j = 0; j < new_data.columns(); ++j)
                {
                    new_data(i, j) = T{};
                }
            }

            // Swap the data
            data_ = std::move(new_data);
        }

        // Copy the new items
        for (size_t i = 0; i < num_items; ++i)
        {
            for (size_t j = 0; j < std::min(items.columns(), data_.columns()); ++j)
            {
                data_(static_cast< size_t >(index_ + 1 + i), j) = items(i, j);
            }
        }

        // Update index
        index_ = new_index;

        // Drop logic if specified - AFTER adding all items
        if (drop_at_.has_value() && (index_ + 1) == static_cast< int >(drop_at_.value()))
        {
            size_t to_drop = drop_at_.value() / 2;
            shiftMatrix(data_, -static_cast< int >(to_drop));
            index_ -= static_cast< int >(to_drop);
        }
        // if (drop_at_.has_value() && (new_index + 1) > static_cast< int >(drop_at_.value()))
        // {
        //     size_t current_size = static_cast< size_t >(new_index + 1);
        //     size_t to_keep      = drop_at_.value();
        //     size_t to_drop      = current_size - to_keep;
        //     shiftMatrix(data_, -static_cast< int >(to_drop));
        //     new_index = static_cast< int >(to_keep) - 1;
        // }
    }

    /**
     * @brief Delete a row from the array
     *
     * @param idx Index to delete
     */
    void deleteRow(int idx)
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        // Handle negative index
        if (idx < 0)
        {
            idx = (index_ + 1) - std::abs(idx);
        }

        if (idx < 0 || idx > index_)
        {
            throw std::out_of_range("Index out of range");
        }

        // Move all rows after idx up by one
        for (int i = idx; i < index_; ++i)
        {
            for (size_t j = 0; j < data_.columns(); ++j)
            {
                data_(static_cast< size_t >(i), j) = data_(static_cast< size_t >(i + 1), j);
            }
        }

        // Clear the last row
        for (size_t j = 0; j < data_.columns(); ++j)
        {
            data_(static_cast< size_t >(index_), j) = T{};
        }

        --index_;

        // If we've deleted a lot, consider resizing to save memory
        if (data_.rows() > bucket_size_ && static_cast< size_t >(index_ + 1) < data_.rows() - bucket_size_)
        {
            TM new_data(data_.rows() - bucket_size_, shape_[1]);

            // Copy existing data
            for (int i = 0; i <= index_; ++i)
            {
                for (size_t j = 0; j < data_.columns(); ++j)
                {
                    new_data(static_cast< size_t >(i), j) = data_(static_cast< size_t >(i), j);
                }
            }

            // Initialize remaining to zeros
            for (size_t i = static_cast< size_t >(index_ + 1); i < new_data.rows(); ++i)
            {
                for (size_t j = 0; j < new_data.columns(); ++j)
                {
                    new_data(i, j) = T{};
                }
            }

            data_ = std::move(new_data);
        }
    }

    /**
     * @brief Get the underlying Blaze matrix
     */
    const TM& data() const { return data_; }

    /**
     * @brief Get a mutable reference to the underlying Blaze matrix
     */
    TM& data() { return data_; }
};
} // namespace CipherDynamicArray

#endif // CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
