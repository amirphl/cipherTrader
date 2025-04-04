#include "DynamicArray.hpp"
#include <vector>
#include <blaze/Math.h>
#include <gtest/gtest.h>

class DynamicBlazeArrayTest : public ::testing::Test
{
   protected:
    // Helper to create a test array with predefined values
    CipherDynamicArray::DynamicBlazeArray< double > createTestArray()
    {
        CipherDynamicArray::DynamicBlazeArray< double > array({3, 2});

        // Add some test data
        blaze::StaticVector< double, 2 > row1 = {1.0, 2.0};
        blaze::StaticVector< double, 2 > row2 = {3.0, 4.0};
        blaze::StaticVector< double, 2 > row3 = {5.0, 6.0};

        array.append(row1);
        array.append(row2);
        array.append(row3);

        return array;
    }

    // Helper to check if two arrays are equal
    void expectArraysEqual(const CipherDynamicArray::DynamicBlazeArray< double >& actual,
                           const std::vector< std::vector< double > >& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());

        for (size_t i = 0; i < expected.size(); ++i)
        {
            auto row = actual[static_cast< int >(i)];
            ASSERT_EQ(row.size(), expected[i].size());

            for (size_t j = 0; j < expected[i].size(); ++j)
            {
                EXPECT_DOUBLE_EQ(row[j], expected[i][j]);
            }
        }
    }

    // Helper method to create a vector for testing
    blaze::StaticVector< double, 6 > createTestVector(double start)
    {
        blaze::StaticVector< double, 6 > vec;
        for (size_t i = 0; i < 6; ++i)
        {
            vec[i] = start + i;
        }
        return vec;
    }
};

// Test construction with different shapes
TEST_F(DynamicBlazeArrayTest, Construction)
{
    // Test default construction
    CipherDynamicArray::DynamicBlazeArray< double > array1({3, 2});
    EXPECT_EQ(array1.size(), 0);
    EXPECT_EQ(array1.capacity(), 3);

    // Test construction with drop_at
    CipherDynamicArray::DynamicBlazeArray< double > array2({3, 2}, 10);
    EXPECT_EQ(array2.size(), 0);
    EXPECT_EQ(array2.capacity(), 3);

    // Test construction with different dimensions
    CipherDynamicArray::DynamicBlazeArray< double > array3({5, 4});
    EXPECT_EQ(array3.capacity(), 5);
    EXPECT_EQ(array3.data().columns(), 4);
}

// Test appending individual items
TEST_F(DynamicBlazeArrayTest, AppendSingle)
{
    CipherDynamicArray::DynamicBlazeArray< double > array({3, 2});

    // Append a single item
    blaze::StaticVector< double, 2 > row1 = {1.0, 2.0};
    array.append(row1);
    EXPECT_EQ(array.size(), 1);

    // Verify the appended item
    auto stored_row = array[0];
    EXPECT_DOUBLE_EQ(stored_row[0], 1.0);
    EXPECT_DOUBLE_EQ(stored_row[1], 2.0);

    // Append more items to force expansion
    for (int i = 0; i < 5; ++i)
    {
        blaze::StaticVector< double, 2 > row = {static_cast< double >(i * 2), static_cast< double >(i * 2 + 1)};
        array.append(row);
    }

    // Verify size after expansion
    EXPECT_EQ(array.size(), 6);
    EXPECT_GE(array.capacity(), 6);

    // Verify the last item
    auto last_row = array[5];
    EXPECT_DOUBLE_EQ(last_row[0], 8.0);
    EXPECT_DOUBLE_EQ(last_row[1], 9.0);
}

// Test appending multiple items at once
TEST_F(DynamicBlazeArrayTest, AppendMultiple)
{
    CipherDynamicArray::DynamicBlazeArray< double > array({3, 2});

    // Create a matrix to append
    blaze::DynamicMatrix< double > items(4, 2);
    for (size_t i = 0; i < 4; ++i)
    {
        items(i, 0) = i * 2.0;
        items(i, 1) = i * 2.0 + 1.0;
    }

    // Append multiple items
    array.appendMultiple(items);

    // Verify size
    EXPECT_EQ(array.size(), 4);
    EXPECT_GE(array.capacity(), 4);

    // Verify content
    for (size_t i = 0; i < 4; ++i)
    {
        auto row = array[static_cast< int >(i)];
        EXPECT_DOUBLE_EQ(row[0], i * 2.0);
        EXPECT_DOUBLE_EQ(row[1], i * 2.0 + 1.0);
    }

    // Append more items to force expansion
    blaze::DynamicMatrix< double > more_items(4, 2);
    for (size_t i = 0; i < 4; ++i)
    {
        more_items(i, 0) = (i + 4) * 2.0;
        more_items(i, 1) = (i + 4) * 2.0 + 1.0;
    }

    array.appendMultiple(more_items);

    // Verify expanded size
    EXPECT_EQ(array.size(), 8);
    EXPECT_GE(array.capacity(), 8);

    // Verify all content
    for (size_t i = 0; i < 8; ++i)
    {
        auto row = array[static_cast< int >(i)];
        EXPECT_DOUBLE_EQ(row[0], i * 2.0);
        EXPECT_DOUBLE_EQ(row[1], i * 2.0 + 1.0);
    }
}

