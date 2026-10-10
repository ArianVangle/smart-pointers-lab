#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Benchmark.hpp"
#include "Demo.hpp"
#include "Output.hpp"
#include "Tests.hpp"
#include "Ui.hpp"

namespace {

const char* const kTestsLog = "results/tests_output.txt";
const char* const kBenchLog = "results/bench_output.txt";
const char* const kBenchCsv = "results/bench.csv";

void PrintUsage(const char* program) {
    std::printf(
        "Использование:\n"
        "  %s                   интерактивное меню\n"
        "  %s --test [фильтр]   функциональные тесты\n"
        "  %s --bench [maxN]    нагрузочные тесты (по умолчанию 1e7)\n"
        "  %s --demo            демонстрация\n"
        "  %s --plot            построить графики по results/bench.csv\n"
        "\nОтчёты сохраняются в %s и %s.\n",
        program, program, program, program, program, kTestsLog, kBenchLog);
}

std::size_t ParseCount(const char* text) {
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0' || value < 10 || value > 1e9) {
        return 0;
    }
    return static_cast<std::size_t>(value);
}

int RunTestsMode(const char* filter) {
    if (!output::Open(kTestsLog)) {
        std::printf("%sПредупреждение:%s не удалось открыть %s, вывод только на экран\n", ui::Yellow(),
                    ui::Reset(), kTestsLog);
    }
    const int failed = RunTests(filter);
    if (output::IsOpen()) {
        std::printf("Отчёт сохранён в %s\n", kTestsLog);
    }
    output::Close();
    return failed == 0 ? 0 : 1;
}

int RunPlotMode() {
    std::fflush(stdout);
    if (std::system("python3 scripts/plot.py results/bench.csv results") == 0) {
        return 0;
    }
    if (std::system("python scripts/plot.py results/bench.csv results") == 0) {
        return 0;
    }
    std::printf(
        "%sГрафики не построены%s: нужен Python с matplotlib "
        "(pip install matplotlib),\nзатем: python3 scripts/plot.py %s results\n",
        ui::Yellow(), ui::Reset(), kBenchCsv);
    return 1;
}

int RunBenchMode(std::size_t maxObjects, bool plot) {
    if (!output::Open(kBenchLog)) {
        std::printf("%sПредупреждение:%s не удалось открыть %s, вывод только на экран\n", ui::Yellow(),
                    ui::Reset(), kBenchLog);
    }
    const int problems = RunBenchmarks(maxObjects, kBenchCsv);
    if (output::IsOpen()) {
        std::printf("Отчёт сохранён в %s\n", kBenchLog);
    }
    output::Close();
    if (plot) {
        RunPlotMode();
    }
    return problems == 0 ? 0 : 1;
}

void DemoMenu() {
    int selected = 0;
    for (;;) {
        ui::BeginScreen("Демонстрация", "выберите сцену");
        for (int i = 0; i < kDemoSceneCount; ++i) {
            std::printf("  %s%d.%s %s\n", ui::Dim(), i + 1, ui::Reset(), kDemoSceneNames[i]);
        }
        std::printf("  %s%d.%s Показать всё подряд\n", ui::Dim(), kDemoSceneCount + 1, ui::Reset());
        std::printf("  %s0.%s Назад\n\n", ui::Dim(), ui::Reset());

        const std::string choice = ui::AskLine("Выберите сцену:");
        if (choice.empty() || choice == "0") {
            return;
        }

        selected = std::atoi(choice.c_str());
        if (selected >= 1 && selected <= kDemoSceneCount) {
            RunDemoScene(selected - 1);
        } else if (selected == kDemoSceneCount + 1) {
            RunDemo();
        } else {
            continue;
        }
        ui::Pause("Enter — к списку сцен");
    }
}

void ShowMenu() {
    ui::BeginScreen("Лабораторная работа №1 — умные указатели",
                    "UnqPtr · ShrdPtr · MemorySpan · MsPtr · Sequence");
    std::printf("  %s1.%s Функциональные тесты\n", ui::Dim(), ui::Reset());
    std::printf("  %s2.%s Нагрузочное тестирование\n", ui::Dim(), ui::Reset());
    std::printf("  %s3.%s Демонстрация\n", ui::Dim(), ui::Reset());
    std::printf("  %s4.%s Построить графики по сохранённым результатам\n", ui::Dim(), ui::Reset());
    std::printf("  %s0.%s Выход\n\n", ui::Dim(), ui::Reset());
}

int RunMenu() {
    for (;;) {
        ShowMenu();
        const std::string choice = ui::AskLine("Выберите пункт:");

        if (choice.empty() || choice == "0") {
            return 0;
        }

        if (choice == "1") {
            ui::BeginScreen("Функциональные тесты", "проверка корректности и отсутствия утечек");
            const std::string filter = ui::AskLine("Фильтр имени теста (Enter — все):");
            std::printf("\n");
            RunTestsMode(filter.empty() ? nullptr : filter.c_str());
        } else if (choice == "2") {
            ui::BeginScreen("Нагрузочное тестирование", "время и память в сравнении с STL");
            const std::string maxN = ui::AskLine("Максимальное N (Enter — 1e7):");
            const std::size_t parsed = maxN.empty() ? 10000000 : ParseCount(maxN.c_str());
            if (parsed == 0) {
                std::printf("\n%sНекорректное N%s: ожидается число от 10 до 1e9\n", ui::Red(), ui::Reset());
            } else {
                std::printf("\n");
                RunBenchMode(parsed, true);
            }
        } else if (choice == "3") {
            DemoMenu();
            continue;
        } else if (choice == "4") {
            ui::BeginScreen("Графики", "построение по results/bench.csv");
            RunPlotMode();
        } else {
            ui::BeginScreen("Ошибка", nullptr);
            std::printf("Нет такого пункта: %s\n", choice.c_str());
        }

        ui::Pause("Enter — вернуться в меню");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        const ui::ScreenSession session;
        return RunMenu();
    }

    const char* mode = argv[1];
    if (std::strcmp(mode, "--test") == 0) {
        return RunTestsMode(argc > 2 ? argv[2] : nullptr);
    }
    if (std::strcmp(mode, "--bench") == 0) {
        std::size_t maxObjects = 10000000;
        if (argc > 2) {
            maxObjects = ParseCount(argv[2]);
            if (maxObjects == 0) {
                std::printf("Некорректное maxN: %s (ожидается число от 10 до 1e9)\n", argv[2]);
                return 2;
            }
        }
        return RunBenchMode(maxObjects, true);
    }
    if (std::strcmp(mode, "--demo") == 0) {
        RunDemo();
        return 0;
    }
    if (std::strcmp(mode, "--plot") == 0) {
        return RunPlotMode();
    }

    PrintUsage(argv[0]);
    return 2;
}
