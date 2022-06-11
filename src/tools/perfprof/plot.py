##
# Performance Profile by D. Salvagnin (2016)
# Internal use only, not to be distributed
##
# Modified by Davide Paro (@dparo) in 2022:
# - Ported to python3
# - Ability to plot raw data directly  (no mandatory ratio computation)
# - Generalized some commandline options to allow for plotting
#   of arbitrary quantities/statistics

# !/usr/bin/env python3

import argparse
import sys
from dataclasses import dataclass, field
from typing import List

import matplotlib
import matplotlib.pyplot as plt
import numpy as np


def errprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


# Constants
PLOT_LINE_WIDTH = 1.2
PLOT_MARKER_SIZE = 7
PLOT_MAX_LEGEND_NAME_LEN = 64
PLOT_GRID_LINE_WIDTH = 0.1
PLOT_GRID_ALPHA = 0.5
LEGEND_FONT_SIZE = 6
NUM_TICKS_IN_PLOT = 8
TICKS_NUM_DECIMALS = 3

DASHES = ["solid", "dotted", "dashed", "dashdot"]
MARKERS = ["s", "^", "o", "d", "v", "<", ">", "*", "2", "x", "+"]
COLORS = [
    "tab:blue",
    "tab:orange",
    "tab:green",
    "tab:red",
    "tab:purple",
    "tab:brown",
    "tab:pink",
    "tab:gray",
    "tab:olive",
    "magenta",
]


class CmdLineParser(object):
    def __init__(self):
        self.parser = argparse.ArgumentParser(
            usage="Usage: python3 perfprof.py [options] cvsfile.csv outputfile.pdf"
        )
        # default options
        self.parser.add_argument(
            "-D",
            "--delimiter",
            dest="delimiter",
            default=None,
            help="Delimiter for input files",
        )
        self.parser.add_argument(
            "-m",
            "--x-min",
            dest="x_min",
            default=None,
            type=float,
            help="Minimum X value for perf. profile",
        )
        self.parser.add_argument(
            "-M",
            "--x-max",
            dest="x_max",
            default=None,
            type=float,
            help="Minimum X value for perf. profile",
        )
        self.parser.add_argument(
            "-S",
            "--shift",
            dest="shift",
            default=0.0,
            type=float,
            help="shift for data",
        )
        self.parser.add_argument(
            "--logplot",
            dest="logplot",
            action="store_true",
            default=False,
            help="Enable logscale for X",
        )
        self.parser.add_argument(
            "--x-raw-lower-limit",
            dest="x_raw_lower_limit",
            default=None,
            type=float,
            help="Raw Lower limit for the X axis",
        )
        self.parser.add_argument(
            "--x-raw-upper-limit",
            dest="x_raw_upper_limit",
            default=None,
            type=float,
            help="Raw Upper limit for the X axis",
        )
        self.parser.add_argument(
            "-P", "--plot-title", dest="plottitle", default=None, help="plot title"
        )
        self.parser.add_argument(
            "-l", "--legend", dest="plotlegend", default=True, help="plot the legend"
        )
        self.parser.add_argument(
            "-X",
            "--x-label",
            dest="xlabel",
            type=str,
            default=None,
            help="x axis label",
        )
        self.parser.add_argument(
            "-B", "--bw", dest="bw", action="store_true", default=False, help="plot B/W"
        )
        self.parser.add_argument(
            "--raw-data",
            dest="raw_data",
            action="store_true",
            default=False,
            help="Keep raw data and do not apply further processing before plotting",
        )
        self.parser.add_argument(
            "--style-offset",
            dest="style_offset",
            default=0,
            type=int,
            help="Start index offset to associate with the line style/colors",
        )
        self.parser.add_argument(
            "--draw-reduced-cost-regions",
            dest="draw_reduced_cost_regions",
            action="store_true",
            default=False,
            help="Draw red/green separation region",
        )
        self.parser.add_argument(
            "-i", dest="input", type=str, required=True, help="Input csv file"
        )
        self.parser.add_argument(
            "-o", dest="output", type=str, required=True, help="Output PDF plot file"
        )

    def parse(self):
        return self.parser.parse_args()


@dataclass
class ParsedCsvContents:
    instance_names: List[str]
    solver_names: List[str]
    data: np.ndarray


@dataclass
class ProcessedData:
    x_min: float
    x_max: float
    data: np.ndarray

    y_min: float = 0.0
    y_max: float = 1.0

    # Plottable X, Y arrays
    x: np.ndarray = field(init=False)
    y: np.ndarray = field(init=False)
    nrows: int = field(init=False)
    ncols: int = field(init=False)

    def __post_init__(self):
        self.nrows, self.ncols = self.data.shape

        self.x = np.sort(self.data, axis=0)
        self.y = np.linspace(self.y_min, self.y_max, self.nrows)

        assert self.y.shape[0] == self.nrows


def read_csv(fp, delimiter):
    """
    Read a CSV file with performance profile specification.

    The format is as follows:

        ncols          |  solver1      |  solver2       |  ...
        ======================================================
        instance_name1    time(algo1)     time(algo2)      ...
        instance_name2    time(algo1)     time(algo2)      ...
        instance_name3    time(algo1)     time(algo2)      ...
        ...               ...             ...              ...
    """
    firstline = fp.readline().strip().split(delimiter)
    solver_names = firstline[1:]
    num_solvers = len(solver_names)
    instance_names = []
    data = []
    for line in fp:
        row = line.strip().split(delimiter)
        instance_names.append(row[0])
        row_data = np.empty(num_solvers)
        assert len(row[1:]) == num_solvers
        for j in range(num_solvers):
            row_data[j] = float(row[j + 1])
        data.append(row_data)

    return ParsedCsvContents(instance_names, solver_names, np.array(data))


