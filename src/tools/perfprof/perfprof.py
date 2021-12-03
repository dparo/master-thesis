##
## Performance Profile by D. Salvagnin (2016)
## Internal use only, not to be distributed
##

#!/usr/bin/env python3

from __future__ import print_function

from optparse import OptionParser

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.use("PDF")

matplotlib.rcParams["figure.dpi"] = 300


def remove_duplicates_from_list(x):
    return list(dict.fromkeys(x))


def get_plt_ticks(lb, ub, cnt):
    return [round(x, 2) for x in remove_duplicates_from_list([lb, ub] + list(
        np.arange(lb, ub, step=(ub - lb) / cnt)))]


# parameters
defLW = 1.2  # default line width
defMS = 7  # default marker size
dashes = ["solid", "dotted", "dashed", "dashdot"]

markers = ["s", "^", "o", "d", "v", "<", ">", "*", "2", "+", "x", "2"]
colors = [
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
        self.parser = OptionParser(
            usage="usage: python3 perfprof.py [options] cvsfile.csv outputfile.pdf"
        )
        # default options
        self.parser.add_option(
            "-D",
            "--delimiter",
            dest="delimiter",
            default=None,
            help="delimiter for input files",
        )
        self.parser.add_option(
            "-m",
            "--minratio",
            dest="minratio",
            default=1,
            type=float,
            help="minratio for perf. profile",
        )
        self.parser.add_option(
            "-M",
            "--maxratio",
            dest="maxratio",
            default=4,
            type=float,
            help="maxratio for perf. profile",
        )
        self.parser.add_option(
            "-S", "--shift", dest="shift", default=1, type=float, help="shift for data"
        )
        self.parser.add_option(
            "--logplot",
            dest="logplot",
            action="store_true",
            default=False,
            help="log scale for x",
        )
        self.parser.add_option(
            "-L",
            "--limit",
            dest="limit",
            default=1e99,
            type=float,
            help="time limit for runs",
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
            "-0",
            "--startidx",
            dest="startidx",
            default=0,
            type=int,
            help="Start index to associate with the colors",
        )

    def addOption(self, *args, **kwargs):
        self.parser.add_option(*args, **kwargs)

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

    max_len = 64
    for i in range(0, len(cnames)):
        if len(cnames[i]) >= max_len:
            cnames[i] = (cnames[i])[0: max_len - 4] + " ..."

    if data.shape == (0,):
        return

    nrows, ncols = data.shape
    # add shift
    data = data + opt.shift

    # compute ratios
    minima = data.min(axis=1)
    ratio = data
    for j in range(ncols):
        ratio[:, j] = data[:, j] / minima
    # compute maxratio
    if opt.maxratio <= 0:
        opt.maxratio = max(1.0, ratio.max())

    # any time >= limit will count as maxratio + bigM (so that it does not show up in plots)
    for i in range(nrows):
        for j in range(ncols):
            if data[i, j] >= opt.limit:
                ratio[i, j] = opt.maxratio + 1e6

    # sort ratios
    ratio.sort(axis=0)
    # plot first
    y = np.arange(nrows, dtype=np.float64) / nrows
    for j in range(ncols):
        options = dict(
            label=cnames[j],
            drawstyle="steps-post",
            linewidth=defLW,
            linestyle=dashes[(opt.startidx + j) % len(dashes)],
            marker=markers[(opt.startidx + j) % len(markers)],
            markeredgewidth=defLW,
            markersize=defMS,
            alpha=0.75,
        )
        if opt.bw:
            options["markerfacecolor"] = "w"
            options["markeredgecolor"] = "k"
            options["color"] = "k"
        else:
            options["color"] = colors[(opt.startidx + j) % len(colors)]
        if opt.logplot:
            plt.semilogx(ratio[:, j], y, **options)
        else:
            plt.plot(ratio[:, j], y, **options)

    plt.axis([opt.minratio, opt.maxratio, 0, 1])
    plt.xticks(get_plt_ticks(opt.minratio, opt.maxratio, 8))
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