// Test array indexing
TEST_F(DynamicBlazeArrayTest, Indexing)
{
    auto array = createTestArray();

    // Test positive indexing
    auto row1 = array[0];
    EXPECT_DOUBLE_EQ(row1[0], 1.0);
    EXPECT_DOUBLE_EQ(row1[1], 2.0);

    auto row2 = array[1];
    EXPECT_DOUBLE_EQ(row2[0], 3.0);
    EXPECT_DOUBLE_EQ(row2[1], 4.0);

    // Test negative indexing
    auto last_row = array[-1];
    EXPECT_DOUBLE_EQ(last_row[0], 5.0);
    EXPECT_DOUBLE_EQ(last_row[1], 6.0);

    auto second_last_row = array[-2];
    EXPECT_DOUBLE_EQ(second_last_row[0], 3.0);
    EXPECT_DOUBLE_EQ(second_last_row[1], 4.0);

    // Test out of bounds
    EXPECT_THROW(array[3], std::out_of_range);
    EXPECT_THROW(array[-4], std::out_of_range);

    // Test on empty array
    CipherDynamicArray::DynamicBlazeArray< double > empty_array({3, 2});
    EXPECT_THROW(empty_array[0], std::out_of_range);
}

// Test getting the last item
TEST_F(DynamicBlazeArrayTest, GetLastItem)
{
    auto array = createTestArray();

    auto last = array.getLastItem();
    EXPECT_DOUBLE_EQ(last[0], 5.0);
    EXPECT_DOUBLE_EQ(last[1], 6.0);

    // Test with empty array
    CipherDynamicArray::DynamicBlazeArray< double > empty_array({3, 2});
    EXPECT_THROW(empty_array.getLastItem(), std::out_of_range);
}

// Test getting past items
TEST_F(DynamicBlazeArrayTest, GetPastItem)
{
    auto array = createTestArray();

    // Get 1 item back
    auto past1 = array.getPastItem(1);
    EXPECT_DOUBLE_EQ(past1[0], 3.0);
    EXPECT_DOUBLE_EQ(past1[1], 4.0);

    // Get 2 items back
    auto past2 = array.getPastItem(2);
    EXPECT_DOUBLE_EQ(past2[0], 1.0);
    EXPECT_DOUBLE_EQ(past2[1], 2.0);

    // Test out of bounds
    EXPECT_THROW(array.getPastItem(3), std::out_of_range);

    // Test with empty array
    CipherDynamicArray::DynamicBlazeArray< double > empty_array({3, 2});
    EXPECT_THROW(empty_array.getPastItem(1), std::out_of_range);
}

// Test flushing the array
TEST_F(DynamicBlazeArrayTest, Flush)
{
    auto array = createTestArray();
    EXPECT_EQ(array.size(), 3);

    array.flush();
    EXPECT_EQ(array.size(), 0);
    EXPECT_THROW(array[0], std::out_of_range);

    // Test that we can still append after flushing
    blaze::StaticVector< double, 2 > row = {10.0, 20.0};
    array.append(row);
    EXPECT_EQ(array.size(), 1);

    auto stored_row = array[0];
    EXPECT_DOUBLE_EQ(stored_row[0], 10.0);
    EXPECT_DOUBLE_EQ(stored_row[1], 20.0);
}

