#!/usr/bin/env python3
"""
Строит графики по CSV, который пишет `lab1 --bench`.
Использование: python3 scripts/plot.py [results/bench.csv] [results]
"""
import csv
import os
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

STYLE = {
    "raw new/delete": ("#555555", "o", "-"),
    "UnqPtr": ("#1f77b4", "s", "-"),
    "ShrdPtr": ("#d62728", "^", "-"),
    "std::unique_ptr": ("#1f77b4", "s", "--"),
    "std::shared_ptr": ("#d62728", "^", "--"),
}


def load(path):
    data = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as file:
        for row in csv.DictReader(file):
            values = {key: (row[key] if key == "impl" else float(row[key])) for key in row}
            data[row["impl"]].append(values)
    for rows in data.values():
        rows.sort(key=lambda row: row["n"])
    return data


def line_chart(data, column, title, ylabel, path):
    figure, axes = plt.subplots(figsize=(9, 5.5))
    for impl, rows in data.items():
        color, marker, dash = STYLE.get(impl, (None, "o", "-"))
        axes.plot([row["n"] for row in rows], [row[column] for row in rows],
                  label=impl, color=color, marker=marker, linestyle=dash)
    axes.set_xscale("log")
    axes.set_xlabel("число объектов N")
    axes.set_ylabel(ylabel)
    axes.set_title(title)
    axes.grid(True, which="both", alpha=0.3)
    axes.legend(fontsize=9)
    figure.tight_layout()
    figure.savefig(path, dpi=130)
    plt.close(figure)


def bar_chart(data, column, max_n, title, ylabel, path):
    impls = [impl for impl in data if any(row["n"] == max_n for row in data[impl])]
    values = [next(row[column] for row in data[impl] if row["n"] == max_n) for impl in impls]
    figure, axes = plt.subplots(figsize=(9, 5.5))
    axes.bar(impls, values)
    axes.set_ylabel(ylabel)
    axes.set_title(f"{title}, N = {int(max_n):,}".replace(",", " "))
    plt.setp(axes.get_xticklabels(), rotation=20, ha="right")
    figure.tight_layout()
    figure.savefig(path, dpi=130)
    plt.close(figure)


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "results/bench.csv"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "results"
    os.makedirs(out_dir, exist_ok=True)

    data = load(csv_path)
    max_n = max(row["n"] for rows in data.values() for row in rows)

    line_chart(data, "total_ns", "Полное время обслуживания одного объекта",
               "нс на объект", os.path.join(out_dir, "total_time.png"))
    line_chart(data, "create_ns", "Создание объекта",
               "нс на объект", os.path.join(out_dir, "create_time.png"))
    line_chart(data, "copy_ns", "Копирование указателя",
               "нс на объект", os.path.join(out_dir, "copy_time.png"))
    line_chart(data, "destroy_ns", "Удаление",
               "нс на объект", os.path.join(out_dir, "destroy_time.png"))
    bar_chart(data, "bytes_per_obj", max_n, "Память на объект",
              "байт на объект", os.path.join(out_dir, "memory.png"))
    bar_chart(data, "allocs_per_obj", max_n, "Выделений памяти на объект",
              "штук", os.path.join(out_dir, "allocations.png"))

    print("Графики сохранены в", out_dir)


if __name__ == "__main__":
    main()
