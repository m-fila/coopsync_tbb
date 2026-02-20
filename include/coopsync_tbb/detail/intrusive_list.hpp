#pragma once

#include <cassert>
#include <utility>

namespace coopsync_tbb::detail {
/// @brief A simple intrusive singly-linked list implementation. The list does
/// not own the nodes, so it is the caller's responsibility to ensure that the
/// nodes remain valid while they are in the list.
/// @tparam T The type of the value stored in the list nodes.
/// @note The list is not thread-safe and should be externally synchronized if
/// accessed from multiple threads.
///
template <typename T>
class intrusive_list {
    public:
    /// @brief Node of the intrusive list. The user is responsible for embedding
    /// this struct in their own data structures and managing its lifetime.
    struct node {
        T value;
        node *next = nullptr;
    };

    /// @brief Constructs an empty intrusive list.
    intrusive_list() noexcept = default;

    /// @brief Copy constructor is not allowed.
    intrusive_list(const intrusive_list &) = delete;

    /// @brief Copy assignment operator is not allowed.
    intrusive_list &operator=(const intrusive_list &) = delete;

    /// @brief Move constructor. Transfers ownership of the list nodes from the
    /// source list to the new list. The source list is left in an empty state.
    /// @param other The source list to move from.
    intrusive_list(intrusive_list &&other) noexcept
        : m_head(other.m_head), m_tail(other.m_tail) {
        assert_invariants();
        other.m_head = nullptr;
        other.m_tail = nullptr;
    }

    /// @brief Move assignment operator is not allowed.
    intrusive_list &operator=(intrusive_list &&) = delete;

    /// @brief Destructor. Does not delete the nodes, as the list does not own
    /// them.
    ~intrusive_list() = default;

    /// @brief Adds a node to the back of the list. The node must not already be
    /// in the list (i.e., its next pointer must be nullptr).
    /// @param node The node to add to the list.
    /// @note The caller is responsible for ensuring that the node remains valid
    /// while it is in the list.
    void push_back(node &node) noexcept {
        assert_invariants();
        assert(node.next == nullptr);
        if (m_tail) {
            m_tail->next = &node;
        } else {
            m_head = &node;
        }
        m_tail = &node;
        assert_invariants();
    }

    /// @brief Removes and returns the node at the front of the list.
    /// @return A pointer to the node at the front of the list, or nullptr if
    /// the list is empty.
    node *pop_front() noexcept {
        assert_invariants();
        if (!m_head) {
            return nullptr;
        }
        auto *node = m_head;
        m_head = m_head->next;
        if (!m_head) {
            m_tail = nullptr;
        }
        node->next = nullptr;
        assert_invariants();
        return node;
    }

    /// @brief Checks if the list is empty.
    /// @return true if the list is empty, false otherwise.
    bool empty() const noexcept {
        assert_invariants();
        return m_head == nullptr;
    }

    void swap(intrusive_list &other) noexcept {
        using std::swap;
        swap(m_head, other.m_head);
        swap(m_tail, other.m_tail);
    }

    private:
    node *m_head = nullptr;  /// Pointer to the first node in the list, or
                             /// nullptr if the list is empty.
    node *m_tail = nullptr;  /// Pointer to the last node in the list, or
                             /// nullptr if the list is empty.
    inline void assert_invariants() const noexcept {
        assert((m_head == nullptr) == (m_tail == nullptr));
        assert(!m_tail || m_tail->next == nullptr);
    }
};

template <typename T>
void swap(intrusive_list<T> &lhs, intrusive_list<T> &rhs) noexcept {
    lhs.swap(rhs);
}

}  // namespace coopsync_tbb::detail
