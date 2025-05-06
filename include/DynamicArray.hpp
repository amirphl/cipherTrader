#ifndef CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
#define CIPHER_DYNAMIC_BLAZE_ARRAY_HPP

#include "Precompiled.hpp"

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
    DynamicBlazeArray(const std::array< size_t, 2 >& shape, std::optional< size_t > drop_at = std::nullopt);

    /**
     * @brief String representation of the array
     */
    std::string toString() const;

    /**
     * @brief Get the number of elements in the array
     */
    size_t size() const;

    /**
     * @brief Get the capacity of the array
     */
    size_t capacity() const;

    /**
     * @brief Get the last item in the array
     */
    blaze::Row< const TM > getLastItem() const;

    /**
     * @brief Get an item relative to the current position
     *
     * @param past_index Number of positions back from current
     */
    blaze::Row< const TM > getPastItem(size_t past_index) const;

    /**
     * @brief Access a specific row
     *
     * @param i Row index, can be negative to index from the end
     * @return A blaze::Row view of the matrix
     */
    blaze::Row< const TM > operator[](int i) const;

    /**
     * @brief Get a slice of the array - Optimized
     *
     * @param start Start index (inclusive)
     * @param stop Stop index (exclusive)
     * @return A view to the specified slice
     */
    blaze::Submatrix< const TM > operator()(int start, int stop) const;

    /**
     * @brief Append a row to the array - Optimized
     *
     * @param item The row to append
     */
    void append(const blaze::DynamicVector< T, blaze::rowVector >& item);

    /**
     * @brief Append multiple rows at once - Optimized
     *
     * @param items Matrix of rows to append
     */
    void appendMultiple(const blaze::DynamicMatrix< T >& items);

    /**
     * @brief Clear the array
     */
    void flush();

    /**
     * @brief Delete a row from the array - Optimized
     *
     * @param idx Index to delete
     */
    void deleteRow(int idx);

    // ISSUE: Performance.
    /**
     * @brief Search for a specific row or column in the array
     *
     * @param item The row or column to search for
     * @param axis The axis to search along (0 for rows, 1 for columns)
     * @param searchByRow If true, search by row; if false, search by column
     * @return The index of the matching row or column, or -1 if not found
     */
    int find(const blaze::DynamicVector< T, blaze::rowVector >& item, size_t axis = 0) const;

    /**
     * @brief Filter rows based on a specific column value
     *
     * @param columnIndex Index of the column to filter on
     * @param filterValue Value to filter by
     * @param epsilon Optional tolerance for floating-point comparisons
     * @return TM Matrix containing only the rows that match the filter
     */
    TM filter(size_t columnIndex, const T& filterValue, double epsilon = 1e-10) const;

    T sum(size_t columnIndex) const;

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
    void reset(TM& matrix) const;

    /**
     * @brief Resize the matrix to a new capacity
     *
     * @param new_rows The new number of rows
     */
    void resize(size_t new_rows);

    /**
     * @brief Optimized function to shrink the matrix to fit
     *
     * @param new_size Desired new size, must be >= index_+1
     */
    void shrinkToFit(size_t new_size);
    /**
     * @brief Shift matrix elements by n positions - Optimized
     *
     * @param n Number of positions to shift (negative = shift left/drop from beginning)
     */
    void shiftMatrix(int n);
};

} // namespace datastructure
} // namespace ct

#endif // CIPHER_DYNAMIC_BLAZE_ARRAY_HPP
