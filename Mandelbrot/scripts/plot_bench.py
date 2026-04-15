#!/usr/bin/env python3
"""Build a simple SVG chart from Mandelbrot benchmark CSV files.

Shows:
- bar: mean benchmark value
- blue error bar: sample stddev across CSV runs
"""

from __future__ import annotations

import argparse
import csv
import glob
import math
import os
from collections import defaultdict
from statistics import fmean
from typing import Dict, List, Tuple


ALIASES = {
    "v3_neon": "v3neon8",
    "v3_neon8": "v3neon8",
    "v3neon": "v3neon8",
    "v3_x86": "v3x86",
    "v3_neon4": "v3neon4",
}

ORDER = ["v1", "v2", "v3neon8", "v3neon4", "v3neon16", "v3x86"]


class ImplStats:
    def __init__(self) -> None:
        self.values: List[float] = []

    def add(self, value: float) -> None:
        self.values.append(value)

    @property
    def mean(self) -> float:
        return fmean(self.values) if self.values else 0.0

    @property
    def stddev(self) -> float:
        n = len(self.values)
        if n <= 1:
            return 0.0
        m = self.mean
        var = sum((x - m) ** 2 for x in self.values) / (n - 1)
        return math.sqrt(var)



def normalize_impl(name: str) -> str:
    return ALIASES.get(name.strip(), name.strip())



def read_rows(paths: List[str]) -> Tuple[str, Dict[str, ImplStats]]:
    stats: Dict[str, ImplStats] = defaultdict(ImplStats)
    metric = ""

    for path in paths:
        with open(path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                if row.get("available", "0") != "1":
                    continue

                impl = normalize_impl(row.get("impl", "").strip())
                avg_ms = float(row.get("avg_ms", "0") or 0)
                avg_ticks = float(row.get("avg_ticks", "0") or 0)

                if avg_ms > 0.0:
                    metric = "avg_ms"
                    stats[impl].add(avg_ms)
                else:
                    if not metric:
                        metric = "avg_ticks"
                    if metric == "avg_ticks":
                        stats[impl].add(avg_ticks)

    return metric or "avg_ms", stats



def fmt_value(v: float, metric: str) -> str:
    if metric == "avg_ms":
        return f"{v:.3f} ms"
    return f"{v:.0f} ticks"



def build_svg(metric: str, stats: Dict[str, ImplStats], title: str) -> str:
    impls = [i for i in ORDER if i in stats]
    if not impls:
        raise ValueError("No benchmark data with available=1 found")

    means = [stats[i].mean for i in impls]
    stds = [stats[i].stddev for i in impls]
    max_y = max(m + s for m, s in zip(means, stds))
    if max_y <= 0:
        max_y = 1.0

    width = 1100
    height = 680
    left = 80
    right = 40
    top = 70
    bottom = 120
    plot_w = width - left - right
    plot_h = height - top - bottom

    n = len(impls)
    slot = plot_w / n
    bar_w = slot * 0.45

    def ypx(v: float) -> float:
        return top + plot_h - (v / max_y) * plot_h

    parts: List[str] = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    parts.append('<rect width="100%" height="100%" fill="#ffffff"/>')
    parts.append(f'<text x="{left}" y="35" font-size="24" font-family="Arial" fill="#111">{title}</text>')
    parts.append(f'<text x="{left}" y="58" font-size="14" font-family="Arial" fill="#333">Метрика: {"среднее время (ms)" if metric == "avg_ms" else "среднее число тиков"}</text>')

    # grid + y labels
    for i in range(6):
        val = max_y * i / 5.0
        y = ypx(val)
        parts.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_w}" y2="{y:.2f}" stroke="#e5e7eb" stroke-width="1"/>')
        parts.append(f'<text x="{left - 10}" y="{y + 5:.2f}" text-anchor="end" font-size="12" font-family="Arial" fill="#555">{val:.2f}</text>')

    parts.append(f'<line x1="{left}" y1="{top + plot_h}" x2="{left + plot_w}" y2="{top + plot_h}" stroke="#111"/>')
    parts.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_h}" stroke="#111"/>')

    for idx, impl in enumerate(impls):
        mean = means[idx]
        std = stds[idx]

        x_center = left + slot * idx + slot / 2.0
        x_bar = x_center - bar_w / 2.0
        y_bar = ypx(mean)
        y_zero = ypx(0)

        # bar
        parts.append(
            f'<rect x="{x_bar:.2f}" y="{y_bar:.2f}" width="{bar_w:.2f}" height="{(y_zero - y_bar):.2f}" fill="#4f46e5" opacity="0.85"/>'
        )

        # stddev error bar (blue)
        y_std_top = ypx(mean + std)
        y_std_bot = ypx(max(mean - std, 0.0))
        parts.append(f'<line x1="{x_center:.2f}" y1="{y_std_top:.2f}" x2="{x_center:.2f}" y2="{y_std_bot:.2f}" stroke="#1d4ed8" stroke-width="3"/>')
        parts.append(f'<line x1="{x_center - 9:.2f}" y1="{y_std_top:.2f}" x2="{x_center + 9:.2f}" y2="{y_std_top:.2f}" stroke="#1d4ed8" stroke-width="3"/>')
        parts.append(f'<line x1="{x_center - 9:.2f}" y1="{y_std_bot:.2f}" x2="{x_center + 9:.2f}" y2="{y_std_bot:.2f}" stroke="#1d4ed8" stroke-width="3"/>')

        parts.append(f'<text x="{x_center:.2f}" y="{top + plot_h + 22}" text-anchor="middle" font-size="13" font-family="Arial" fill="#111">{impl}</text>')
        parts.append(f'<text x="{x_center:.2f}" y="{y_bar - 8:.2f}" text-anchor="middle" font-size="12" font-family="Arial" fill="#111">{fmt_value(mean, metric)}</text>')

    lx = left + plot_w - 250
    ly = top + 10
    parts.append(f'<rect x="{lx}" y="{ly}" width="230" height="45" fill="#fff" stroke="#d1d5db"/>')
    parts.append(f'<line x1="{lx + 14}" y1="{ly + 24}" x2="{lx + 46}" y2="{ly + 24}" stroke="#1d4ed8" stroke-width="3"/>')
    parts.append(f'<text x="{lx + 54}" y="{ly + 29}" font-size="12" font-family="Arial" fill="#111">Факт. отклонение (stddev)</text>')

    parts.append('</svg>')
    return "\n".join(parts)



def main() -> int:
    parser = argparse.ArgumentParser(description="Generate benchmark SVG chart with mean + deviations")
    parser.add_argument("--input-glob", default="bench_results/*.csv", help="Input CSV glob")
    parser.add_argument("--output", default="bench_results/benchmark_avg.svg", help="Output SVG path")
    parser.add_argument("--title", default="Mandelbrot benchmark (average)", help="Chart title")
    args = parser.parse_args()

    paths = sorted(glob.glob(args.input_glob))
    if not paths:
        raise SystemExit(f"No CSV files found by pattern: {args.input_glob}")

    metric, stats = read_rows(paths)
    svg = build_svg(metric, stats, args.title)

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(svg)

    print(f"SVG saved: {args.output}")
    print(f"CSV files used: {len(paths)}")
    print(f"Metric: {metric}")
    for impl in ORDER:
        if impl not in stats:
            continue
        s = stats[impl]
        print(f"{impl:8} mean={s.mean:.4f} stddev={s.stddev:.4f} n={len(s.values)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
