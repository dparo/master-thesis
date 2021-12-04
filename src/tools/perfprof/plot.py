## This script is a highly customized version of the perfprof.py script from:
##
##      Performance Profile by D. Salvagnin (2016)
##      Internal use only, not to be distributed
##
## The script was modified:
##   - Porting to python3
##   - More command line options
##   - Visual enhancements of the generated plot
##   - Generalized how data is processed. Ratios computations are in fact now optional.
#      Thus, this utility can be used to plot different kinds of already processed data.

# !/usr/bin/env python3

from argparse import ArgumentParser
from math import inf

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.transforms as mtransforms
import numpy as np

# parameters
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

matplotlib.use("PDF")
matplotlib.rcParams["figure.dpi"] = 300


def remove_duplicates_from_list(x):
    return list(dict.fromkeys(x))


def get_plt_ticks(lb, ub, cnt):
    if ub == lb:
        ub = lb + 1.0
    return np.array(
        [
            round(x, 2)
            for x in remove_duplicates_from_list(
                [lb, ub] + list(np.arange(lb, ub, step=(ub - lb) / cnt))
            )
        ]
    )


class CmdLineParser(object):
    def __init__(self):
        self.parser = ArgumentParser(
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
            "-S", "--shift", dest="shift", default=0, type=float, help="shift for data"
        )
        self.parser.add_argument(
            "--logplot",
            dest="logplot",
            action="store_true",
            default=False,
            help="Enable logscale for X",
        )
        self.parser.add_argument(
            "--x-lower-limit",
            dest="x_lower_limit",
            default=-1e99,
            type=float,
            help="Lower limit for runs",
        )
        self.parser.add_argument(
            "--x-upper-limit",
            dest="x_upper_limit",
            default=+1e99,
            type=float,
            help="Upper limit for runs",
        )
        self.parser.add_argument(
            "-P", "--plot-title", dest="plottitle", default=None, help="plot title"
        )
        self.parser.add_argument(
            "-l", "--legend", dest="plotlegend", default=True, help="plot the legend"
        )
        self.parser.add_argument(
            "-X", "--x-label", dest="xlabel", default="Time Ratio", help="x axis label"
        )
        self.parser.add_argument(
            "-B", "--bw", dest="bw", action="store_true", default=False, help="plot B/W"
        )
        self.parser.add_argument(
            "--plot-as-ratios",
            dest="plot_as_ratios",
            action="store_true",
            default=False,
            help="To plot data as ratios or not",
        )
        self.parser.add_argument(
            "--startidx",
            dest="startidx",
            default=0,
            type=int,
            help="Start index to associate with the colors",
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


def read_csv(fp, delimiter):
    """
    read a CSV file with performance profile specification
    the format is as follows:
    ncols algo1 algo2 ...
    nome_istanza tempo(algo1) tempo(algo2) ...
    ...
    """
    firstline = fp.readline().strip().split(delimiter)
    ncols = len(firstline) - 1
    cnames = firstline[1:]
    rnames = []
    rows = []
    for row in fp:
        row = row.strip().split(delimiter)
        rnames.append(row[0])
        rdata = np.empty(ncols)
        for j in range(ncols):
            rdata[j] = float(row[j + 1])
        rows.append(rdata)
    data = np.array(rows)
    return rnames, cnames, data


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


def main():
    parser = CmdLineParser()
    opt = parser.parse()
    rnames, cnames, data = read_csv(open(opt.input, "r"), opt.delimiter)

    for i in range(0, len(cnames)):
        if len(cnames[i]) >= PLOT_MAX_LEGEND_NAME_LEN:
            cnames[i] = (cnames[i])[0 : PLOT_MAX_LEGEND_NAME_LEN - 4] + " ..."

    if data.shape == (0,):
        return

    nrows, ncols = data.shape
    # add shift
    data = data + opt.shift

    eps = 1e6

    # compute ratios
    if opt.plot_as_ratios:
        minima = data.min(axis=1)
        for j in range(ncols):
            data[:, j] = data[:, j] / minima
        opt.x_lower_limit /= minima
        opt.x_upper_limit /= minima
        opt.x_min /= minima
        opt.x_max /= minima
        eps /= minima

    # Deduce minratio and maxratio if they are not specified on the command line
    if opt.x_min is None or (opt.x_min <= -1e21):
        opt.x_min = max(opt.x_lower_limit, data.min())

    if opt.x_max is None or (opt.x_max >= 1e21):
        opt.x_max = min(opt.x_upper_limit, data.max())

    # any time value exceeds limit, we push the sample out of bounds
    for i in range(nrows):
        for j in range(ncols):
            if data[i, j] >= opt.x_upper_limit:
                data[i, j] = opt.x_max + eps
            if data[i, j] <= opt.x_lower_limit:
                data[i, j] = opt.x_min - eps

    if opt.x_min == opt.x_max:
        opt.x_max = opt.x_min + 1.0

    # sort data
    data.sort(axis=0)
    # plot first
    y = np.arange(nrows, dtype=np.float64) / nrows
    for j in range(ncols):
        linestyle = DASHES[(opt.startidx + j) % len(DASHES)]
        marker = MARKERS[(opt.startidx + j) % len(MARKERS)]
        color = COLORS[(opt.startidx + j) % len(COLORS)]

        options = dict(
            label=cnames[j],
            drawstyle="steps-post",
            linewidth=PLOT_LINE_WIDTH,
            linestyle=linestyle,
            marker=marker,
            markeredgewidth=PLOT_LINE_WIDTH,
            markersize=PLOT_MARKER_SIZE,
            alpha=0.75,
        )
        if opt.bw:
            options["markerfacecolor"] = "w"
            options["markeredgecolor"] = "k"
            options["color"] = "k"
        else:
            options["color"] = color
        if opt.logplot:
            plt.semilogx(data[:, j], y, **options)
        else:
            plt.plot(data[:, j], y, **options)

    xticks = get_plt_ticks(opt.x_min, opt.x_max, 8)
    yticks = get_plt_ticks(0.0, 1.0, 8)
    plt.axis([opt.x_min, opt.x_max, 0, 1])
    plt.xticks(xticks)
    plt.yticks(yticks)

    plt.grid(visible=True, linewidth=PLOT_GRID_LINE_WIDTH, alpha=PLOT_GRID_ALPHA)

    if opt.draw_separated_regions is not None and opt.draw_separated_regions:
        draw_regions(data, ncols)

    if opt.plotlegend is not None and opt.plotlegend is True:
        plt.legend(loc="best", fontsize=6, prop={"size": 6})
    if opt.plottitle is not None:
        plt.title(opt.plottitle)
    plt.xlabel(opt.xlabel)
    plt.savefig(opt.output)


if __name__ == "__main__":
    main()
