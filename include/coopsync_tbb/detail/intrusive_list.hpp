#pragma once

#include <cassert>

namespace coopsync_tbb::detail {

template <typename T>
class intrusive_list {
    public:
    struct node {
        T value;
        node *next = nullptr;
    };

    intrusive_list() noexcept = default;
    intrusive_list(const intrusive_list &) = delete;
    intrusive_list &operator=(const intrusive_list &) = delete;
    intrusive_list(intrusive_list &&other) noexcept
        : m_head(other.m_head), m_tail(other.m_tail) {
        assert_invariants();
        other.m_head = nullptr;
        other.m_tail = nullptr;
    }
    intrusive_list &operator=(intrusive_list &&) = delete;
    ~intrusive_list() = default;

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

    [[nodiscard]] bool empty() const noexcept {
        assert_invariants();
        return m_head == nullptr;
    }

    private:
    node *m_head = nullptr;
    node *m_tail = nullptr;
    inline void assert_invariants() const noexcept {
        assert((m_head == nullptr) == (m_tail == nullptr));
        assert(!m_tail || m_tail->next == nullptr);
    }
};

}  // namespace coopsync_tbb::detail