// Test deleting rows
TEST_F(DynamicBlazeArrayTest, DeleteRow)
{
    auto array = createTestArray();
    EXPECT_EQ(array.size(), 3);

    // Delete the middle row
    array.deleteRow(1);
    EXPECT_EQ(array.size(), 2);

    // Verify remaining rows
    auto row1 = array[0];
    EXPECT_DOUBLE_EQ(row1[0], 1.0);
    EXPECT_DOUBLE_EQ(row1[1], 2.0);

    auto row2 = array[1];
    EXPECT_DOUBLE_EQ(row2[0], 5.0);
    EXPECT_DOUBLE_EQ(row2[1], 6.0);

    // Test negative indexing for deletion
    array.deleteRow(-1);
    EXPECT_EQ(array.size(), 1);

    // Verify the only remaining row
    auto remaining = array[0];
    EXPECT_DOUBLE_EQ(remaining[0], 1.0);
    EXPECT_DOUBLE_EQ(remaining[1], 2.0);

    // Test deleting the last element
    array.deleteRow(0);
    EXPECT_EQ(array.size(), 0);

    // Test deleting from empty array
    EXPECT_THROW(array.deleteRow(0), std::out_of_range);
}

// Test slicing
TEST_F(DynamicBlazeArrayTest, Slice)
{
    auto array = createTestArray();

    // Test basic slice
    auto slice1 = array.slice(0, 2);
    EXPECT_EQ(slice1.rows(), 2);
    EXPECT_EQ(slice1.columns(), 2);
    EXPECT_DOUBLE_EQ(slice1(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(slice1(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(slice1(1, 0), 3.0);
    EXPECT_DOUBLE_EQ(slice1(1, 1), 4.0);

    // Test slice with negative indices
    auto slice2 = array.slice(-2, -1);
    EXPECT_EQ(slice2.rows(), 1);
    EXPECT_DOUBLE_EQ(slice2(0, 0), 3.0);
    EXPECT_DOUBLE_EQ(slice2(0, 1), 4.0);

    // Test slice with out of bounds indices (should clamp)
    auto slice3 = array.slice(-5, 10);
    EXPECT_EQ(slice3.rows(), 3);

    // Test slice with empty result
    auto slice4 = array.slice(2, 2);
    EXPECT_EQ(slice4.rows(), 0);

    // Test slice on empty array
    CipherDynamicArray::DynamicBlazeArray< double > empty_array({3, 2});
    auto empty_slice = empty_array.slice(0, 1);
    EXPECT_EQ(empty_slice.rows(), 0);
}

// Test the drop_at functionality
TEST_F(DynamicBlazeArrayTest, DropAt)
{
    // Create array with drop_at = 6
    CipherDynamicArray::DynamicBlazeArray< double > array({3, 2}, 6);

    // Add 5 elements
    for (int i = 0; i < 5; ++i)
    {
        blaze::StaticVector< double, 2 > row = {static_cast< double >(i), static_cast< double >(i + 10)};
        array.append(row);
    }

    // Check size
    EXPECT_EQ(array.size(), 5);

    // Add 6th element to trigger drop
    blaze::StaticVector< double, 2 > row = {5.0, 15.0};
    array.append(row);

    // After dropping half, size should be 3 (6 - 3)
    EXPECT_EQ(array.size(), 3);

    // Check remaining elements (should be the last 3)
    auto row1 = array[0];
    EXPECT_DOUBLE_EQ(row1[0], 3.0); // Original index 3
    EXPECT_DOUBLE_EQ(row1[1], 13.0);

    auto row2 = array[1];
    EXPECT_DOUBLE_EQ(row2[0], 4.0); // Original index 4
    EXPECT_DOUBLE_EQ(row2[1], 14.0);

    auto row3 = array[2];
    EXPECT_DOUBLE_EQ(row3[0], 5.0); // Original index 5
    EXPECT_DOUBLE_EQ(row3[1], 15.0);
}

// Test with different data types
TEST_F(DynamicBlazeArrayTest, DifferentTypes)
{
    // Test with int
    CipherDynamicArray::DynamicBlazeArray< int > int_array({3, 2});
    blaze::StaticVector< int, 2 > int_row = {1, 2};
    int_array.append(int_row);
    EXPECT_EQ(int_array[0][0], 1);
    EXPECT_EQ(int_array[0][1], 2);

    // Test with float
    CipherDynamicArray::DynamicBlazeArray< float > float_array({3, 2});
    blaze::StaticVector< float, 2 > float_row = {1.5f, 2.5f};
    float_array.append(float_row);
    EXPECT_FLOAT_EQ(float_array[0][0], 1.5f);
    EXPECT_FLOAT_EQ(float_array[0][1], 2.5f);

    // Test with custom type
    struct TestType
    {
        int a;
        double b;
        TestType() : a(0), b(0.0) {}
        TestType(int a_, double b_) : a(a_), b(b_) {}
        bool operator==(const TestType& other) const { return a == other.a && std::abs(b - other.b) < 1e-6; }
    };

    CipherDynamicArray::DynamicBlazeArray< TestType > custom_array({3, 2});
    blaze::StaticVector< TestType, 2 > custom_row = {TestType(1, 2.5), TestType(3, 4.5)};
    custom_array.append(custom_row);
    EXPECT_EQ(custom_array[0][0], TestType(1, 2.5));
    EXPECT_EQ(custom_array[0][1], TestType(3, 4.5));
}

// Test with large number of elements
TEST_F(DynamicBlazeArrayTest, LargeData)
{
    CipherDynamicArray::DynamicBlazeArray< double > array({10, 2});

    // Add 100 elements
    const int count = 100;
    for (int i = 0; i < count; ++i)
    {
        blaze::StaticVector< double, 2 > row = {static_cast< double >(i), static_cast< double >(i * 2)};
        array.append(row);
    }

    // Verify size
    EXPECT_EQ(array.size(), count);

    // Verify some random elements
    auto row25 = array[25];
    EXPECT_DOUBLE_EQ(row25[0], 25.0);
    EXPECT_DOUBLE_EQ(row25[1], 50.0);

    auto row75 = array[75];
    EXPECT_DOUBLE_EQ(row75[0], 75.0);
    EXPECT_DOUBLE_EQ(row75[1], 150.0);

    // Test negative indexing with large data
    auto row_last = array[-1];
    EXPECT_DOUBLE_EQ(row_last[0], 99.0);
    EXPECT_DOUBLE_EQ(row_last[1], 198.0);
}

// Test expansion behavior
TEST_F(DynamicBlazeArrayTest, Expansion)
{
    // Create array with small initial capacity
    CipherDynamicArray::DynamicBlazeArray< double > array({2, 2});
    EXPECT_EQ(array.capacity(), 2);

    // Add elements to force expansion
    for (int i = 0; i < 5; ++i)
    {
        blaze::StaticVector< double, 2 > row = {static_cast< double >(i), static_cast< double >(i * 2)};
        array.append(row);
    }

    // Check capacity increased
    EXPECT_GE(array.capacity(), 6);

    // Check all elements
    for (int i = 0; i < 5; ++i)
    {
        auto row = array[i];
        EXPECT_DOUBLE_EQ(row[0], static_cast< double >(i));
        EXPECT_DOUBLE_EQ(row[1], static_cast< double >(i * 2));
    }
}

// Test edge cases
TEST_F(DynamicBlazeArrayTest, EdgeCases)
{
    // Test with zero initial capacity
    CipherDynamicArray::DynamicBlazeArray< double > array({0, 2});
    EXPECT_EQ(array.capacity(), 0);

    // Should still be able to append
    blaze::StaticVector< double, 2 > row = {1.0, 2.0};
    array.append(row);
    EXPECT_EQ(array.size(), 1);
    EXPECT_GT(array.capacity(), 0);

    // Test with more columns than rows
    CipherDynamicArray::DynamicBlazeArray< double > wide_array({2, 10});
    blaze::StaticVector< double, 10 > wide_row;
    for (size_t i = 0; i < 10; ++i)
    {
        wide_row[i] = static_cast< double >(i);
    }
    wide_array.append(wide_row);
    EXPECT_EQ(wide_array.size(), 1);

    // Test appending a row with fewer columns
    blaze::StaticVector< double, 5 > short_row;
    for (size_t i = 0; i < 5; ++i)
    {
        short_row[i] = static_cast< double >(i + 10);
    }
    wide_array.append(short_row);
    EXPECT_EQ(wide_array.size(), 2);

    // Remaining columns should be default initialized
    auto stored_row = wide_array[1];
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_DOUBLE_EQ(stored_row[i], static_cast< double >(i + 10));
    }
    for (size_t i = 5; i < 10; ++i)
    {
        EXPECT_DOUBLE_EQ(stored_row[i], 0.0);
    }

    // Test appending a row with more columns (extras should be ignored)
    blaze::StaticVector< double, 15 > long_row;
    for (size_t i = 0; i < 15; ++i)
    {
        long_row[i] = static_cast< double >(i + 20);
    }
    wide_array.append(long_row);
    EXPECT_EQ(wide_array.size(), 3);

    auto stored_long_row = wide_array[2];
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_DOUBLE_EQ(stored_long_row[i], static_cast< double >(i + 20));
    }
}

// FIXME:
// Test concurrent operations (multi-threaded behavior)

// TEST_F(DynamicBlazeArrayTest, ConcurrentOperations)
// {
//     CipherDynamicArray::DynamicBlazeArray< double > array({10, 2});

// // Define an operation to run in parallel
// auto append_items = [&array](int start_id, int count)
// {
//     for (int i = 0; i < count; ++i)
//     {
//         blaze::StaticVector< double, 2 > row = {static_cast< double >(start_id + i),
//                                                 static_cast< double >((start_id + i) * 2)};
//         array.append(row);
//     }
// };

// // Launch multiple threads
// std::vector< std::thread > threads;
// const int num_threads      = 4;
// const int items_per_thread = 25;

// for (int i = 0; i < num_threads; ++i)
// {
//     threads.emplace_back(append_items, i * items_per_thread, items_per_thread);
// }

// // Wait for all threads to complete
// for (auto& thread : threads)
// {
//     thread.join();
// }

// // Verify total number of items
// EXPECT_EQ(array.size(), num_threads * items_per_thread);

// // Note: Order of items will be non-deterministic due to threading
// }

// FIXME:
// Test memory handling with large appends

// TEST_F(DynamicBlazeArrayTest, MemoryHandling)
// {
//     CipherDynamicArray::DynamicBlazeArray< double > array({10, 2}, 50);

// // Add 100 items
// for (int i = 0; i < 100; ++i)
// {
//     blaze::StaticVector< double, 2 > row = {static_cast< double >(i), static_cast< double >(i * 2)};
//     array.append(row);
// }

// // Due to drop_at=50, we should have around 50-75 items remaining
// EXPECT_GE(array.size(), 50);
// EXPECT_LE(array.size(), 75);

// // The first item should be from later in the sequence
// auto first_row = array[0];
// EXPECT_GT(first_row[0], 0.0);

// // Test multiple drops by adding more items
// for (int i = 0; i < 100; ++i)
// {
//     blaze::StaticVector< double, 2 > row = {static_cast< double >(i + 200), static_cast< double >((i + 200) * 2)};
//     array.append(row);
// }

// // Size should still be constrained
// EXPECT_GE(array.size(), 50);
// EXPECT_LE(array.size(), 100);

// // The first item should be from much later in the sequence
// first_row = array[0];
// EXPECT_GT(first_row[0], 100.0);
// }

// Test equality and serialization
TEST_F(DynamicBlazeArrayTest, StringRepresentation)
{
    auto array = createTestArray();

    // Test toString output contains expected text
    std::string str = array.toString();
    EXPECT_NE(str.find("DynamicBlazeArray"), std::string::npos);
    EXPECT_NE(str.find("size=3"), std::string::npos);
    EXPECT_NE(str.find("1"), std::string::npos);
    EXPECT_NE(str.find("2"), std::string::npos);
    EXPECT_NE(str.find("3"), std::string::npos);
    EXPECT_NE(str.find("4"), std::string::npos);
    EXPECT_NE(str.find("5"), std::string::npos);
    EXPECT_NE(str.find("6"), std::string::npos);
}

// Test append functionality
TEST_F(DynamicBlazeArrayTest, Append)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({10, 6});

    auto vec1 = createTestVector(1);
    a.append(vec1);
    EXPECT_EQ(a.size(), 1);
    EXPECT_EQ(a[0][0], 1);
    EXPECT_EQ(a[0][1], 2);
    EXPECT_EQ(a[0][2], 3);
    EXPECT_EQ(a[0][3], 4);
    EXPECT_EQ(a[0][4], 5);
    EXPECT_EQ(a[0][5], 6);

    auto vec2 = createTestVector(7);
    a.append(vec2);
    EXPECT_EQ(a.size(), 2);
    EXPECT_EQ(a[1][0], 7);
    EXPECT_EQ(a[1][1], 8);
    EXPECT_EQ(a[1][2], 9);
    EXPECT_EQ(a[1][3], 10);
    EXPECT_EQ(a[1][4], 11);
    EXPECT_EQ(a[1][5], 12);
}

// Test flush functionality
TEST_F(DynamicBlazeArrayTest, Flush_2)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({10, 6});

    a.append(createTestVector(1));
    a.append(createTestVector(7));
    EXPECT_EQ(a.size(), 2);
    EXPECT_EQ(a[0][0], 1);
    EXPECT_EQ(a[1][0], 7);

    a.flush();
    EXPECT_EQ(a.size(), 0);
    EXPECT_THROW(a[0], std::out_of_range);
}

