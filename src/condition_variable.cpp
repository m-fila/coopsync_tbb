#include "coopsync_tbb/condition_variable.hpp"

#include <cassert>

namespace coopsync_tbb {

condition_variable::~condition_variable() {
    assert(m_waiters.empty());
}

void condition_variable::notify_one() {
    m_waiters.resume_one();
}

void condition_variable::notify_all() {
    m_waiters.resume_all();
}

}  // namespace coopsync_tbb
