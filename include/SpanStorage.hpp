#pragma once

#include <cstddef>
#include <utility>

#include "ShrdPtr.hpp"

namespace smart::detail {

template <class T>
class SpanStorage {
  public:
    explicit SpanStorage(std::size_t size)
        : slots_(size != 0 ? new ShrdPtr<T>[size] : nullptr), size_(size) {}

    ~SpanStorage() { delete[] slots_; }

    SpanStorage(const SpanStorage&) = delete;
    SpanStorage& operator=(const SpanStorage&) = delete;

    std::size_t Size() const noexcept { return size_; }

    ShrdPtr<T>& operator[](std::size_t index) noexcept { return slots_[index]; }
    const ShrdPtr<T>& operator[](std::size_t index) const noexcept { return slots_[index]; }

    void Resize(std::size_t newSize) {
        if (newSize == size_) {
            return;
        }
        ShrdPtr<T>* fresh = newSize != 0 ? new ShrdPtr<T>[newSize] : nullptr;
        const std::size_t kept = size_ < newSize ? size_ : newSize;
        for (std::size_t i = 0; i < kept; ++i) {
            fresh[i] = std::move(slots_[i]);
        }
        delete[] slots_;
        slots_ = fresh;
        size_ = newSize;
    }

  private:
    ShrdPtr<T>* slots_;
    std::size_t size_;
};

}  // namespace smart::detail
