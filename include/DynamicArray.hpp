#ifndef CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
#define CIPHER_DYNAMIC_BLAZE_ARRAY_HPP

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <blaze/Math.h>

namespace ct
{
namespace datastructure
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
template < typename T, class TM = blaze::DynamicMatrix< T > >
class DynamicBlazeArray
{
   private:
    int index_ = -1;                  // Current index (points to the last valid element)
    TM data_;                         // Blaze matrix for storage
    size_t bucket_size_;              // Size of each allocation bucket
    std::array< size_t, 2 > shape_;   // Shape of the matrix [rows, cols]
    std::optional< size_t > drop_at_; // When to drop elements

    // Growth factor for resizing operations (similar to std::vector)
    static constexpr double growth_factor_ = 1.5;

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
        // Initialize the matrix with proper size (using Blaze's resize)
        data_.resize(shape[0], shape[1], false);
        reset(data_);
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
     * @brief Get a slice of the array - Optimized
     *
     * @param start Start index (inclusive)
     * @param stop Stop index (exclusive)
     * @return A copy of the specified slice
     */
    TM slice(int start, int stop) const
    {
        if (index_ == -1)
        {
            // Return empty matrix for empty arrays
            return TM(0, shape_[1]);
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
            // Return empty matrix for invalid slice range
            return TM(0, shape_[1]);
        }

        // Create result matrix directly with the right size
        size_t slice_rows = static_cast< size_t >(stop - start);
        TM result(slice_rows, shape_[1]);

        // Use Blaze's submatrix for efficient slicing
        auto startRow = static_cast< size_t >(start);
        // TODO: Is it deep copy?
        blaze::submatrix(result, 0, 0, slice_rows, shape_[1]) =
            blaze::submatrix(data_, startRow, 0, slice_rows, shape_[1]);

        return result;
    }

    /**
     * @brief Append a row to the array - Optimized
     *
     * @param item The row to append
     */
    template < typename VecType >
    void append(const VecType& item)
    {
        // Check if we need to expand
        if (index_ + 1 >= static_cast< int >(data_.rows()))
        {
            // Grow with a factor similar to std::vector for better amortized performance
            size_t new_size = std::max(
                {static_cast< size_t >(1), bucket_size_, static_cast< size_t >(data_.rows() * growth_factor_)});
            resize(new_size);
        }

        ++index_;

        // Add the new item using Blaze's row assignment
        auto rowView = blaze::row(data_, static_cast< size_t >(index_));

        // Use min to avoid out-of-bounds access if item is smaller than our row width
        size_t cols_to_copy = std::min(shape_[1], static_cast< size_t >(item.size()));

        // Use efficient subvector operations
        blaze::subvector(rowView, 0, cols_to_copy) = blaze::subvector(item, 0, cols_to_copy);

        // Clear any remaining elements in the row if needed
        if (cols_to_copy < shape_[1])
        {
            blaze::subvector(rowView, cols_to_copy, shape_[1] - cols_to_copy) = 0;
        }

        // Drop logic if specified - AFTER adding the new item
        // Check periodically based on modulo of index + 1
        if (drop_at_.has_value() && ((index_ + 1) % static_cast< int >(drop_at_.value()) == 0))
        {
            size_t to_drop = drop_at_.value() / 2;
            shiftMatrix(-static_cast< int >(to_drop));
        }
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

        // Consider shrinking the matrix if it's much larger than the bucket size
        if (data_.rows() > 2 * bucket_size_)
        {
            data_.resize(bucket_size_, shape_[1], false);
        }

        // Reset values to zero
        reset(data_);
    }