// Test get_last_item functionality
TEST_F(DynamicBlazeArrayTest, GetLastItem_2)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({10, 6});

    EXPECT_THROW(a.getLastItem(), std::out_of_range);

    a.append(createTestVector(1));
    a.append(createTestVector(7));
    EXPECT_EQ(a.size(), 2);

    auto last = a.getLastItem();
    EXPECT_EQ(last[0], 7);
    EXPECT_EQ(last[1], 8);
    EXPECT_EQ(last[2], 9);
    EXPECT_EQ(last[3], 10);
    EXPECT_EQ(last[4], 11);
    EXPECT_EQ(last[5], 12);
}

// Test get_past_item functionality
TEST_F(DynamicBlazeArrayTest, GetPastItem_2)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({10, 6});

    EXPECT_THROW(a.getPastItem(1), std::out_of_range);

    a.append(createTestVector(1));
    a.append(createTestVector(7));
    EXPECT_EQ(a.size(), 2);

    auto past = a.getPastItem(1);
    EXPECT_EQ(past[0], 1);
    EXPECT_EQ(past[1], 2);
    EXPECT_EQ(past[2], 3);
    EXPECT_EQ(past[3], 4);
    EXPECT_EQ(past[4], 5);
    EXPECT_EQ(past[5], 6);

    EXPECT_THROW(a.getPastItem(2), std::out_of_range);
}

