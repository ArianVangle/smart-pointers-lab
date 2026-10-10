#include "Demo.hpp"

#include <cstdio>
#include <stdexcept>
#include <utility>

#include "ArraySequence.hpp"
#include "ListSequence.hpp"
#include "MemorySpan.hpp"
#include "Ui.hpp"

using namespace smart;

namespace {

void Step(const char* code) { std::printf("\n  %s%s%s\n", ui::Cyan(), code, ui::Reset()); }

void Note(const char* text) { std::printf("      %s%s%s\n", ui::Dim(), text, ui::Reset()); }

void Good(const char* text) { std::printf("      %s✓%s %s\n", ui::Green(), ui::Reset(), text); }

void Bad(const char* text) { std::printf("      %s✗%s %s\n", ui::Red(), ui::Reset(), text); }

void Value(const char* label, const char* text) {
    std::printf("      %s: %s%s%s\n", label, ui::Bold(), text, ui::Reset());
}

struct Animal {
    explicit Animal(const char* n) : name(n) {
        std::printf("      %s+ создан Animal(%s)%s\n", ui::Green(), name, ui::Reset());
    }
    Animal(const Animal& other) : name(other.name) {
        std::printf("      %s+ скопирован Animal(%s)%s\n", ui::Green(), name, ui::Reset());
    }
    virtual ~Animal() { std::printf("      %s- удалён Animal(%s)%s\n", ui::Red(), name, ui::Reset()); }
    virtual const char* Voice() const { return "..."; }
    const char* name;
};

struct Cat : Animal {
    explicit Cat(const char* n) : Animal(n) {}
    ~Cat() override { std::printf("      %s- удалён Cat(%s)%s\n", ui::Red(), name, ui::Reset()); }
    const char* Voice() const override { return "мяу"; }
};

void DrawSharing(const char* const* holders, int count, std::size_t useCount) {
    for (int i = 0; i < count; ++i) {
        const char* branch = count == 1 ? "──" : (i == 0 ? "──┐" : (i + 1 == count ? "──┘" : "──┤"));
        if (i == count / 2 && count > 1) {
            std::printf("      %-6s%s──> [ Cat ]  %sсчётчик = %zu%s\n", holders[i], branch, ui::Yellow(),
                        useCount, ui::Reset());
        } else {
            std::printf("      %-6s%s\n", holders[i], branch);
        }
    }
    if (count == 1) {
        std::printf("      %s──> [ Cat ]  %sсчётчик = %zu%s\n", "", ui::Yellow(), useCount, ui::Reset());
    }
}

void DrawSpan(MemorySpan<int>& span, long pointerIndex, const char* pointerName) {
    std::printf("      ┌");
    for (std::size_t i = 0; i < span.Size(); ++i) {
        std::printf("──────%s", i + 1 == span.Size() ? "┐" : "┬");
    }
    std::printf("\n      ");
    for (std::size_t i = 0; i < span.Size(); ++i) {
        std::printf("│ %s%4d%s ", ui::Bold(), *span.Get(i), ui::Reset());
    }
    std::printf("│\n      └");
    for (std::size_t i = 0; i < span.Size(); ++i) {
        std::printf("──────%s", i + 1 == span.Size() ? "┘" : "┴");
    }
    std::printf("\n");
    if (pointerIndex >= 0) {
        std::printf("      ");
        for (long i = 0; i < pointerIndex; ++i) {
            std::printf("       ");
        }
        std::printf("   %s▲%s\n      ", ui::Cyan(), ui::Reset());
        for (long i = 0; i < pointerIndex; ++i) {
            std::printf("       ");
        }
        std::printf("   %s%s (индекс %ld)%s\n", ui::Cyan(), pointerName, pointerIndex, ui::Reset());
    }
}

void DemoUnq() {
    ui::BeginScreen("UnqPtr — ведущий указатель", "копия указателя означает копию объекта");

    Step("UnqPtr<Animal> master = MakeUnq<Cat>(\"Мурка\");");
    UnqPtr<Animal> master = MakeUnq<Cat>("Мурка");
    Value("голос", master->Voice());

    Step("UnqPtr<Animal> copy = master;        // копируется сам объект");
    UnqPtr<Animal> copy = master;
    if (copy.Get() != master.Get()) {
        Good("адреса разные: два указателя никогда не владеют одним объектом");
    } else {
        Bad("адреса совпали");
    }
    Good("копия осталась кошкой, хотя тип указателя — Animal: срезки нет");
    Note("тип объекта запомнен при создании, копирование идёт по нему");

    Step("copy.Reset();                        // разрушаем копию");
    copy.Reset();
    Good("вызван деструктор Cat — удаление тоже идёт по запомненному типу");

    Step("UnqPtr<Animal> other = std::move(master);   // перемещение, без копии");
    UnqPtr<Animal> other = std::move(master);
    if (!master) {
        Good("источник перемещения пуст, объект переехал без копирования");
    } else {
        Bad("источник не опустел");
    }

    Step("выход из области видимости — delete писать не нужно:");
}

void DemoShrd() {
    ui::BeginScreen("ShrdPtr — дескриптор", "подсчёт ссылок через size_t* referenceCount");

    Step("ShrdPtr<Animal> a = MakeShrd<Cat>(\"Рыжик\");");
    ShrdPtr<Animal> a = MakeShrd<Cat>("Рыжик");
    const char* one[] = {"a"};
    DrawSharing(one, 1, a.UseCount());

    Step("ShrdPtr<Animal> b = a;   ShrdPtr<Animal> c = b;");
    {
        ShrdPtr<Animal> b = a;
        ShrdPtr<Animal> c = b;
        const char* three[] = {"a", "b", "c"};
        DrawSharing(three, 3, a.UseCount());
        Good("объект не копировался — копии увеличили общий счётчик");

        Step("b и c выходят из области видимости");
    }
    DrawSharing(one, 1, a.UseCount());
    Note("объект ещё жив: остался один владелец");

    Step("ShrdPtr<Cat> cat = DynamicPointerCast<Cat>(a);   // приведение вниз");
    ShrdPtr<Cat> cat = DynamicPointerCast<Cat>(a);
    if (cat) {
        Good("внутри действительно Cat — приведение удалось, счётчик общий");
    } else {
        Bad("приведение не удалось");
    }
    std::printf("      %sсчётчик = %zu%s\n", ui::Yellow(), a.UseCount(), ui::Reset());

    Step("a.Reset(); cat.Reset();              // уходит последний владелец");
    a.Reset();
    cat.Reset();
    Note("счётчик дошёл до нуля — объект удалён ровно один раз");
}

void DemoSpan() {
    ui::BeginScreen("MemorySpan и MsPtr", "массив и безопасная арифметика указателей");

    Step("MemorySpan<int> span{10, 20, 30, 40};");
    MemorySpan<int> span{10, 20, 30, 40};
    DrawSpan(span, -1, "");

    Step("ShrdPtr<int> ref = span.Copy(0);   UnqPtr<int> own = span.Get(0);   span.Set(0, 111);");
    ShrdPtr<int> ref = span.Copy(0);
    UnqPtr<int> own = span.Get(0);
    span.Set(0, 111);
    std::printf("      Copy (ссылка на элемент) -> %s%d%s   %sизменение видно%s\n", ui::Bold(), *ref,
                ui::Reset(), ui::Green(), ui::Reset());
    std::printf("      Get  (копия элемента)    -> %s%d%s     %sнезависимая копия%s\n", ui::Bold(), *own,
                ui::Reset(), ui::Yellow(), ui::Reset());

    Step("MsPtr<int> p = span.Begin();  p += 2;");
    MsPtr<int> p = span.Begin();
    p += 2;
    DrawSpan(span, static_cast<long>(p.Index()), "p");
    std::printf("      *p = %s%d%s,  span.End() - p = %td\n", ui::Bold(), *p, ui::Reset(), span.End() - p);

    Step("p += 5;                              // выход за границу массива");
    try {
        p += 5;
        Bad("указатель вышел за границы (этого не должно происходить)");
    } catch (const std::out_of_range& error) {
        Bad(error.what());
        Good("исключение вместо порчи чужой памяти");
    }
    Note("положение указателя не изменилось — им можно пользоваться дальше");

    Step("MsPtr переживает сам массив:");
    MsPtr<int> survivor;
    {
        MemorySpan<int> temporary{7, 8, 9};
        survivor = temporary.Locate(1);
    }
    std::printf("      массив уничтожен, *survivor = %s%d%s\n", ui::Bold(), *survivor, ui::Reset());
    Good("хранилище живо, пока на него ссылается хотя бы один MsPtr");
}

void DemoSequence() {
    ui::BeginScreen("Sequence", "контейнеры, построенные на умных указателях");

    Step("ArraySequence<int> array{1, 2, 3};   ListSequence<int> list{4, 5};");
    sequence::ArraySequence<int> array{1, 2, 3};
    sequence::ListSequence<int> list{4, 5};
    Note("массив построен на MemorySpan, список — на цепочке UnqPtr<Node>");

    Step("UnqPtr<Sequence<int>> joined = array.Concat(list);");
    UnqPtr<sequence::Sequence<int>> joined = array.Concat(list);
    std::printf("      результат: %s", ui::Bold());
    for (std::size_t i = 0; i < joined->GetLength(); ++i) {
        std::printf("%d ", joined->Get(i));
    }
    std::printf("%s\n", ui::Reset());

    Step("UnqPtr<Sequence<int>> copy = joined;   // копия клонирует КОНКРЕТНЫЙ тип");
    UnqPtr<sequence::Sequence<int>> copy = joined;
    copy->Set(0, 100);
    std::printf("      оригинал[0] = %s%d%s, копия[0] = %s%d%s\n", ui::Bold(), joined->Get(0), ui::Reset(),
                ui::Bold(), copy->Get(0), ui::Reset());
    if (dynamic_cast<sequence::ArraySequence<int>*>(copy.Get()) != nullptr) {
        Good("копия — снова ArraySequence, хотя указатель имеет тип Sequence");
    } else {
        Bad("тип копии не сохранился");
    }

    Step("ListSequence: миллион узлов удаляется в цикле, без переполнения стека");
    sequence::ListSequence<int> big;
    for (int i = 0; i < 1000000; ++i) {
        big.Prepend(i);
    }
    std::printf("      длина = %s%zu%s, последний = %s%d%s\n", ui::Bold(), big.GetLength(), ui::Reset(),
                ui::Bold(), big.GetLast(), ui::Reset());
    Good("рекурсивная цепочка деструкторов здесь уронила бы программу");
}

void DemoSubtyping() {
    ui::BeginScreen("Подтипизация", "что компилируется, а что нет");

    Note("разрешено (Cat — подтип Animal):");
    Step("ShrdPtr<Cat> cat = MakeShrd<Cat>(\"Кузя\");");
    ShrdPtr<Cat> cat = MakeShrd<Cat>("Кузя");
    Step("ShrdPtr<Animal> animal = cat;                    // работает");
    ShrdPtr<Animal> animal = cat;
    Good("ShrdPtr<Cat> -> ShrdPtr<Animal>");
    Value("голос через базовый указатель", animal->Voice());

    Note("");
    Note("запрещено на этапе компиляции:");
    Step("ShrdPtr<Cat> back = animal;                      // ошибка компиляции");
    Bad("Animal не подтип Cat — отсекается концептом PointerCompatible");
    Step("MsPtr<Animal> p = spanOfCats.Locate(0);          // ошибка компиляции");
    Bad("арифметика указателя на базу по массиву наследников — это срезка");

    Note("");
    Note("привести вниз можно только явно:");
    Step("ShrdPtr<Cat> back = DynamicPointerCast<Cat>(animal);");
    ShrdPtr<Cat> back = DynamicPointerCast<Cat>(animal);
    if (back) {
        Good("тип проверен во время выполнения, приведение удалось");
    } else {
        Bad("приведение не удалось");
    }
    Note("эти правила зафиксированы в тестах через static_assert");
}

}  // namespace

const char* const kDemoSceneNames[kDemoSceneCount] = {
    "UnqPtr — владение и копирование объекта",    "ShrdPtr — подсчёт ссылок",
    "MemorySpan и MsPtr — безопасная арифметика", "Sequence — контейнеры на умных указателях",
    "Подтипизация — что можно, а что нет",
};

void RunDemoScene(int scene) {
    switch (scene) {
        case 0:
            DemoUnq();
            break;
        case 1:
            DemoShrd();
            break;
        case 2:
            DemoSpan();
            break;
        case 3:
            DemoSequence();
            break;
        case 4:
            DemoSubtyping();
            break;
        default:
            break;
    }
    std::printf("\n");
}

void RunDemo() {
    for (int scene = 0; scene < kDemoSceneCount; ++scene) {
        RunDemoScene(scene);
        if (scene + 1 < kDemoSceneCount) {
            ui::Pause("Enter — следующая сцена");
        }
    }
}