    /**
     * @brief Append multiple rows at once - Optimized
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
            // Calculate more optimal new size with growth factor
            size_t required_rows = static_cast< size_t >(new_index + 1);
            size_t new_size      = std::max(required_rows, static_cast< size_t >(data_.rows() * growth_factor_));
            resize(new_size);
        }

        // Use submatrix for efficient bulk copying
        // auto target_rows = std::min(num_items, data_.rows() - static_cast< size_t >(index_ + 1));
        auto target_rows = num_items;
        auto target_cols = std::min(items.columns(), data_.columns());

        blaze::submatrix(data_, static_cast< size_t >(index_ + 1), 0, target_rows, target_cols) =
            blaze::submatrix(items, 0, 0, target_rows, target_cols);

        // Update index
        index_ = new_index;

        // Drop logic if specified - check periodically
        if (drop_at_.has_value() && ((index_ + 1) % static_cast< int >(drop_at_.value()) == 0))
        {
            size_t to_drop = drop_at_.value() / 2;
            shiftMatrix(-static_cast< int >(to_drop));
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

    /**
     * @brief Delete a row from the array - Optimized
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

        // Use Blaze's efficient submatrix operations to shift elements
        if (idx < index_)
        {
            // Move all rows after idx up by one position using submatrix
            blaze::submatrix(
                data_, static_cast< size_t >(idx), 0, static_cast< size_t >(index_ - idx), data_.columns()) =
                blaze::submatrix(
                    data_, static_cast< size_t >(idx + 1), 0, static_cast< size_t >(index_ - idx), data_.columns());
        }

        // Clear the last row
        blaze::row(data_, static_cast< size_t >(index_)) = 0;

        --index_;

        // Consider shrinking the matrix if we have a lot of unused space
        // Use a threshold similar to what std::vector might use
        if (data_.rows() > bucket_size_ && static_cast< size_t >(index_ + 1) < data_.rows() / 4)
        {
            // Shrink to half the current size but not less than bucket_size_
            size_t new_size = std::max(bucket_size_, data_.rows() / 2);

            // Only resize if it's meaningfully smaller
            if (new_size < data_.rows())
            {
                shrinkToFit(new_size);
            }
        }
    }

    // ISSUE: Performance.
    /**
     * @brief Search for a specific row or column in the array
     *
     * @param item The row or column to search for
     * @param axis The axis to search along (0 for rows, 1 for columns)
     * @param searchByRow If true, search by row; if false, search by column
     * @return The index of the matching row or column, or -1 if not found
     */
    template < typename VecType >
    int find(const VecType& item, size_t axis = 0) const
    {
        if (axis > 1)
        {
            throw std::invalid_argument("Invalid axis, must be 0 for rows or 1 for columns");
        }


        // Iterate over the rows or columns and compare each one with the item
        for (size_t i = 0; i <= static_cast< size_t >(index_); ++i)
        {
            if (axis == 0)
            {
                if (blaze::row(data_, i) == item)
                {
                    return i;
                }
            }
            else
            {
                if (blaze::column(data_, i) == item)
                {
                    return i;
                }
            }
        }

        // If no match is found, return -1
        return -1;
    }

    /**
     * @brief Get a deep copy of a row at the specified position
     *
     * @param pos Row position, can be negative to index from the end
     * @return A deep copy of the row as a blaze::DynamicVector
     */
    blaze::DynamicVector< T > getRow(int pos) const
    {
        if (index_ == -1)
        {
            throw std::out_of_range("Array is empty");
        }

        // Handle negative index
        if (pos < 0)
        {
            pos = (index_ + 1) - std::abs(pos);
        }

        if (pos < 0 || pos > index_)
        {
            throw std::out_of_range("Index out of range");
        }

        // Create a new vector with the same size as the row
        blaze::DynamicVector< T > result(shape_[1]);

        // Copy the data from the row to the new vector
        result = blaze::row(data_, static_cast< size_t >(pos));

        return result;
    }

    /**
     * @brief Filter rows based on a specific column value
     *
     * @param columnIndex Index of the column to filter on
     * @param filterValue Value to filter by
     * @param epsilon Optional tolerance for floating-point comparisons
     * @return TM Matrix containing only the rows that match the filter
     */
    TM filter(size_t columnIndex, const T& filterValue, double epsilon = 1e-10) const
    {
        if (index_ == -1)
        {
            // Return empty matrix for empty arrays
            return TM(0, shape_[1]);
        }

        // Check bounds
        if (columnIndex >= shape_[1])
        {
            throw std::out_of_range("Column index out of range");
        }

        blaze::DynamicVector< bool > mask(index_ + 1, false);
        size_t matchCount = 0;
        for (size_t i = 0; i <= static_cast< size_t >(index_); ++i)
        {
            // For floating point types, use approximate comparison with epsilon
            if constexpr (std::is_floating_point_v< T >)
            {
                if (std::abs(data_(i, columnIndex) - filterValue) <= epsilon)
                {
                    ++matchCount;
                    mask[i] = true;
                }
            }
            else
            {
                // For non-floating point types, use exact comparison
                if (data_(i, columnIndex) == filterValue)
                {
                    ++matchCount;
                    mask[i] = true;
                }
            }
        }

        TM result(matchCount, shape_[1]);

        if (matchCount == 0)
        {
            return result;
        }

        size_t row = 0;
        for (size_t i = 0; i <= index_; ++i)
        {
            if (mask[i])
            {
                blaze::row(result, row) = blaze::row(data_, i);
                ++row;
            }
        }

        return result;
    }

