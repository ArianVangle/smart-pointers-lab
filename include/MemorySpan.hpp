#pragma once

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <utility>

#include "MsPtr.hpp"
#include "ShrdPtr.hpp"
#include "SpanStorage.hpp"
#include "UnqPtr.hpp"

namespace smart {

template <class T>
class MemorySpan {
  public:
    MemorySpan() : storage_(MakeShrd<Storage>(0)) {}

    explicit MemorySpan(std::size_t size)
        requires std::default_initializable<T>
        : storage_(MakeShrd<Storage>(size)) {
        for (std::size_t i = 0; i < size; ++i) {
            (*storage_)[i] = MakeShrd<T>();
        }
    }

    MemorySpan(std::size_t size, const T& value)
        requires std::copy_constructible<T>
        : storage_(MakeShrd<Storage>(size)) {
        for (std::size_t i = 0; i < size; ++i) {
            (*storage_)[i] = MakeShrd<T>(value);
        }
    }

    MemorySpan(std::initializer_list<T> items)
        requires std::copy_constructible<T>
        : storage_(MakeShrd<Storage>(items.size())) {
        std::size_t i = 0;
        for (const T& item : items) {
            (*storage_)[i++] = MakeShrd<T>(item);
        }
    }

    MemorySpan(const MemorySpan& other) : storage_(MakeShrd<Storage>(other.Size())) {
        for (std::size_t i = 0; i < other.Size(); ++i) {
            (*storage_)[i] = ShrdPtr<T>((*other.storage_)[i].Clone());
        }
    }

    MemorySpan(MemorySpan&& other) noexcept : storage_(std::move(other.storage_)) {}

    MemorySpan& operator=(MemorySpan other) noexcept {
        Swap(other);
        return *this;
    }

    void Swap(MemorySpan& other) noexcept { storage_.Swap(other.storage_); }

    std::size_t Size() const noexcept { return storage_ ? storage_->Size() : 0; }
    bool IsEmpty() const noexcept { return Size() == 0; }

    ShrdPtr<T> Copy(std::size_t index) const { return CheckedSlot(index); }

    UnqPtr<T> Get(std::size_t index) const { return CheckedSlot(index).Clone(); }

    MsPtr<T> Locate(std::size_t index) {
        EnsureStorage();
        if (index > Size()) {
            throw std::out_of_range("MemorySpan::Locate: index is out of range");
        }
        return MsPtr<T>(storage_, static_cast<std::ptrdiff_t>(index));
    }

    MsPtr<T> Begin() { return Locate(0); }
    MsPtr<T> End() { return Locate(Size()); }

    MsPtr<T> begin() { return Begin(); }
    MsPtr<T> end() { return End(); }

    void Set(std::size_t index, const T& value)
        requires std::is_copy_assignable_v<T>
    {
        *CheckedSlot(index) = value;
    }

    template <class U>
        requires PointerCompatible<U, T>
    void Replace(std::size_t index, UnqPtr<U>&& value) {
        if (!value) {
            throw std::invalid_argument("MemorySpan::Replace: empty value");
        }
        CheckedSlot(index) = ShrdPtr<T>(std::move(value));
    }

    void Swap(std::size_t first, std::size_t second) { CheckedSlot(first).Swap(CheckedSlot(second)); }

    void Resize(std::size_t newSize)
        requires std::default_initializable<T>
    {
        EnsureStorage();
        const std::size_t old = Size();
        storage_->Resize(newSize);
        for (std::size_t i = old; i < newSize; ++i) {
            (*storage_)[i] = MakeShrd<T>();
        }
    }

  private:
    using Storage = detail::SpanStorage<T>;

    void EnsureStorage() {
        if (!storage_) {
            storage_ = MakeShrd<Storage>(0);
        }
    }

    ShrdPtr<T>& CheckedSlot(std::size_t index) const {
        if (index >= Size()) {
            throw std::out_of_range("MemorySpan: index is out of range");
        }
        return (*storage_)[index];
    }

    ShrdPtr<Storage> storage_;
};

}  // namespace smart
