#include "AllocCounter.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

namespace {

std::size_t g_liveBytes = 0;
std::size_t g_totalAllocations = 0;

constexpr std::size_t kHeaderSize = alignof(std::max_align_t);

void* Allocate(std::size_t size) noexcept {
    if (size == 0) {
        size = 1;
    }
    void* block = std::malloc(size + kHeaderSize);
    if (block == nullptr) {
        return nullptr;
    }
    std::memcpy(block, &size, sizeof(size));
    g_liveBytes += size;
    ++g_totalAllocations;
    return static_cast<char*>(block) + kHeaderSize;
}

void Deallocate(void* memory) noexcept {
    if (memory == nullptr) {
        return;
    }
    char* block = static_cast<char*>(memory) - kHeaderSize;
    std::size_t size = 0;
    std::memcpy(&size, block, sizeof(size));
    g_liveBytes -= size;
    std::free(block);
}

}  // namespace

namespace alloc_stats {

Stats Current() noexcept { return Stats{g_liveBytes, g_totalAllocations}; }

}  // namespace alloc_stats

void* operator new(std::size_t size) {
    void* memory = Allocate(size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    return memory;
}

void* operator new[](std::size_t size) {
    void* memory = Allocate(size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    return memory;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return Allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return Allocate(size); }

void operator delete(void* memory) noexcept { Deallocate(memory); }
void operator delete[](void* memory) noexcept { Deallocate(memory); }
void operator delete(void* memory, std::size_t) noexcept { Deallocate(memory); }
void operator delete[](void* memory, std::size_t) noexcept { Deallocate(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { Deallocate(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { Deallocate(memory); }
