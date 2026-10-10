#pragma once

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace smart {

class NullPointerError : public std::logic_error {
  public:
    using std::logic_error::logic_error;
};

template <class From, class To>
concept PointerCompatible = std::is_convertible_v<From*, To*>;

namespace detail {

using DestroyFn = void (*)(void*) noexcept;
using CloneFn = void* (*)(const void*);

template <class U>
void DestroyAs(void* object) noexcept {
    delete static_cast<U*>(object);
}

template <class U>
void* CloneAs(const void* object) {
    if constexpr (std::is_copy_constructible_v<U>) {
        return new U(*static_cast<const U*>(object));
    } else {
        throw std::logic_error("UnqPtr: object type is not copy-constructible");
    }
}

inline std::ptrdiff_t AddressOffset(const void* from, const void* base) noexcept {
    return static_cast<const char*>(from) - static_cast<const char*>(base);
}

inline void* ShiftAddress(void* base, std::ptrdiff_t offset) noexcept {
    return static_cast<char*>(base) + offset;
}

}  // namespace detail

template <class T>
class ShrdPtr;

template <class T>
class UnqPtr {
  public:
    UnqPtr() noexcept = default;
    UnqPtr(std::nullptr_t) noexcept {}

    template <class U>
        requires PointerCompatible<U, T> && (!std::is_void_v<U>)
    explicit UnqPtr(U* raw) noexcept
        : ptr_(raw),
          object_(const_cast<std::remove_cv_t<U>*>(raw)),
          destroy_(raw != nullptr ? &detail::DestroyAs<U> : nullptr),
          clone_(raw != nullptr ? &detail::CloneAs<U> : nullptr) {}

    UnqPtr(const UnqPtr& other) { CopyFrom(other); }

    template <class U>
        requires PointerCompatible<U, T>
    UnqPtr(const UnqPtr<U>& other) {
        CopyFrom(other);
    }

    UnqPtr(UnqPtr&& other) noexcept { StealFrom(other); }

    template <class U>
        requires PointerCompatible<U, T>
    UnqPtr(UnqPtr<U>&& other) noexcept {
        StealFrom(other);
    }

    ~UnqPtr() { Destroy(); }

    UnqPtr& operator=(UnqPtr other) noexcept {
        Swap(other);
        return *this;
    }

    void Reset() noexcept {
        UnqPtr empty;
        Swap(empty);
    }

    void Swap(UnqPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(object_, other.object_);
        std::swap(destroy_, other.destroy_);
        std::swap(clone_, other.clone_);
    }

    T* Get() noexcept { return ptr_; }
    const T* Get() const noexcept { return ptr_; }

    std::add_lvalue_reference_t<T> operator*() { return *Checked(); }
    std::add_lvalue_reference_t<const T> operator*() const { return *Checked(); }
    T* operator->() { return Checked(); }
    const T* operator->() const { return Checked(); }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

  private:
    template <class>
    friend class UnqPtr;
    template <class>
    friend class ShrdPtr;

    UnqPtr(T* ptr, void* object, detail::DestroyFn destroy, detail::CloneFn clone) noexcept
        : ptr_(ptr), object_(object), destroy_(destroy), clone_(clone) {}

    T* Checked() const {
        if (ptr_ == nullptr) {
            throw NullPointerError("UnqPtr: dereference of an empty pointer");
        }
        return ptr_;
    }

    template <class U>
    void CopyFrom(const UnqPtr<U>& other) {
        if (other.ptr_ == nullptr) {
            return;
        }
        const std::ptrdiff_t offset = detail::AddressOffset(other.ptr_, other.object_);
        void* object = other.clone_(other.object_);
        ptr_ = static_cast<U*>(detail::ShiftAddress(object, offset));
        object_ = object;
        destroy_ = other.destroy_;
        clone_ = other.clone_;
    }

    template <class U>
    void StealFrom(UnqPtr<U>& other) noexcept {
        ptr_ = other.ptr_;
        object_ = other.object_;
        destroy_ = other.destroy_;
        clone_ = other.clone_;
        other.Forget();
    }

    void Forget() noexcept {
        ptr_ = nullptr;
        object_ = nullptr;
        destroy_ = nullptr;
        clone_ = nullptr;
    }

    void Destroy() noexcept {
        if (ptr_ == nullptr) {
            return;
        }
        void* object = object_;
        const detail::DestroyFn destroy = destroy_;
        Forget();
        destroy(object);
    }

    T* ptr_ = nullptr;
    void* object_ = nullptr;
    detail::DestroyFn destroy_ = nullptr;
    detail::CloneFn clone_ = nullptr;
};

template <class T, class... Args>
    requires(!std::is_array_v<T> && !std::is_void_v<T>)
UnqPtr<T> MakeUnq(Args&&... args) {
    return UnqPtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace smart
