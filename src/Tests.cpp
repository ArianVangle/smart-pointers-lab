#include "Tests.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "AllocCounter.hpp"
#include "ArraySequence.hpp"
#include "ListSequence.hpp"
#include "MemorySpan.hpp"
#include "Output.hpp"

using namespace smart;
using sequence::ArraySequence;
using sequence::ListSequence;
using sequence::Sequence;

namespace {

struct Tracked {
    static long alive;
    static long constructed;

    explicit Tracked(int v = 0) : value(v) {
        ++alive;
        ++constructed;
    }
    Tracked(const Tracked& other) : value(other.value) {
        ++alive;
        ++constructed;
    }
    Tracked& operator=(const Tracked&) = default;
    ~Tracked() { --alive; }

    bool operator==(const Tracked& other) const { return value == other.value; }

    int value;
};

long Tracked::alive = 0;
long Tracked::constructed = 0;

struct Base {
    explicit Base(int v = 1) : tracked(v) {}
    virtual ~Base() = default;
    virtual int Id() const { return 1; }
    Tracked tracked;
};

struct Derived : Base {
    explicit Derived(int v = 2) : Base(v) {}
    int Id() const override { return 2; }
    int extra = 42;
};

struct AbstractShape {
    virtual ~AbstractShape() = default;
    virtual int Area() const = 0;
};

struct Square : AbstractShape {
    explicit Square(int s) : side(s) {}
    int Area() const override { return side * side; }
    int side;
    Tracked tracked;
};

struct PlainBase {
    Tracked tracked;
};

struct PlainDerived : PlainBase {
    static int destroyed;
    ~PlainDerived() { ++destroyed; }
    Tracked more;
};

int PlainDerived::destroyed = 0;

struct Left {
    virtual ~Left() = default;
    int left = 1;
};

struct Right {
    virtual ~Right() = default;
    int right = 2;
};

struct Both : Left, Right {
    int both = 3;
    Tracked tracked;
};

struct Unrelated {
    int value = 0;
};

struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    Tracked tracked;
};

struct ThrowingOnConstruction {
    ThrowingOnConstruction() : tracked(1) { throw std::runtime_error("construction failed"); }
    Tracked tracked;
};

struct Holder {
    ShrdPtr<Holder> next;
    Tracked tracked;
};

struct CheckFailed {
    std::string message;
};

[[noreturn]] void Fail(const char* file, int line, const std::string& text) {
    throw CheckFailed{std::string(file) + ":" + std::to_string(line) + ": " + text};
}

