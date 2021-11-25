# perfprof.py:

Performance Profile by D. Salvagnin (2016)
Internal use only, not to be distributed

Usage:

```sh
python ../perfprof.py -D , -T 3600 -S 2 -M 20 lagr.csv pp.pdf -P "all instances, shift 2 sec.s"
```


Parameters:

```python
self.parser.add_option("-D", "--delimiter", dest="delimiter", default=None, help="delimiter for input files")
self.parser.add_option("-M", "--maxratio", dest="maxratio", default=4, type=int, help="maxratio for perf. profile")
self.parser.add_option("-S", "--shift", dest="shift", default=0, type=float, help="shift for data")
self.parser.add_option("-L", "--logplot", dest="logplot", action="store_true", default=False, help="log scale for x")
self.parser.add_option("-T", "--timelimit", dest="timelimit", default=1e99, type=float, help="time limit for runs")
self.parser.add_option("-P", "--plot-title", dest="plottitle", default=None, help="plot title")
self.parser.add_option("-X", "--x-label", dest="xlabel", default='Time Ratio', help="x axis label")
self.parser.add_option("-B", "--bw", dest="bw", action="store_true", default=False, help="plot B/W")
```

## References

- <a id="dolan2002benchmarking">[1]</a>
Dolan, E. D., & Moré, J. J. (2002). Benchmarking optimization software with performance profiles. Mathematical programming, 91(2), 201-213.