    T sum(size_t columnIndex)
    {
        // Check bounds
        if (columnIndex >= shape_[1])
        {
            throw std::out_of_range("Column index out of range");
        }

        // Compute the sum of the specified column
        return blaze::sum(blaze::column(data_, columnIndex));
    }

    /**
     * @brief Apply a function to the data matrix and return its result
     *
     * @tparam Func Function type
     * @tparam ReturnType Return type of the function
     * @param func Function to apply to the data matrix
     * @return ReturnType Result of the function application
     */
    template < typename Func, typename ReturnType = std::invoke_result_t< Func, const TM& > >
    ReturnType applyFunction(Func&& func) const
    {
        // Apply the function to the data matrix and return the result
        return std::forward< Func >(func)(data_);
    }

   private:
    /**
     * @brief Optimized function to reset a matrix to zeros
     */
    void reset(TM& matrix) const
    {
        // Use Blaze's built-in reset function for efficient zeroing
        blaze::reset(matrix);
    }

    /**
     * @brief Resize the matrix to a new capacity
     *
     * @param new_rows The new number of rows
     */
    void resize(size_t new_rows)
    {
        // Create a new matrix with the desired size
        TM new_data(new_rows, shape_[1]);

        // Reset immediately to ensure zeros
        reset(new_data);

        // Copy existing data using submatrix for efficiency
        if (index_ >= 0)
        {
            blaze::submatrix(new_data, 0, 0, static_cast< size_t >(index_ + 1), shape_[1]) =
                blaze::submatrix(data_, 0, 0, static_cast< size_t >(index_ + 1), shape_[1]);
        }

        // Swap data
        data_ = std::move(new_data);
    }

    /**
     * @brief Optimized function to shrink the matrix to fit
     *
     * @param new_size Desired new size, must be >= index_+1
     */
    void shrinkToFit(size_t new_size)
    {
        if (static_cast< int >(new_size) <= index_)
        {
            throw std::invalid_argument("New size must be larger than current index");
        }

        // Only resize if the new size is smaller than the current size
        if (new_size < data_.rows())
        {
            TM new_data(new_size, shape_[1]);

            // Copy just the elements we need
            blaze::submatrix(new_data, 0, 0, static_cast< size_t >(index_ + 1), shape_[1]) =
                blaze::submatrix(data_, 0, 0, static_cast< size_t >(index_ + 1), shape_[1]);

            // Swap data
            data_ = std::move(new_data);
        }
    }

    /**
     * @brief Shift matrix elements by n positions - Optimized
     *
     * @param n Number of positions to shift (negative = shift left/drop from beginning)
     */
    void shiftMatrix(int n)
    {
        if (n == 0)
            return;

        if (n > 0)
        {
            throw std::runtime_error("Right shift not implemented");
        }
        else
        {
            // Shift left (drop from beginning)
            size_t abs_n = static_cast< size_t >(std::abs(n));

            // Update index first
            index_ -= static_cast< int >(abs_n);

            // Nothing left after shift
            if (index_ < 0)
            {
                index_ = -1;
                reset(data_);
                return;
            }

            // Use Blaze's submatrix assignments for efficient shifting
            // Move data [abs_n:end] to [0:end-abs_n]
            size_t remaining_rows = static_cast< size_t >(index_ + 1);

            blaze::submatrix(data_, 0, 0, remaining_rows, shape_[1]) =
                blaze::submatrix(data_, abs_n, 0, remaining_rows, shape_[1]);

            // Clear the now unused rows
            for (size_t i = remaining_rows; i < remaining_rows + abs_n && i < data_.rows(); ++i)
            {
                blaze::row(data_, i) = 0;
            }
        }
    }
};

} // namespace datastructure
} // namespace ct

#endif // CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