#define CHECK(expr)                                       \
    do {                                                  \
        if (!(expr)) {                                    \
            Fail(__FILE__, __LINE__, "CHECK(" #expr ")"); \
        }                                                 \
    } while (false)

#define CHECK_EQ(actual, expected)                                                       \
    do {                                                                                 \
        const auto lab_actual = (actual);                                                \
        const auto lab_expected = (expected);                                            \
        if (!(lab_actual == lab_expected)) {                                             \
            Fail(__FILE__, __LINE__,                                                     \
                 "CHECK_EQ(" #actual ", " #expected "): " + std::to_string(lab_actual) + \
                     " != " + std::to_string(lab_expected));                             \
        }                                                                                \
    } while (false)

#define CHECK_THROWS(statement, ExceptionType)                                         \
    do {                                                                               \
        bool lab_thrown = false;                                                       \
        try {                                                                          \
            statement;                                                                 \
        } catch (const ExceptionType&) {                                               \
            lab_thrown = true;                                                         \
        } catch (...) {                                                                \
        }                                                                              \
        if (!lab_thrown) {                                                             \
            Fail(__FILE__, __LINE__, "expected " #ExceptionType " from: " #statement); \
        }                                                                              \
    } while (false)

int g_passed = 0;
int g_failed = 0;

void RunTest(const char* name, void (*body)(), const char* filter) {
    if (filter != nullptr && *filter != '\0' && std::string(name).find(filter) == std::string::npos) {
        return;
    }

    const long aliveBefore = Tracked::alive;
    const std::size_t bytesBefore = alloc_stats::Current().liveBytes;

    std::string error;
    try {
        body();
    } catch (const CheckFailed& failure) {
        error = failure.message;
    } catch (const std::exception& problem) {
        error = std::string("unexpected exception: ") + problem.what();
    } catch (...) {
        error = "unexpected unknown exception";
    }

    if (error.empty() && Tracked::alive != aliveBefore) {
        error =
            "leak: Tracked alive " + std::to_string(aliveBefore) + " -> " + std::to_string(Tracked::alive);
    }
    if (error.empty() && alloc_stats::Current().liveBytes != bytesBefore) {
        error = "leak: heap bytes " + std::to_string(bytesBefore) + " -> " +
                std::to_string(alloc_stats::Current().liveBytes);
    }

    if (error.empty()) {
        ++g_passed;
        output::Printf("[  OK  ] %s\n", name);
    } else {
        ++g_failed;
        output::Printf("[ FAIL ] %s\n         %s\n", name, error.c_str());
    }
}

static_assert(std::is_convertible_v<UnqPtr<Derived>, UnqPtr<Base>>);
static_assert(std::is_convertible_v<UnqPtr<Base>, UnqPtr<const Base>>);
static_assert(!std::is_convertible_v<UnqPtr<Base>, UnqPtr<Derived>>);
static_assert(!std::is_constructible_v<UnqPtr<Unrelated>, UnqPtr<Base>>);
static_assert(!std::is_convertible_v<Base*, UnqPtr<Base>>);

static_assert(std::is_convertible_v<ShrdPtr<Derived>, ShrdPtr<Base>>);
static_assert(std::is_convertible_v<ShrdPtr<Both>, ShrdPtr<Right>>);
static_assert(std::is_convertible_v<UnqPtr<Derived>&&, ShrdPtr<Base>>);
static_assert(!std::is_convertible_v<ShrdPtr<Base>, ShrdPtr<Derived>>);
static_assert(!std::is_constructible_v<ShrdPtr<Unrelated>, ShrdPtr<Base>>);
static_assert(!std::is_assignable_v<ShrdPtr<Left>&, ShrdPtr<Right>>);

static_assert(!std::is_convertible_v<MsPtr<Derived>, MsPtr<Base>>);

// ---------------------------------------------------------------------------
// UnqPtr
// ---------------------------------------------------------------------------

void UnqPtrDefaultIsEmpty() {
    UnqPtr<Tracked> pointer;
    CHECK(!pointer);
    CHECK(pointer == nullptr);
    CHECK(pointer.Get() == nullptr);
    CHECK_THROWS((void)*pointer, NullPointerError);
    CHECK_THROWS((void)pointer->value, NullPointerError);
}

void UnqPtrOwnsAndDestroys() {
    const long before = Tracked::alive;
    {
        auto pointer = MakeUnq<Tracked>(7);
        CHECK(pointer);
        CHECK_EQ(pointer->value, 7);
        CHECK_EQ((*pointer).value, 7);
        CHECK_EQ(Tracked::alive, before + 1);
    }
    CHECK_EQ(Tracked::alive, before);
}

void UnqPtrCopyIsDeep() {
    auto original = MakeUnq<Tracked>(5);
    UnqPtr<Tracked> copy = original;
    CHECK(copy.Get() != original.Get());
    CHECK_EQ(copy->value, 5);
    copy->value = 10;
    CHECK_EQ(original->value, 5);

    UnqPtr<Tracked> empty;
    UnqPtr<Tracked> copyOfEmpty = empty;
    CHECK(!copyOfEmpty);
}

void UnqPtrCopyKeepsDynamicType() {
    UnqPtr<Base> base = MakeUnq<Derived>(9);
    UnqPtr<Base> copy = base;
    CHECK_EQ(copy->Id(), 2);
    CHECK(dynamic_cast<Derived*>(copy.Get()) != nullptr);
    CHECK_EQ(copy->tracked.value, 9);
}

void UnqPtrCopyOfAbstractBase() {
    UnqPtr<AbstractShape> shape = MakeUnq<Square>(3);
    UnqPtr<AbstractShape> copy = shape;
    CHECK_EQ(copy->Area(), 9);
    CHECK(copy.Get() != shape.Get());
}

void UnqPtrMoveTransfersOwnership() {
    auto source = MakeUnq<Tracked>(1);
    Tracked* raw = source.Get();
    UnqPtr<Tracked> target = std::move(source);
    CHECK(!source);
    CHECK(target.Get() == raw);

    UnqPtr<Tracked> other = MakeUnq<Tracked>(2);
    other = std::move(target);
    CHECK(other.Get() == raw);
    CHECK(!target);
}

void UnqPtrNonVirtualDestructor() {
    const int before = PlainDerived::destroyed;
    {
        UnqPtr<PlainBase> base = MakeUnq<PlainDerived>();
    }
    CHECK_EQ(PlainDerived::destroyed, before + 1);
}

void UnqPtrMultipleInheritance() {
    UnqPtr<Right> right = MakeUnq<Both>();
    CHECK_EQ(right->right, 2);
    UnqPtr<Right> copy = right;
    CHECK_EQ(copy->right, 2);
    CHECK_EQ(dynamic_cast<Both&>(*copy).both, 3);
}

void UnqPtrNonCopyableThrows() {
    auto pointer = MakeUnq<NonCopyable>();
    CHECK_THROWS(UnqPtr<NonCopyable> copy(pointer), std::logic_error);
    UnqPtr<NonCopyable> moved = std::move(pointer);
    CHECK(moved);
}

void UnqPtrExceptionInConstructor() { CHECK_THROWS(MakeUnq<ThrowingOnConstruction>(), std::runtime_error); }

void UnqPtrResetAndSwap() {
    auto a = MakeUnq<Tracked>(1);
    auto b = MakeUnq<Tracked>(2);
    a.Swap(b);
    CHECK_EQ(a->value, 2);
    CHECK_EQ(b->value, 1);
    a.Reset();
    CHECK(!a);
    b = nullptr;
    CHECK(!b);
}

void UnqPtrSelfAssignment() {
    auto pointer = MakeUnq<Tracked>(4);
    auto& alias = pointer;
    pointer = alias;
    CHECK_EQ(pointer->value, 4);
    pointer = std::move(alias);
    CHECK(pointer);
    CHECK_EQ(pointer->value, 4);
}

void UnqPtrDeepConstness() {
    const auto pointer = MakeUnq<Tracked>(1);
    static_assert(std::is_same_v<decltype(pointer.Get()), const Tracked*>);
    static_assert(std::is_same_v<decltype(*pointer), const Tracked&>);
    CHECK_EQ(pointer->value, 1);
}

void UnqPtrManyObjectsNoLeaks() {
    const long before = Tracked::alive;
    UnqPtr<Tracked> keep;
    for (int i = 0; i < 10000; ++i) {
        keep = MakeUnq<Tracked>(i);
    }
    CHECK_EQ(Tracked::alive, before + 1);
    CHECK_EQ(keep->value, 9999);
}

// ---------------------------------------------------------------------------
// ShrdPtr
// ---------------------------------------------------------------------------

void ShrdPtrDefaultIsEmpty() {
    ShrdPtr<Tracked> pointer;
    CHECK(!pointer);
    CHECK_EQ(pointer.UseCount(), 0u);
    CHECK_THROWS((void)*pointer, NullPointerError);
}

void ShrdPtrCountsReferences() {
    auto first = MakeShrd<Tracked>(1);
    CHECK_EQ(first.UseCount(), 1u);
    {
        ShrdPtr<Tracked> second = first;
        ShrdPtr<Tracked> third = second;
        CHECK_EQ(first.UseCount(), 3u);
        CHECK(first == third);
        third->value = 5;
    }
    CHECK_EQ(first.UseCount(), 1u);
    CHECK_EQ(first->value, 5);
}

void ShrdPtrTakesObjectFromUnqPtr() {
    auto unique = MakeUnq<Tracked>(3);
    Tracked* raw = unique.Get();
    ShrdPtr<Tracked> shared = std::move(unique);
    CHECK(!unique);
    CHECK(shared.Get() == raw);
    CHECK_EQ(shared.UseCount(), 1u);
}

void ShrdPtrMoveDoesNotChangeCount() {
    auto a = MakeShrd<Tracked>(1);
    auto b = a;
    ShrdPtr<Tracked> c = std::move(b);
    CHECK(!b);
    CHECK_EQ(a.UseCount(), 2u);
    c = std::move(a);
    CHECK_EQ(c.UseCount(), 1u);
}

void ShrdPtrSubtypingSharesCounter() {
    auto derived = MakeShrd<Derived>(4);
    ShrdPtr<Base> base = derived;
    CHECK_EQ(derived.UseCount(), 2u);
    CHECK_EQ(base->Id(), 2);
    CHECK(static_cast<Base*>(derived.Get()) == base.Get());
}

void ShrdPtrMultipleInheritance() {
    auto both = MakeShrd<Both>();
    ShrdPtr<Left> left = both;
    ShrdPtr<Right> right = both;
    CHECK_EQ(left->left, 1);
    CHECK_EQ(right->right, 2);
    CHECK(static_cast<Right*>(both.Get()) == right.Get());
    CHECK_EQ(both.UseCount(), 3u);

    ShrdPtr<Both> back = DynamicPointerCast<Both>(right);
    CHECK(back.Get() == both.Get());
    ShrdPtr<Left> cross = DynamicPointerCast<Left>(right);
    CHECK(cross.Get() == left.Get());
    CHECK_EQ(both.UseCount(), 5u);
}

void ShrdPtrCasts() {
    ShrdPtr<Base> base = MakeShrd<Derived>(1);
    ShrdPtr<Derived> derived = StaticPointerCast<Derived>(base);
    CHECK_EQ(derived->extra, 42);
    CHECK_EQ(base.UseCount(), 2u);

    ShrdPtr<Base> plain = MakeShrd<Base>();
    ShrdPtr<Derived> failed = DynamicPointerCast<Derived>(plain);
    CHECK(!failed);
    CHECK_EQ(plain.UseCount(), 1u);
}

void ShrdPtrCloneIsIndependent() {
    ShrdPtr<Right> right = MakeShrd<Both>();
    UnqPtr<Right> clone = right.Clone();
    CHECK(clone.Get() != right.Get());
    CHECK_EQ(clone->right, 2);
    CHECK_EQ(dynamic_cast<Both&>(*clone).both, 3);
    CHECK_EQ(right.UseCount(), 1u);
}

void ShrdPtrNonVirtualDestructor() {
    const int before = PlainDerived::destroyed;
    {
        ShrdPtr<PlainBase> base(new PlainDerived());
        ShrdPtr<PlainBase> copy = base;
    }
    CHECK_EQ(PlainDerived::destroyed, before + 1);
}

void ShrdPtrSelfAssignment() {
    auto pointer = MakeShrd<Tracked>(1);
    auto& alias = pointer;
    pointer = alias;
    CHECK_EQ(pointer.UseCount(), 1u);
    pointer = std::move(alias);
    CHECK_EQ(pointer->value, 1);
}

void ShrdPtrExceptionInConstructor() { CHECK_THROWS(MakeShrd<ThrowingOnConstruction>(), std::runtime_error); }

void ShrdPtrCascadeRelease() {
    auto head = MakeShrd<Holder>();
    ShrdPtr<Holder> current = head;
    for (int i = 0; i < 1000; ++i) {
        current->next = MakeShrd<Holder>();
        current = current->next;
    }
    current.Reset();
    head.Reset();
}

void ShrdPtrCycleMustBeBrokenManually() {
    auto a = MakeShrd<Holder>();
    auto b = MakeShrd<Holder>();
    a->next = b;
    b->next = a;
    CHECK_EQ(a.UseCount(), 2u);
    b->next.Reset();
}

// ---------------------------------------------------------------------------
// MemorySpan
// ---------------------------------------------------------------------------

void SpanConstruction() {
    MemorySpan<int> empty;
    CHECK_EQ(empty.Size(), 0u);
    CHECK(empty.IsEmpty());

    MemorySpan<int> zeros(5);
    CHECK_EQ(zeros.Size(), 5u);
    CHECK_EQ(*zeros.Get(4), 0);

    MemorySpan<int> filled(3, 7);
    CHECK_EQ(*filled.Get(0), 7);

    MemorySpan<int> listed{1, 2, 3};
    CHECK_EQ(listed.Size(), 3u);
    CHECK_EQ(*listed.Get(1), 2);
}

void SpanGetReturnsIndependentCopy() {
    MemorySpan<Tracked> span(3, Tracked(1));
    UnqPtr<Tracked> copy = span.Get(0);
    copy->value = 100;
    CHECK_EQ(span.Get(0)->value, 1);
}

void SpanCopyReturnsSharedReference() {
    MemorySpan<Tracked> span(3, Tracked(1));
    ShrdPtr<Tracked> shared = span.Copy(1);
    shared->value = 55;
    CHECK_EQ(span.Get(1)->value, 55);
    CHECK_EQ(span.Locate(1)->value, 55);
    CHECK_EQ(shared.UseCount(), 2u);
}

void SpanSharedElementOutlivesSpan() {
    const long before = Tracked::alive;
    ShrdPtr<Tracked> survivor;
    {
        MemorySpan<Tracked> span(10, Tracked(3));
        survivor = span.Copy(5);
    }
    CHECK_EQ(Tracked::alive, before + 1);
    CHECK_EQ(survivor->value, 3);
}

void SpanSetIsVisibleEverywhere() {
    MemorySpan<int> span(4);
    ShrdPtr<int> shared = span.Copy(2);
    MsPtr<int> located = span.Locate(2);
    span.Set(2, 9);
    CHECK_EQ(*shared, 9);
    CHECK_EQ(*located, 9);
}

void SpanReplaceKeepsOldObject() {
    MemorySpan<Tracked> span(2, Tracked(1));
    ShrdPtr<Tracked> old = span.Copy(0);
    span.Replace(0, MakeUnq<Tracked>(2));
    CHECK_EQ(old->value, 1);
    CHECK_EQ(span.Get(0)->value, 2);
    CHECK_THROWS(span.Replace(0, UnqPtr<Tracked>()), std::invalid_argument);
}

void SpanIndexChecks() {
    MemorySpan<int> span(3);
    CHECK_THROWS(span.Get(3), std::out_of_range);
    CHECK_THROWS(span.Copy(3), std::out_of_range);
    CHECK_THROWS(span.Set(100, 1), std::out_of_range);
    CHECK_THROWS(span.Swap(0, 3), std::out_of_range);
    CHECK_THROWS(span.Locate(4), std::out_of_range);
    CHECK_THROWS(span.Get(static_cast<std::size_t>(-1)), std::out_of_range);

    MsPtr<int> end = span.Locate(3);
    CHECK(!end.IsDereferenceable());

    MemorySpan<int> empty;
    CHECK_THROWS(empty.Get(0), std::out_of_range);
    CHECK(empty.Begin() == empty.End());
}

void SpanDeepCopyAndMove() {
    MemorySpan<int> original{1, 2, 3};
    MemorySpan<int> copy = original;
    copy.Set(0, 10);
    CHECK_EQ(*original.Get(0), 1);

    MemorySpan<int> moved = std::move(copy);
    CHECK_EQ(moved.Size(), 3u);
    CHECK_EQ(*moved.Get(0), 10);
    CHECK_EQ(copy.Size(), 0u);
    CHECK_THROWS(copy.Get(0), std::out_of_range);

    copy = original;
    CHECK_EQ(*copy.Get(2), 3);
}

void SpanCopyPreservesDynamicTypes() {
    MemorySpan<Base> span(2);
    span.Replace(1, MakeUnq<Derived>(5));
    MemorySpan<Base> copy = span;
    CHECK_EQ(copy.Locate(0)->Id(), 1);
    CHECK_EQ(copy.Locate(1)->Id(), 2);
    CHECK(span.Copy(1).Get() != copy.Copy(1).Get());
}

void SpanSwapDoesNotCopyObjects() {
    MemorySpan<Tracked> span(2, Tracked(0));
    span.Set(0, Tracked(1));
    span.Set(1, Tracked(2));
    const long constructed = Tracked::constructed;
    ShrdPtr<Tracked> first = span.Copy(0);
    span.Swap(0, 1);
    CHECK_EQ(Tracked::constructed, constructed);
    CHECK_EQ(span.Locate(1)->value, 1);
    CHECK(span.Copy(1).Get() == first.Get());
}

void SpanResize() {
    MemorySpan<int> span{1, 2, 3};
    span.Resize(5);
    CHECK_EQ(span.Size(), 5u);
    CHECK_EQ(*span.Get(2), 3);
    CHECK_EQ(*span.Get(4), 0);
    span.Resize(2);
    CHECK_EQ(span.Size(), 2u);
    CHECK_THROWS(span.Get(2), std::out_of_range);
    span.Resize(0);
    CHECK(span.IsEmpty());
}

void SpanLargeNoLeaks() {
    const long before = Tracked::alive;
    {
        MemorySpan<Tracked> span(100000, Tracked(1));
        long sum = 0;
        for (const Tracked& item : span) {
            sum += item.value;
        }
        CHECK_EQ(sum, 100000);
    }
    CHECK_EQ(Tracked::alive, before);
}

// ---------------------------------------------------------------------------
// MsPtr
// ---------------------------------------------------------------------------

MemorySpan<int> MakeRange(int count) {
    MemorySpan<int> span(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        span.Set(static_cast<std::size_t>(i), i * 10);
    }
    return span;
}

void MsPtrUnbound() {
    MsPtr<int> pointer;
    CHECK(!pointer);
    CHECK(!pointer.IsDereferenceable());
    CHECK_THROWS((void)*pointer, NullPointerError);
    CHECK_THROWS(++pointer, NullPointerError);
}

void MsPtrIncrementAndIndexing() {
    MemorySpan<int> span = MakeRange(5);
    MsPtr<int> pointer = span.Begin();
    CHECK_EQ(*pointer, 0);
    ++pointer;
    CHECK_EQ(*pointer, 10);
    MsPtr<int> old = pointer++;
    CHECK_EQ(*old, 10);
    CHECK_EQ(*pointer, 20);
    --pointer;
    CHECK_EQ(*pointer, 10);
    CHECK_EQ(pointer[2], 30);
    CHECK_EQ(pointer[-1], 0);
    pointer[3] = 99;
    CHECK_EQ(*span.Get(4), 99);
}

void MsPtrArithmetic() {
    MemorySpan<int> span = MakeRange(10);
    MsPtr<int> begin = span.Begin();
    MsPtr<int> end = span.End();
    CHECK_EQ(end - begin, 10);
    MsPtr<int> middle = begin + 4;
    CHECK_EQ(*middle, 40);
    CHECK_EQ(*(2 + middle), 60);
    CHECK_EQ(*(middle - 3), 10);
    middle += 5;
    CHECK_EQ(*middle, 90);
    middle -= 9;
    CHECK(middle == begin);
    CHECK(begin < end);
    CHECK(end >= begin);
    CHECK(begin != end);
    CHECK_EQ(middle.Index(), 0u);
}

void MsPtrCannotLeaveSpan() {
    MemorySpan<int> span = MakeRange(3);
    MsPtr<int> pointer = span.Begin();
    CHECK_THROWS(--pointer, std::out_of_range);
    CHECK_EQ(pointer.Index(), 0u);
    CHECK_THROWS(pointer += 4, std::out_of_range);
    CHECK_THROWS(pointer - 1, std::out_of_range);
    CHECK_THROWS(pointer[3], std::out_of_range);
    CHECK_THROWS(pointer[-1], std::out_of_range);
    CHECK_THROWS(pointer += PTRDIFF_MAX, std::out_of_range);
    CHECK_THROWS(pointer -= PTRDIFF_MIN, std::out_of_range);

    pointer += 3;
    CHECK(pointer == span.End());
    CHECK_THROWS((void)*pointer, std::out_of_range);
    CHECK_THROWS(++pointer, std::out_of_range);
}

void MsPtrDifferentSpans() {
    MemorySpan<int> first = MakeRange(3);
    MemorySpan<int> second = MakeRange(3);
    MsPtr<int> a = first.Begin();
    MsPtr<int> b = second.Begin();
    CHECK(a != b);
    CHECK_THROWS((void)(a - b), std::invalid_argument);
    CHECK_THROWS((void)(a < b), std::invalid_argument);
}

void MsPtrOutlivesSpan() {
    const long before = Tracked::alive;
    MsPtr<Tracked> pointer;
    {
        MemorySpan<Tracked> span(4, Tracked(8));
        pointer = span.Locate(2);
    }
    CHECK_EQ(pointer->value, 8);
    CHECK_EQ((pointer + 1)->value, 8);
    CHECK_EQ(Tracked::alive, before + 4);
    pointer = MsPtr<Tracked>();
    CHECK_EQ(Tracked::alive, before);
}

void MsPtrSeesResize() {
    MemorySpan<int> span = MakeRange(5);
    MsPtr<int> pointer = span.Locate(4);
    span.Resize(2);
    CHECK(!pointer.IsDereferenceable());
    CHECK_THROWS((void)*pointer, std::out_of_range);
    pointer -= 3;
    CHECK_EQ(*pointer, 10);
    span.Resize(6);
    CHECK_EQ(pointer[4], 0);
}

void MsPtrRangeForAndShare() {
    MemorySpan<int> span = MakeRange(4);
    int sum = 0;
    for (int& value : span) {
        sum += value;
        value += 1;
    }
    CHECK_EQ(sum, 60);
    ShrdPtr<int> shared = (span.Begin() + 3).Share();
    CHECK_EQ(*shared, 31);
}

// ---------------------------------------------------------------------------
// Sequence
// ---------------------------------------------------------------------------

template <class SeqType>
void SequenceBasicOperations() {
    SeqType seq;
    CHECK(seq.IsEmpty());
    CHECK_THROWS(seq.GetFirst(), std::out_of_range);
    CHECK_THROWS(seq.Get(0), std::out_of_range);
    CHECK_THROWS(seq.RemoveAt(0), std::out_of_range);

    seq.Append(2);
    seq.Append(3);
    seq.Prepend(1);
    seq.InsertAt(3, 5);
    seq.InsertAt(3, 4);
    CHECK_EQ(seq.GetLength(), 5u);
    CHECK_EQ(seq.GetFirst(), 1);
    CHECK_EQ(seq.GetLast(), 5);
    CHECK_EQ(seq.Get(3), 4);
    CHECK_THROWS(seq.InsertAt(6, 0), std::out_of_range);

    seq.Set(2, 30);
    seq.RemoveAt(0);
    CHECK_EQ(seq.Get(1), 30);
    CHECK_EQ(seq.GetLength(), 4u);
    CHECK_THROWS(seq.Set(4, 0), std::out_of_range);
}

template <class SeqType>
void SequenceSubsequenceAndConcat() {
    SeqType seq{1, 2, 3, 4, 5};
    UnqPtr<Sequence<int>> sub = seq.GetSubsequence(1, 4);
    CHECK_EQ(sub->GetLength(), 3u);
    CHECK_EQ(sub->GetFirst(), 2);
    CHECK(dynamic_cast<SeqType*>(sub.Get()) != nullptr);
    CHECK_THROWS(seq.GetSubsequence(3, 2), std::out_of_range);
    CHECK_THROWS(seq.GetSubsequence(0, 6), std::out_of_range);

    UnqPtr<Sequence<int>> joined = seq.Concat(*sub);
    CHECK_EQ(joined->GetLength(), 8u);
    CHECK_EQ(joined->GetLast(), 4);
    CHECK_EQ(seq.GetLength(), 5u);
}

template <class SeqType>
void SequenceCopyThroughBasePointer() {
    UnqPtr<Sequence<int>> original = MakeUnq<SeqType>(SeqType{1, 2, 3});
    UnqPtr<Sequence<int>> copy = original;
    CHECK(dynamic_cast<SeqType*>(copy.Get()) != nullptr);
    copy->Set(0, 100);
    CHECK_EQ(original->Get(0), 1);
    CHECK_EQ(copy->Get(0), 100);
}

template <class SeqType>
void SequenceValueSemantics() {
    const long before = Tracked::alive;
    {
        SeqType seq;
        for (int i = 0; i < 1000; ++i) {
            seq.Append(Tracked(i));
        }
        SeqType copy = seq;
        copy.RemoveAt(0);
        CHECK_EQ(seq.Get(0).value, 0);
        CHECK_EQ(copy.Get(0).value, 1);
        CHECK_EQ(copy.GetLength(), 999u);
    }
    CHECK_EQ(Tracked::alive, before);
}

void ArrayBasicOperations() { SequenceBasicOperations<ArraySequence<int>>(); }
void ArraySubsequenceAndConcat() { SequenceSubsequenceAndConcat<ArraySequence<int>>(); }
void ArrayCopyThroughBasePointer() { SequenceCopyThroughBasePointer<ArraySequence<int>>(); }
void ArrayValueSemantics() { SequenceValueSemantics<ArraySequence<Tracked>>(); }

void ListBasicOperations() { SequenceBasicOperations<ListSequence<int>>(); }
void ListSubsequenceAndConcat() { SequenceSubsequenceAndConcat<ListSequence<int>>(); }
void ListCopyThroughBasePointer() { SequenceCopyThroughBasePointer<ListSequence<int>>(); }
void ListValueSemantics() { SequenceValueSemantics<ListSequence<Tracked>>(); }

void ArrayGeometricGrowth() {
    ArraySequence<int> seq;
    std::size_t reallocations = 0;
    std::size_t capacity = seq.GetCapacity();
    for (int i = 0; i < 100000; ++i) {
        seq.Append(i);
        if (seq.GetCapacity() != capacity) {
            capacity = seq.GetCapacity();
            ++reallocations;
        }
    }
    CHECK(reallocations <= 20);
    CHECK_EQ(seq.Get(99999), 99999);
}

void ListLongChain() {
    ListSequence<int> list;
    for (int i = 0; i < 1000000; ++i) {
        list.Prepend(i);
    }
    ListSequence<int> copy = list;
    CHECK_EQ(copy.GetLength(), 1000000u);
    CHECK_EQ(copy.GetLast(), 0);
}

void SequenceMixedTypes() {
    ArraySequence<std::string> array{"a", "b"};
    ListSequence<std::string> list{"c"};
    UnqPtr<Sequence<std::string>> joined = array.Concat(list);
    CHECK_EQ(joined->GetLength(), 3u);
    CHECK(joined->GetLast() == "c");
}

}  // namespace

int RunTests(const char* filter) {
    g_passed = 0;
    g_failed = 0;

    RunTest("UnqPtr.DefaultIsEmpty", &UnqPtrDefaultIsEmpty, filter);
    RunTest("UnqPtr.OwnsAndDestroys", &UnqPtrOwnsAndDestroys, filter);
    RunTest("UnqPtr.CopyIsDeep", &UnqPtrCopyIsDeep, filter);
    RunTest("UnqPtr.CopyKeepsDynamicType", &UnqPtrCopyKeepsDynamicType, filter);
    RunTest("UnqPtr.CopyOfAbstractBase", &UnqPtrCopyOfAbstractBase, filter);
    RunTest("UnqPtr.MoveTransfersOwnership", &UnqPtrMoveTransfersOwnership, filter);
    RunTest("UnqPtr.NonVirtualDestructor", &UnqPtrNonVirtualDestructor, filter);
    RunTest("UnqPtr.MultipleInheritance", &UnqPtrMultipleInheritance, filter);
    RunTest("UnqPtr.NonCopyableThrows", &UnqPtrNonCopyableThrows, filter);
    RunTest("UnqPtr.ExceptionInConstructor", &UnqPtrExceptionInConstructor, filter);
    RunTest("UnqPtr.ResetAndSwap", &UnqPtrResetAndSwap, filter);
    RunTest("UnqPtr.SelfAssignment", &UnqPtrSelfAssignment, filter);
    RunTest("UnqPtr.DeepConstness", &UnqPtrDeepConstness, filter);
    RunTest("UnqPtr.ManyObjectsNoLeaks", &UnqPtrManyObjectsNoLeaks, filter);

    RunTest("ShrdPtr.DefaultIsEmpty", &ShrdPtrDefaultIsEmpty, filter);
    RunTest("ShrdPtr.CountsReferences", &ShrdPtrCountsReferences, filter);
    RunTest("ShrdPtr.TakesObjectFromUnqPtr", &ShrdPtrTakesObjectFromUnqPtr, filter);
    RunTest("ShrdPtr.MoveDoesNotChangeCount", &ShrdPtrMoveDoesNotChangeCount, filter);
    RunTest("ShrdPtr.SubtypingSharesCounter", &ShrdPtrSubtypingSharesCounter, filter);
    RunTest("ShrdPtr.MultipleInheritance", &ShrdPtrMultipleInheritance, filter);
    RunTest("ShrdPtr.Casts", &ShrdPtrCasts, filter);
    RunTest("ShrdPtr.CloneIsIndependent", &ShrdPtrCloneIsIndependent, filter);
    RunTest("ShrdPtr.NonVirtualDestructor", &ShrdPtrNonVirtualDestructor, filter);
    RunTest("ShrdPtr.SelfAssignment", &ShrdPtrSelfAssignment, filter);
    RunTest("ShrdPtr.ExceptionInConstructor", &ShrdPtrExceptionInConstructor, filter);
    RunTest("ShrdPtr.CascadeRelease", &ShrdPtrCascadeRelease, filter);
    RunTest("ShrdPtr.CycleMustBeBrokenManually", &ShrdPtrCycleMustBeBrokenManually, filter);

    RunTest("MemorySpan.Construction", &SpanConstruction, filter);
    RunTest("MemorySpan.GetReturnsIndependentCopy", &SpanGetReturnsIndependentCopy, filter);
    RunTest("MemorySpan.CopyReturnsSharedReference", &SpanCopyReturnsSharedReference, filter);
    RunTest("MemorySpan.SharedElementOutlivesSpan", &SpanSharedElementOutlivesSpan, filter);
    RunTest("MemorySpan.SetIsVisibleEverywhere", &SpanSetIsVisibleEverywhere, filter);
    RunTest("MemorySpan.ReplaceKeepsOldObject", &SpanReplaceKeepsOldObject, filter);
    RunTest("MemorySpan.IndexChecks", &SpanIndexChecks, filter);
    RunTest("MemorySpan.DeepCopyAndMove", &SpanDeepCopyAndMove, filter);
    RunTest("MemorySpan.CopyPreservesDynamicTypes", &SpanCopyPreservesDynamicTypes, filter);
    RunTest("MemorySpan.SwapDoesNotCopyObjects", &SpanSwapDoesNotCopyObjects, filter);
    RunTest("MemorySpan.Resize", &SpanResize, filter);
    RunTest("MemorySpan.LargeNoLeaks", &SpanLargeNoLeaks, filter);

    RunTest("MsPtr.Unbound", &MsPtrUnbound, filter);
    RunTest("MsPtr.IncrementAndIndexing", &MsPtrIncrementAndIndexing, filter);
    RunTest("MsPtr.Arithmetic", &MsPtrArithmetic, filter);
    RunTest("MsPtr.CannotLeaveSpan", &MsPtrCannotLeaveSpan, filter);
    RunTest("MsPtr.DifferentSpans", &MsPtrDifferentSpans, filter);
    RunTest("MsPtr.OutlivesSpan", &MsPtrOutlivesSpan, filter);
    RunTest("MsPtr.SeesResize", &MsPtrSeesResize, filter);
    RunTest("MsPtr.RangeForAndShare", &MsPtrRangeForAndShare, filter);

    RunTest("ArraySequence.BasicOperations", &ArrayBasicOperations, filter);
    RunTest("ArraySequence.SubsequenceAndConcat", &ArraySubsequenceAndConcat, filter);
    RunTest("ArraySequence.CopyThroughBasePointer", &ArrayCopyThroughBasePointer, filter);
    RunTest("ArraySequence.ValueSemantics", &ArrayValueSemantics, filter);
    RunTest("ArraySequence.GeometricGrowth", &ArrayGeometricGrowth, filter);

    RunTest("ListSequence.BasicOperations", &ListBasicOperations, filter);
    RunTest("ListSequence.SubsequenceAndConcat", &ListSubsequenceAndConcat, filter);
    RunTest("ListSequence.CopyThroughBasePointer", &ListCopyThroughBasePointer, filter);
    RunTest("ListSequence.ValueSemantics", &ListValueSemantics, filter);
    RunTest("ListSequence.LongChain", &ListLongChain, filter);

    RunTest("Sequence.MixedTypes", &SequenceMixedTypes, filter);

    output::Printf("\nИтого: %d пройдено, %d провалено\n", g_passed, g_failed);
    return g_failed;
}