def draw_reduced_cost_regions(p: ProcessedData):
    plt.axvspan(p.x_min, -1e-6, facecolor="green", alpha=0.15, zorder=-100)
    plt.axvspan(0.0, p.x_max, facecolor="red", alpha=0.15, zorder=-100)

    plt.vlines(
        x=0.0,
        ymin=0.05 * (p.y_max - p.y_min),
        ymax=0.95 * (p.y_max - p.y_min),
        colors="black",
        lw=PLOT_LINE_WIDTH * 1.0,
        alpha=0.5,
        label="Reduced cost threshold",
    )


def process_data(p: ParsedCsvContents, opt: argparse.Namespace):
    nrows, ncols = p.data.shape

    data = None
    raw_data = np.copy(p.data)
    x_min = opt.x_min
    x_max = opt.x_max

    if opt.raw_data:
        data = raw_data
    else:
        data = raw_data + opt.shift
        baseline = p.data.min(axis=1)
        for j in range(ncols):
            data[:, j] = data[:, j] / baseline

    # x_min, x_max are either cmdline provided or auto-computed from the data
    x_min = max(x_min, data.min()) if x_min is not None else data.min()
    x_max = min(x_max, data.max()) if x_max is not None else data.max()

    if x_min >= x_max:
        x_max = x_min + 1.0

    # Any time the statistic under analysis exceeds
    # the upper and lower limit of the x axis
    # remap its value to x_min/x_max   +/-  1e6
    for i in range(nrows):
        for j in range(ncols):
            if (
                opt.x_raw_upper_limit is not None
                and raw_data[i, j] >= opt.x_raw_upper_limit
            ):
                data[i, j] = x_max + 1e6
            if (
                opt.x_raw_lower_limit is not None
                and raw_data[i, j] <= opt.x_raw_lower_limit
            ):
                data[i, j] = x_min - 1e6

    return ProcessedData(x_min, x_max, data)


def generate_plot(p: ProcessedData, opt: argparse.Namespace, solver_names: List[str]):

    # Helper functions
    def truncate_solver_name(name: str):
        if len(name) >= PLOT_MAX_LEGEND_NAME_LEN:
            return name[0 : PLOT_MAX_LEGEND_NAME_LEN - 4] + " ..."
        return name

    def get_plt_ticks(lb, ub, cnt):
        if ub == lb:
            ub = lb + 1.0
        return np.linspace(lb, ub, cnt).round(TICKS_NUM_DECIMALS)

    # Truncate solver names if they are too long
    solver_names = solver_names.copy()
    for i in range(0, len(solver_names)):
        solver_names[i] = truncate_solver_name(solver_names[i])

    assert p.ncols == len(solver_names)

    for j in range(p.ncols):
        off = opt.style_offset
        linestyle = DASHES[(off + j) % len(DASHES)]
        marker = MARKERS[(off + j) % len(MARKERS)]
        color = COLORS[(off + j) % len(COLORS)]

        # Setup style options for plotting this solver
        options = dict(
            label=solver_names[j],
            drawstyle="steps-post",
            linewidth=PLOT_LINE_WIDTH,
            linestyle=linestyle,
            marker=marker,
            markeredgewidth=PLOT_LINE_WIDTH,
            markersize=PLOT_MARKER_SIZE,
            alpha=0.75,
        )

        # Setup colors options
        if opt.bw:
            options["markerfacecolor"] = "w"
            options["markeredgecolor"] = "k"
            options["color"] = "k"
        else:
            options["color"] = color

        # Plot solver
        if opt.logplot:
            plt.semilogx(p.x[:, j], p.y, **options)
        else:
            plt.plot(p.x[:, j], p.y, **options)

    # Add ticks and grid to the plot
    plt.axis([p.x_min, p.x_max, p.y_min, p.y_max])
    plt.xticks(get_plt_ticks(p.x_min, p.x_max, NUM_TICKS_IN_PLOT))
    plt.yticks(get_plt_ticks(p.y_min, p.y_max, NUM_TICKS_IN_PLOT))
    plt.grid(visible=True, linewidth=PLOT_GRID_LINE_WIDTH, alpha=PLOT_GRID_ALPHA)

    if opt.draw_reduced_cost_regions is not None and opt.draw_reduced_cost_regions:
        draw_reduced_cost_regions(p)

    # Customize the plot with additional effects/information
    if opt.plotlegend is not None and opt.plotlegend is True:
        plt.legend(loc="best", fontsize=LEGEND_FONT_SIZE, prop={"size": 6})
    if opt.plottitle is not None:
        plt.title(opt.plottitle)
    if opt.xlabel is not None:
        plt.xlabel(opt.xlabel)


def init():
    matplotlib.use("PDF")
    matplotlib.rcParams["figure.dpi"] = 300


def main():
    init()

    parser = CmdLineParser()
    opt = parser.parse()

    parsed_contents = read_csv(open(opt.input, "r"), opt.delimiter)

    if parsed_contents.data.shape == (0,):
        errprint(f"Cannot retrieve data from `{opt.input}` input file")
        sys.exit(-1)

    p = process_data(parsed_contents, opt)
    if p.data is None:
        errprint(f"Failed to parse data contents from `{opt.input}` input file")
        sys.exit(-1)

    generate_plot(p, opt, parsed_contents.solver_names)

    # Save the plot
    plt.savefig(opt.output)


if __name__ == "__main__":
    main()
