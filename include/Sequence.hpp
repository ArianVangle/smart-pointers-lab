#pragma once

#include <cstddef>
#include <stdexcept>

#include "UnqPtr.hpp"

namespace sequence {

template <class T>
class Sequence {
  public:
    virtual ~Sequence() = default;

    virtual std::size_t GetLength() const noexcept = 0;
    bool IsEmpty() const noexcept { return GetLength() == 0; }

    virtual T Get(std::size_t index) const = 0;

    T GetFirst() const {
        RequireNonEmpty();
        return Get(0);
    }

    T GetLast() const {
        RequireNonEmpty();
        return Get(GetLength() - 1);
    }

    virtual void Set(std::size_t index, const T& value) = 0;
    virtual void Append(const T& value) = 0;
    virtual void Prepend(const T& value) = 0;
    virtual void InsertAt(std::size_t index, const T& value) = 0;
    virtual void RemoveAt(std::size_t index) = 0;

    virtual smart::UnqPtr<Sequence<T>> GetSubsequence(std::size_t start, std::size_t end) const = 0;
    virtual smart::UnqPtr<Sequence<T>> Concat(const Sequence<T>& other) const = 0;

  protected:
    static void CheckIndex(std::size_t index, std::size_t length) {
        if (index >= length) {
            throw std::out_of_range("Sequence: index is out of range");
        }
    }

    static void CheckInsertIndex(std::size_t index, std::size_t length) {
        if (index > length) {
            throw std::out_of_range("Sequence: insert position is out of range");
        }
    }

    static void CheckRange(std::size_t start, std::size_t end, std::size_t length) {
        if (start > end || end > length) {
            throw std::out_of_range("Sequence: invalid subsequence bounds");
        }
    }

  private:
    void RequireNonEmpty() const {
        if (IsEmpty()) {
            throw std::out_of_range("Sequence: sequence is empty");
        }
    }
};

}  // namespace sequence
