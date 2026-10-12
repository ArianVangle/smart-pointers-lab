#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "ShrdPtr.hpp"
#include "SpanStorage.hpp"

namespace smart {

template <class T>
class MemorySpan;

template <class T>
class MsPtr {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;

    MsPtr() noexcept = default;

    T& operator*() const { return *Slot(); }
    T* operator->() const { return Slot().Get(); }
    T& operator[](std::ptrdiff_t offset) const { return *(*this + offset); }

    MsPtr& operator+=(std::ptrdiff_t offset) {
        index_ = CheckedIndex(offset);
        return *this;
    }

    MsPtr& operator-=(std::ptrdiff_t offset) {
        if (offset == PTRDIFF_MIN) {
            throw std::out_of_range("MsPtr: offset is out of range");
        }
        return *this += -offset;
    }

    MsPtr& operator++() { return *this += 1; }
    MsPtr& operator--() { return *this -= 1; }

    MsPtr operator++(int) {
        MsPtr copy(*this);
        ++*this;
        return copy;
    }

    MsPtr operator--(int) {
        MsPtr copy(*this);
        --*this;
        return copy;
    }

    friend MsPtr operator+(MsPtr pointer, std::ptrdiff_t offset) { return pointer += offset; }
    friend MsPtr operator+(std::ptrdiff_t offset, MsPtr pointer) { return pointer += offset; }
    friend MsPtr operator-(MsPtr pointer, std::ptrdiff_t offset) { return pointer -= offset; }

    friend std::ptrdiff_t operator-(const MsPtr& lhs, const MsPtr& rhs) {
        RequireSameSpan(lhs, rhs);
        return lhs.index_ - rhs.index_;
    }

    friend bool operator==(const MsPtr& lhs, const MsPtr& rhs) noexcept {
        return lhs.storage_.Get() == rhs.storage_.Get() && lhs.index_ == rhs.index_;
    }

    friend std::strong_ordering operator<=>(const MsPtr& lhs, const MsPtr& rhs) {
        RequireSameSpan(lhs, rhs);
        return lhs.index_ <=> rhs.index_;
    }

    explicit operator bool() const noexcept { return static_cast<bool>(storage_); }

    std::size_t Index() const noexcept { return static_cast<std::size_t>(index_); }

    bool IsDereferenceable() const noexcept {
        return storage_ && static_cast<std::size_t>(index_) < storage_->Size();
    }

    ShrdPtr<T> Share() const { return Slot(); }

  private:
    template <class>
    friend class MemorySpan;

    MsPtr(ShrdPtr<detail::SpanStorage<T>> storage, std::ptrdiff_t index) noexcept
        : storage_(std::move(storage)), index_(index) {}

    static void RequireSameSpan(const MsPtr& lhs, const MsPtr& rhs) {
        if (lhs.storage_.Get() != rhs.storage_.Get()) {
            throw std::invalid_argument("MsPtr: pointers belong to different spans");
        }
    }

    ShrdPtr<T>& Slot() const {
        if (!storage_) {
            throw NullPointerError("MsPtr: dereference of an unbound pointer");
        }
        if (static_cast<std::size_t>(index_) >= storage_->Size()) {
            throw std::out_of_range("MsPtr: dereference outside of the span");
        }
        return (*storage_)[static_cast<std::size_t>(index_)];
    }

    std::ptrdiff_t CheckedIndex(std::ptrdiff_t offset) const {
        if (!storage_) {
            throw NullPointerError("MsPtr: arithmetic on an unbound pointer");
        }
        const auto size = static_cast<std::ptrdiff_t>(storage_->Size());
        if ((offset > 0 && offset > size - index_) || (offset < 0 && offset < -index_)) {
            throw std::out_of_range("MsPtr: pointer would leave the span");
        }
        return index_ + offset;
    }

    ShrdPtr<detail::SpanStorage<T>> storage_;
    std::ptrdiff_t index_ = 0;
};

}  // namespace smart