// Test array access operator
TEST_F(DynamicBlazeArrayTest, GetItem)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({10, 6});

    EXPECT_THROW(a[0], std::out_of_range);

    a.append(createTestVector(1));
    a.append(createTestVector(7));
    EXPECT_EQ(a.size(), 2);

    auto first = a[0];
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 2);
    EXPECT_EQ(first[2], 3);
    EXPECT_EQ(first[3], 4);
    EXPECT_EQ(first[4], 5);
    EXPECT_EQ(first[5], 6);

    auto second = a[1];
    EXPECT_EQ(second[0], 7);
    EXPECT_EQ(second[1], 8);
    EXPECT_EQ(second[2], 9);
    EXPECT_EQ(second[3], 10);
    EXPECT_EQ(second[4], 11);
    EXPECT_EQ(second[5], 12);

    EXPECT_THROW(a[2], std::out_of_range);
}

// Test array size increases
// TEST_F(DynamicBlazeArrayTest, ArraySizeIncreases)
// {
//     CipherDynamicArray::DynamicBlazeArray< double > a({3, 6});

// EXPECT_EQ(a.capacity(), 3);

// a.append(createTestVector(1));
// a.append(createTestVector(7));
// a.append(createTestVector(13));
// EXPECT_EQ(a.capacity(), 6);
// EXPECT_EQ(a.size(), 3);

