#pragma once

#include <cstddef>

namespace alloc_stats {

struct Stats {
    std::size_t liveBytes;
    std::size_t totalAllocations;
};

Stats Current() noexcept;

}  // namespace alloc_stats
