#pragma once

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "Sequence.hpp"
#include "UnqPtr.hpp"

namespace sequence {

template <class T>
    requires std::copyable<T>
class ListSequence final : public Sequence<T> {
  public:
    ListSequence() = default;

    ListSequence(std::initializer_list<T> items) {
        for (const T& item : items) {
            Append(item);
        }
    }

    ListSequence(const ListSequence& other) {
        for (const Node* node = other.head_.Get(); node != nullptr; node = node->next.Get()) {
            Append(node->value);
        }
    }

    ListSequence(ListSequence&& other) noexcept { Swap(other); }

    ListSequence& operator=(ListSequence other) noexcept {
        Swap(other);
        return *this;
    }

    ~ListSequence() override { Clear(); }

    void Swap(ListSequence& other) noexcept {
        head_.Swap(other.head_);
        std::swap(tail_, other.tail_);
        std::swap(length_, other.length_);
    }

    void Clear() noexcept {
        while (head_) {
            smart::UnqPtr<Node> next = std::move(head_->next);
            head_ = std::move(next);
        }
        tail_ = nullptr;
        length_ = 0;
    }

    std::size_t GetLength() const noexcept override { return length_; }

    T Get(std::size_t index) const override {
        this->CheckIndex(index, length_);
        return NodeAt(index)->value;
    }

    void Set(std::size_t index, const T& value) override {
        this->CheckIndex(index, length_);
        NodeAt(index)->value = value;
    }

    void Append(const T& value) override {
        auto node = smart::MakeUnq<Node>(value);
        Node* raw = node.Get();
        if (tail_ == nullptr) {
            head_ = std::move(node);
        } else {
            tail_->next = std::move(node);
        }
        tail_ = raw;
        ++length_;
    }

    void Prepend(const T& value) override {
        auto node = smart::MakeUnq<Node>(value);
        node->next = std::move(head_);
        head_ = std::move(node);
        if (tail_ == nullptr) {
            tail_ = head_.Get();
        }
        ++length_;
    }

    void InsertAt(std::size_t index, const T& value) override {
        this->CheckInsertIndex(index, length_);
        if (index == 0) {
            Prepend(value);
            return;
        }
        if (index == length_) {
            Append(value);
            return;
        }
        Node* previous = NodeAt(index - 1);
        auto node = smart::MakeUnq<Node>(value);
        node->next = std::move(previous->next);
        previous->next = std::move(node);
        ++length_;
    }

    void RemoveAt(std::size_t index) override {
        this->CheckIndex(index, length_);
        if (index == 0) {
            smart::UnqPtr<Node> victim = std::move(head_);
            head_ = std::move(victim->next);
            if (!head_) {
                tail_ = nullptr;
            }
        } else {
            Node* previous = NodeAt(index - 1);
            smart::UnqPtr<Node> victim = std::move(previous->next);
            previous->next = std::move(victim->next);
            if (!previous->next) {
                tail_ = previous;
            }
        }
        --length_;
    }

    smart::UnqPtr<Sequence<T>> GetSubsequence(std::size_t start, std::size_t end) const override {
        this->CheckRange(start, end, length_);
        auto result = smart::MakeUnq<ListSequence<T>>();
        const Node* node = start < length_ ? NodeAt(start) : nullptr;
        for (std::size_t i = start; i < end; ++i, node = node->next.Get()) {
            result->Append(node->value);
        }
        return result;
    }

    smart::UnqPtr<Sequence<T>> Concat(const Sequence<T>& other) const override {
        auto result = smart::MakeUnq<ListSequence<T>>(*this);
        for (std::size_t i = 0; i < other.GetLength(); ++i) {
            result->Append(other.Get(i));
        }
        return result;
    }

  private:
    struct Node {
        explicit Node(const T& item) : value(item) {}
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        T value;
        smart::UnqPtr<Node> next;
    };

    Node* NodeAt(std::size_t index) noexcept {
        Node* node = head_.Get();
        for (std::size_t i = 0; i < index; ++i) {
            node = node->next.Get();
        }
        return node;
    }

    const Node* NodeAt(std::size_t index) const noexcept {
        const Node* node = head_.Get();
        for (std::size_t i = 0; i < index; ++i) {
            node = node->next.Get();
        }
        return node;
    }

    smart::UnqPtr<Node> head_;
    Node* tail_ = nullptr;
    std::size_t length_ = 0;
};

}  // namespace sequence