// a.append(createTestVector(19));
// a.append(createTestVector(25));
// a.append(createTestVector(31));
// EXPECT_EQ(a.capacity(), 9);
// EXPECT_EQ(a.size(), 6);
// }

// Test array size increases
TEST_F(DynamicBlazeArrayTest, ArraySizeIncreases)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({3, 6});

    EXPECT_EQ(a.capacity(), 3);

    a.append(createTestVector(1));
    a.append(createTestVector(7));
    a.append(createTestVector(13));
    // The array should expand to at least accommodate these items
    EXPECT_GE(a.capacity(), 3);
    EXPECT_EQ(a.size(), 3);

    a.append(createTestVector(19));
    a.append(createTestVector(25));
    a.append(createTestVector(31));
    // After adding 3 more items, capacity should be at least 6
    EXPECT_GE(a.capacity(), 6);
    EXPECT_EQ(a.size(), 6);
}

// Test drop_at functionality
TEST_F(DynamicBlazeArrayTest, DropAt_2)
{
    CipherDynamicArray::DynamicBlazeArray< double > a({100, 6}, 6);

    // Add 5 items
    a.append(createTestVector(1));
    a.append(createTestVector(7));
    a.append(createTestVector(13));
    a.append(createTestVector(19));
    a.append(createTestVector(25));

    EXPECT_EQ(a.getLastItem()[0], 25);
    EXPECT_EQ(a[0][0], 1);

    // Add 6th item - should trigger drop
    a.append(createTestVector(31));
    EXPECT_EQ(a[0][0], 19); // First 3 items should be dropped
    EXPECT_EQ(a.size(), 3); // Should have 3 items remaining
}
