#include "coopsync_tbb/detail/intrusive_list.hpp"

#include <gtest/gtest.h>

TEST(IntrusiveList, BasicOperations) {
    using Node = coopsync_tbb::detail::intrusive_list<int>::node;
    auto list = coopsync_tbb::detail::intrusive_list<int>{};

    auto node1 = Node{1};
    auto node2 = Node{2};
    auto node3 = Node{3};

    EXPECT_TRUE(list.empty());

    list.push_back(node1);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.pop_front()->value, 1);
    EXPECT_TRUE(list.empty());

    list.push_back(node2);
    list.push_back(node3);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.pop_front()->value, 2);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.pop_front()->value, 3);
    EXPECT_TRUE(list.empty());

    // Popping from empty list should return nullptr
    EXPECT_EQ(list.pop_front(), nullptr);
}

TEST(IntrusiveList, MoveConstructor) {
    using Node = coopsync_tbb::detail::intrusive_list<int>::node;
    auto list1 = coopsync_tbb::detail::intrusive_list<int>{};

    auto node1 = Node{1};
    auto node2 = Node{2};

    list1.push_back(node1);
    list1.push_back(node2);

    auto list2 = std::move(list1);
    EXPECT_TRUE(list1.empty());
    EXPECT_FALSE(list2.empty());
    EXPECT_EQ(list2.pop_front()->value, 1);
    EXPECT_FALSE(list2.empty());
    EXPECT_EQ(list2.pop_front()->value, 2);
    EXPECT_TRUE(list2.empty());
}
