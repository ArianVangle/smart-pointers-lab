#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "UnqPtr.hpp"

namespace smart {

template <class T>
class ShrdPtr {
  public:
    ShrdPtr() noexcept = default;
    ShrdPtr(std::nullptr_t) noexcept {}

    template <class U>
        requires PointerCompatible<U, T>
    ShrdPtr(UnqPtr<U>&& owner) {
        Adopt(owner);
    }

    template <class U>
        requires PointerCompatible<U, T> && (!std::is_void_v<U>)
    explicit ShrdPtr(U* raw) {
        UnqPtr<U> owner(raw);
        Adopt(owner);
    }

    ShrdPtr(const ShrdPtr& other) noexcept { ShareFrom(other); }

    template <class U>
        requires PointerCompatible<U, T>
    ShrdPtr(const ShrdPtr<U>& other) noexcept {
        ShareFrom(other);
    }

    ShrdPtr(ShrdPtr&& other) noexcept { StealFrom(other); }

    template <class U>
        requires PointerCompatible<U, T>
    ShrdPtr(ShrdPtr<U>&& other) noexcept {
        StealFrom(other);
    }

    template <class U>
    ShrdPtr(const ShrdPtr<U>& owner, T* view) noexcept {
        if (owner.master_ == nullptr || view == nullptr) {
            return;
        }
        master_ = owner.master_;
        count_ = owner.count_;
        ptr_ = view;
        ++*count_;
    }

    ~ShrdPtr() { Release(); }

    ShrdPtr& operator=(ShrdPtr other) noexcept {
        Swap(other);
        return *this;
    }

    void Reset() noexcept { Release(); }

    void Swap(ShrdPtr& other) noexcept {
        std::swap(master_, other.master_);
        std::swap(count_, other.count_);
        std::swap(ptr_, other.ptr_);
    }

    T* Get() const noexcept { return ptr_; }

    std::add_lvalue_reference_t<T> operator*() const { return *Checked(); }
    T* operator->() const { return Checked(); }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

    template <class U>
    bool operator==(const ShrdPtr<U>& other) const noexcept {
        return Get() == other.Get();
    }

    std::size_t UseCount() const noexcept { return count_ != nullptr ? *count_ : 0; }

    UnqPtr<T> Clone() const {
        if (master_ == nullptr) {
            return UnqPtr<T>();
        }
        UnqPtr<void> copy(*master_);
        const std::ptrdiff_t offset = detail::AddressOffset(ptr_, master_->ptr_);
        UnqPtr<T> result(static_cast<T*>(detail::ShiftAddress(copy.ptr_, offset)), copy.object_,
                         copy.destroy_, copy.clone_);
        copy.Forget();
        return result;
    }

  private:
    template <class>
    friend class ShrdPtr;

    T* Checked() const {
        if (ptr_ == nullptr) {
            throw NullPointerError("ShrdPtr: dereference of an empty pointer");
        }
        return ptr_;
    }

    template <class U>
    void Adopt(UnqPtr<U>& owner) {
        if (owner.ptr_ == nullptr) {
            return;
        }
        std::size_t* count = new std::size_t(1);
        UnqPtr<void>* master = nullptr;
        try {
            master = new UnqPtr<void>();
        } catch (...) {
            delete count;
            throw;
        }
        master->ptr_ = owner.object_;
        master->object_ = owner.object_;
        master->destroy_ = owner.destroy_;
        master->clone_ = owner.clone_;

        ptr_ = owner.ptr_;
        master_ = master;
        count_ = count;
        owner.Forget();
    }

    template <class U>
    void ShareFrom(const ShrdPtr<U>& other) noexcept {
        if (other.master_ == nullptr) {
            return;
        }
        master_ = other.master_;
        count_ = other.count_;
        ptr_ = other.ptr_;
        ++*count_;
    }

    template <class U>
    void StealFrom(ShrdPtr<U>& other) noexcept {
        if (other.master_ == nullptr) {
            return;
        }
        master_ = other.master_;
        count_ = other.count_;
        ptr_ = other.ptr_;
        other.master_ = nullptr;
        other.count_ = nullptr;
        other.ptr_ = nullptr;
    }

    void Release() noexcept {
        if (count_ == nullptr) {
            return;
        }
        std::size_t* count = count_;
        UnqPtr<void>* master = master_;
        master_ = nullptr;
        count_ = nullptr;
        ptr_ = nullptr;
        if (--*count == 0) {
            delete count;
            delete master;
        }
    }

    UnqPtr<void>* master_ = nullptr;
    std::size_t* count_ = nullptr;
    T* ptr_ = nullptr;
};

template <class T, class... Args>
    requires(!std::is_array_v<T> && !std::is_void_v<T>)
ShrdPtr<T> MakeShrd(Args&&... args) {
    return ShrdPtr<T>(MakeUnq<T>(std::forward<Args>(args)...));
}

template <class T, class U>
ShrdPtr<T> StaticPointerCast(const ShrdPtr<U>& from) noexcept {
    return ShrdPtr<T>(from, static_cast<T*>(from.Get()));
}

template <class T, class U>
ShrdPtr<T> DynamicPointerCast(const ShrdPtr<U>& from) noexcept {
    return ShrdPtr<T>(from, dynamic_cast<T*>(from.Get()));
}

}  // namespace smart
