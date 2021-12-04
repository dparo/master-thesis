##
## Performance Profile by D. Salvagnin (2016)
## Internal use only, not to be distributed
##

# !/usr/bin/env python3

from __future__ import print_function

from optparse import OptionParser

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# parameters
PLOT_LINE_WIDTH = 1.2
PLOT_MARKER_SIZE = 7
PLOT_MAX_LEGEND_NAME_LEN = 64

DASHES = ["solid", "dotted", "dashed", "dashdot"]
MARKERS = ["s", "^", "o", "d", "v", "<", ">", "*", "2", "+", "x", "2"]
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
    return [
        round(x, 2)
        for x in remove_duplicates_from_list(
            [lb, ub] + list(np.arange(lb, ub, step=(ub - lb) / cnt))
        )
    ]


class CmdLineParser(object):
    def __init__(self):
        self.parser = OptionParser(
            usage="Usage: python3 perfprof.py [options] cvsfile.csv outputfile.pdf"
        )
        # default options
        self.parser.add_option(
            "-D",
            "--delimiter",
            dest="delimiter",
            default=None,
            help="Delimiter for input files",
        )
        self.parser.add_option(
            "-m",
            "--x-min",
            dest="x_min",
            default=None,
            type=float,
            help="Minimum X value for perf. profile",
        )
        self.parser.add_option(
            "-M",
            "--x-max",
            dest="x_max",
            default=None,
            type=float,
            help="Minimum X value for perf. profile",
        )
        self.parser.add_option(
            "-S", "--shift", dest="shift", default=0, type=float, help="shift for data"
        )
        self.parser.add_option(
            "--logplot",
            dest="logplot",
            action="store_true",
            default=False,
            help="Enable logscale for X",
        )
        self.parser.add_option(
            "--x-lower-limit",
            dest="x_lower_limit",
            default=-1e99,
            type=float,
            help="Lower limit for runs",
        )
        self.parser.add_option(
            "--x-upper-limit",
            dest="x_upper_limit",
            default=+1e99,
            type=float,
            help="Upper limit for runs",
        )
        self.parser.add_option(
            "-P", "--plot-title", dest="plottitle", default=None, help="plot title"
        )
        self.parser.add_option(
            "-l", "--legend", dest="plotlegend", default=True, help="plot the legend"
        )
        self.parser.add_option(
            "-X", "--x-label", dest="xlabel", default="Time Ratio", help="x axis label"
        )
        self.parser.add_option(
            "-B", "--bw", dest="bw", action="store_true", default=False, help="plot B/W"
        )
        self.parser.add_option(
            "--plot-as-ratios",
            dest="plot_as_ratios",
            action="store_true",
            default=False,
            help="To plot data as ratios or not",
        )
        self.parser.add_option(
            "--startidx",
            dest="startidx",
            default=0,
            type=int,
            help="Start index to associate with the colors",
        )

    def parseArgs(self):
        (options, args) = self.parser.parse_args()
        options.input = args[0]
        options.output = args[1]
        return options


def readTable(fp, delimiter):
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
    return (rnames, cnames, data)


def main():
    parser = CmdLineParser()
    opt = parser.parseArgs()
    # read data
    rnames, cnames, data = readTable(open(opt.input, "r"), opt.delimiter)

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
    if opt.x_min is None or opt.x_min <= -1e21:
        opt.x_min = max(opt.x_lower_limit, data.min())

    if opt.x_max is None or opt.x_max >= 1e21:
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
        options = dict(
            label=cnames[j],
            drawstyle="steps-post",
            linewidth=PLOT_LINE_WIDTH,
            linestyle=DASHES[(opt.startidx + j) % len(DASHES)],
            marker=MARKERS[(opt.startidx + j) % len(MARKERS)],
            markeredgewidth=PLOT_LINE_WIDTH,
            markersize=PLOT_MARKER_SIZE,
            alpha=0.75,
        )
        if opt.bw:
            options["markerfacecolor"] = "w"
            options["markeredgecolor"] = "k"
            options["color"] = "k"
        else:
            options["color"] = COLORS[(opt.startidx + j) % len(COLORS)]
        if opt.logplot:
            plt.semilogx(data[:, j], y, **options)
        else:
            plt.plot(data[:, j], y, **options)

    plt.axis([opt.x_min, opt.x_max, 0, 1])
    plt.xticks(get_plt_ticks(opt.x_min, opt.x_max, 8))
    plt.yticks(get_plt_ticks(0.0, 1.0, 8))
    plt.grid(visible=True, linewidth=0.1, alpha=0.5)
    if opt.plotlegend is not None and opt.plotlegend is True:
        plt.legend(loc="best", fontsize=6, prop={"size": 6})
    if opt.plottitle is not None:
        plt.title(opt.plottitle)
    plt.xlabel(opt.xlabel)
    plt.savefig(opt.output)


if __name__ == "__main__":
    main()
