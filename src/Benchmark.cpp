#include "Benchmark.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <type_traits>

#include "AllocCounter.hpp"
#include "Output.hpp"
#include "ShrdPtr.hpp"
#include "UnqPtr.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double NanosecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

volatile long long g_sink = 0;

template <class Ptr>
Ptr CopyOf(const Ptr& pointer) {
    if constexpr (std::is_copy_constructible_v<Ptr>) {
        return pointer;
    } else {
        return std::make_unique<int>(*pointer);
    }
}

template <class Ptr>
void ReleaseOriginal(Ptr& pointer) {
    if constexpr (std::is_pointer_v<Ptr>) {
        delete pointer;
        pointer = nullptr;
    } else {
        pointer = Ptr();
    }
}

template <class Ptr>
void ReleaseCopy(Ptr& pointer) {
    if constexpr (std::is_pointer_v<Ptr>) {
        pointer = nullptr;
    } else {
        pointer = Ptr();
    }
}

struct Row {
    const char* name;
    double createNs = 0;
    double accessNs = 0;
    double copyNs = 0;
    double destroyNs = 0;
    double bytesPerObject = 0;
    double allocationsPerObject = 0;
    long long leakedBytes = 0;
    bool checksumOk = true;
};

template <class Ptr, class Make>
Row Measure(const char* name, std::size_t n, std::size_t repeats, Make make) {
    Row row;
    row.name = name;

    const long long expectedSum = static_cast<long long>(n) * static_cast<long long>(n - 1) / 2;
    const std::size_t bytesBefore = alloc_stats::Current().liveBytes;
    const std::size_t allocationsBefore = alloc_stats::Current().totalAllocations;
    bool memoryRecorded = false;

    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        Ptr* originals = new Ptr[n]();
        Ptr* copies = new Ptr[n]();

        const std::size_t baseline = alloc_stats::Current().liveBytes;
        const std::size_t allocationsBaseline = alloc_stats::Current().totalAllocations;

        auto start = Clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            originals[i] = make(static_cast<int>(i));
        }
        row.createNs += NanosecondsSince(start);

        if (!memoryRecorded) {
            const auto objects = static_cast<double>(n);
            row.bytesPerObject = static_cast<double>(alloc_stats::Current().liveBytes - baseline) / objects;
            row.allocationsPerObject =
                static_cast<double>(alloc_stats::Current().totalAllocations - allocationsBaseline) / objects;
            memoryRecorded = true;
        }

        start = Clock::now();
        long long sum = 0;
        for (std::size_t i = 0; i < n; ++i) {
            sum += *originals[i];
        }
        row.accessNs += NanosecondsSince(start);
        g_sink = sum;
        row.checksumOk = row.checksumOk && sum == expectedSum;

        start = Clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            copies[i] = CopyOf(originals[i]);
        }
        row.copyNs += NanosecondsSince(start);

        start = Clock::now();
        for (std::size_t i = 0; i < n; ++i) {
            ReleaseCopy(copies[i]);
        }
        for (std::size_t i = 0; i < n; ++i) {
            ReleaseOriginal(originals[i]);
        }
        row.destroyNs += NanosecondsSince(start);

        delete[] copies;
        delete[] originals;
    }

    const double operations = static_cast<double>(n) * static_cast<double>(repeats);
    row.createNs /= operations;
    row.accessNs /= operations;
    row.copyNs /= operations;
    row.destroyNs /= operations;

    row.leakedBytes =
        static_cast<long long>(alloc_stats::Current().liveBytes) - static_cast<long long>(bytesBefore);
    (void)allocationsBefore;
    return row;
}

void PrintHeader(std::size_t n) {
    output::Printf("\nN = %zu   (время — нс на объект, память — байт на объект)\n", n);
    output::Printf("%-22s %9s %9s %9s %9s %9s %9s %8s %6s\n", "implementation", "create", "access", "copy",
                   "destroy", "total", "bytes", "allocs", "leak");
}

void PrintRow(const Row& row) {
    const double total = row.createNs + row.accessNs + row.copyNs + row.destroyNs;
    output::Printf("%-22s %9.2f %9.2f %9.2f %9.2f %9.2f %9.1f %8.2f %6lld%s\n", row.name, row.createNs,
                   row.accessNs, row.copyNs, row.destroyNs, total, row.bytesPerObject,
                   row.allocationsPerObject, row.leakedBytes,
                   row.checksumOk ? "" : "  ОШИБКА КОНТРОЛЬНОЙ СУММЫ");
}

void WriteCsvRow(std::FILE* csv, std::size_t n, const Row& row) {
    if (csv == nullptr) {
        return;
    }
    const double total = row.createNs + row.accessNs + row.copyNs + row.destroyNs;
    std::fprintf(csv, "%s,%zu,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.4f,%lld\n", row.name, n, row.createNs,
                 row.accessNs, row.copyNs, row.destroyNs, total, row.bytesPerObject, row.allocationsPerObject,
                 row.leakedBytes);
}

}  // namespace

int RunBenchmarks(std::size_t maxObjects, const char* csvPath) {
    std::FILE* csv = std::fopen(csvPath, "w");
    if (csv == nullptr) {
        output::Printf("Предупреждение: не удалось открыть %s, CSV не будет записан\n", csvPath);
    } else {
        std::fprintf(csv,
                     "impl,n,create_ns,access_ns,copy_ns,destroy_ns,total_ns,"
                     "bytes_per_obj,allocs_per_obj,leaked_bytes\n");
    }

    output::Printf(
        "Размеры указателей (байт): сырой %zu, UnqPtr %zu, ShrdPtr %zu, "
        "std::unique_ptr %zu, std::shared_ptr %zu\n",
        sizeof(int*), sizeof(smart::UnqPtr<int>), sizeof(smart::ShrdPtr<int>), sizeof(std::unique_ptr<int>),
        sizeof(std::shared_ptr<int>));

    int problems = 0;
    for (std::size_t n = 10; n <= maxObjects; n *= 10) {
        std::size_t repeats = 1000000 / n;
        if (repeats == 0) {
            repeats = 1;
        }

        PrintHeader(n);
        const Row rows[] = {
            Measure<int*>("raw new/delete", n, repeats, [](int i) { return new int(i); }),
            Measure<smart::UnqPtr<int>>("UnqPtr", n, repeats, [](int i) { return smart::MakeUnq<int>(i); }),
            Measure<smart::ShrdPtr<int>>("ShrdPtr", n, repeats,
                                         [](int i) { return smart::MakeShrd<int>(i); }),
            Measure<std::unique_ptr<int>>("std::unique_ptr", n, repeats,
                                          [](int i) { return std::make_unique<int>(i); }),
            Measure<std::shared_ptr<int>>("std::shared_ptr", n, repeats,
                                          [](int i) { return std::make_shared<int>(i); }),
        };

        for (const Row& row : rows) {
            PrintRow(row);
            WriteCsvRow(csv, n, row);
            if (row.leakedBytes != 0 || !row.checksumOk) {
                ++problems;
            }
        }

        if (n > maxObjects / 10) {
            break;
        }
    }

    if (csv != nullptr) {
        std::fclose(csv);
        output::Printf("\nДанные сохранены в %s\n", csvPath);
    }
    output::Printf("Утечек обнаружено: %s\n", problems == 0 ? "нет" : "ЕСТЬ");
    return problems;
}
