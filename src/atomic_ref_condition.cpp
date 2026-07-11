// SPDX-FileCopyrightText: 2026 CERN
//
// SPDX-License-Identifier: Apache-2.0

#include "coopsync_tbb/feature_test.hpp"

#if defined(COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION) && \
    COOPSYNC_TBB_HAS_ATOMIC_REF_CONDITION == 1
#include "coopsync_tbb/atomic_ref_condition.hpp"
#endif
