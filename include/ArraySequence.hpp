#pragma once

#include <concepts>
#include <cstddef>
#include <initializer_list>

#include "MemorySpan.hpp"
#include "Sequence.hpp"

namespace sequence {

template <class T>
    requires std::default_initializable<T> && std::copyable<T>
class ArraySequence final : public Sequence<T> {
  public:
    ArraySequence() = default;

    ArraySequence(std::initializer_list<T> items) {
        Reserve(items.size());
        for (const T& item : items) {
            Append(item);
        }
    }

    std::size_t GetLength() const noexcept override { return length_; }
    std::size_t GetCapacity() const noexcept { return buffer_.Size(); }

    T Get(std::size_t index) const override {
        this->CheckIndex(index, length_);
        return *buffer_.Copy(index);
    }

    void Set(std::size_t index, const T& value) override {
        this->CheckIndex(index, length_);
        buffer_.Set(index, value);
    }

    void Append(const T& value) override {
        Reserve(length_ + 1);
        buffer_.Set(length_, value);
        ++length_;
    }

    void Prepend(const T& value) override { InsertAt(0, value); }

    void InsertAt(std::size_t index, const T& value) override {
        this->CheckInsertIndex(index, length_);
        Append(value);
        for (std::size_t i = length_ - 1; i > index; --i) {
            buffer_.Swap(i, i - 1);
        }
    }

    void RemoveAt(std::size_t index) override {
        this->CheckIndex(index, length_);
        for (std::size_t i = index; i + 1 < length_; ++i) {
            buffer_.Swap(i, i + 1);
        }
        --length_;
    }

    smart::UnqPtr<Sequence<T>> GetSubsequence(std::size_t start, std::size_t end) const override {
        this->CheckRange(start, end, length_);
        auto result = smart::MakeUnq<ArraySequence<T>>();
        result->Reserve(end - start);
        for (std::size_t i = start; i < end; ++i) {
            result->Append(Get(i));
        }
        return result;
    }

    smart::UnqPtr<Sequence<T>> Concat(const Sequence<T>& other) const override {
        auto result = smart::MakeUnq<ArraySequence<T>>(*this);
        result->Reserve(length_ + other.GetLength());
        for (std::size_t i = 0; i < other.GetLength(); ++i) {
            result->Append(other.Get(i));
        }
        return result;
    }

    void Reserve(std::size_t capacity) {
        if (capacity <= buffer_.Size()) {
            return;
        }
        std::size_t grown = buffer_.Size() * 2;
        if (grown < kMinCapacity) {
            grown = kMinCapacity;
        }
        buffer_.Resize(grown > capacity ? grown : capacity);
    }

  private:
    static constexpr std::size_t kMinCapacity = 4;

    smart::MemorySpan<T> buffer_;
    std::size_t length_ = 0;
};

}  // namespace sequence
