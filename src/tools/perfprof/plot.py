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

import sys
from typing import List

import argparse
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.transforms as mtransforms
import numpy as np


def errprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def list_keep_uniqs(x):
    return list(dict.fromkeys(x))


# Constants
PLOT_LINE_WIDTH = 1.2
PLOT_MARKER_SIZE = 7
PLOT_MAX_LEGEND_NAME_LEN = 64
PLOT_GRID_LINE_WIDTH = 0.1
PLOT_GRID_ALPHA = 0.5

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
            "--draw-separated-regions",
            dest="draw_separated_regions",
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


class ParsedCsvContents:
    solver_names: List[str]
    instance_names: List[str]
    data: np.ndarray

    def __init__(
        self, instance_names: List[str], solver_names: List[str], data: np.ndarray
    ):
        self.instance_names = instance_names
        self.solver_names = solver_names
        self.data = data


def read_csv(fp, delimiter):
    """
    Read a CSV file with performance profile specification.

    The format is as follows:

        ncols          algo1           algo2 ...
        nome_istanza   tempo(algo1)    tempo(algo2) ...
    """
    firstline = fp.readline().strip().split(delimiter)
    ncols = len(firstline) - 1
    solver_names = firstline[1:]
    instance_names = []
    rows = []
    for row in fp:
        row = row.strip().split(delimiter)
        instance_names.append(row[0])
        rdata = np.empty(ncols)
        for j in range(ncols):
            rdata[j] = float(row[j + 1])
        rows.append(rdata)

    return ParsedCsvContents(instance_names, solver_names, np.array(rows))


def draw_regions(data, ncols):
    x = [0.0]
    for j in range(ncols):
        x.extend(data[:, j])

    x = sorted(x)
    x = np.array(x)

    ax = plt.gca()
    trans = mtransforms.blended_transform_factory(ax.transData, ax.transAxes)

    zero = 0.0
    x_gte_0 = x >= zero
    x_lt_0 = x < zero

    if np.any(x_gte_0):
        plt.fill_between(
            x,
            -1.0,
            2.0,
            where=x_gte_0,
            facecolor="red",
            alpha=0.2,
            transform=trans,
        )
    if np.any(x_lt_0):
        plt.fill_between(
            x,
            -1.0,
            2.0,
            where=x_lt_0,
            facecolor="green",
            alpha=0.2,
            transform=trans,
        )

    if False:
        plt.axvline(x=0.0, linewidth=12.0 * PLOT_GRID_LINE_WIDTH, alpha=0.8, color="r")


class ProcessedData:
    x_min: float
    x_max: float
    data: np.ndarray
    nrows: int
    ncols: int

    def __init__(self, x_min: float, x_max: float, data: np.ndarray):
        self.x_min = x_min
        self.x_max = x_max
        self.data = data
        self.nrows, self.ncols = data.shape


def process_data(p: ParsedCsvContents, opt: argparse.Namespace):
    nrows, ncols = p.data.shape

    data = None
    raw_data = np.copy(p.data)
    x_min = opt.x_min
    x_max = opt.x_max

    if opt.raw_data:
        data = np.copy(raw_data)
    else:
        data = p.data + opt.shift
        baseline = p.data.min(axis=1)
        for j in range(ncols):
            data[:, j] = data[:, j] / baseline

    # x_min, x_max are either cmdline provided or auto-computed from the data
    x_min = x_min if x_min is not None else data.min()
    x_max = x_max if x_max is not None else data.max()

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
        return np.array(
            [
                round(x, 3)
                for x in list_keep_uniqs(
                    [lb, ub] + list(np.arange(lb, ub, step=(ub - lb) / cnt))
                )
            ]
        )

    nrows, ncols = p.data.shape

    # Compute ideal data to plot
    x = np.copy(p.data).sort(axis=0)
    # Evenly spaced values within the Y axis.
    y = np.arange(nrows, dtype=np.float64) / nrows

    # Truncate solver names if they are too long
    solver_names = solver_names.copy()
    for i in range(0, len(solver_names)):
        solver_names[i] = truncate_solver_name(solver_names[i])

    for j in range(ncols):
        off = opt.style_offset
        linestyle = DASHES[(off + j) % len(DASHES)]
        marker = MARKERS[(off + j) % len(MARKERS)]
        color = COLORS[(off + j) % len(COLORS)]

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

        # Plot
        if opt.logplot:
            plt.semilogx(x[:, j], y, **options)
        else:
            plt.plot(x[:, j], y, **options)

    xticks = get_plt_ticks(p.x_min, p.x_max, 8)
    yticks = get_plt_ticks(0.0, 1.0, 8)
    plt.axis([p.x_min, p.x_max, 0, 1])
    plt.xticks(xticks)
    plt.yticks(yticks)

    plt.grid(visible=True, linewidth=PLOT_GRID_LINE_WIDTH, alpha=PLOT_GRID_ALPHA)

    if opt.draw_separated_regions is not None and opt.draw_separated_regions:
        draw_regions(x, ncols)

    # Customize the plot with additional details
    if opt.plotlegend is not None and opt.plotlegend is True:
        plt.legend(loc="best", fontsize=6, prop={"size": 6})
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
