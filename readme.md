Plots
=====

Plots of incoming data on stdin. Data expected to be floats. All plots have an
alternate screen indicating controls and tag locations, this screen can be
toggled to via the spacebar.

### Plot

Basic time series line plot. No options.

### Raster1d

No options. Same as the basic plot but now each trace has some persistance so
you have more time to observe any outlier behavior similar to a max trace.

### Waterfall

2D raster plot of incoming data. Each line corresponds to a single pixel row.
The current time of the raster is indicated by the falling bar.

#### Options

- `c` Set waterfall plot colormap. { "inferno" (default), "viridis", "turbo", "grey" }.

Examples
========

```sh
$ scripts/gen_noise.py | ./plot
$ scripts/gen_noise.py | ./raster1d
$ scripts/gen_noise.py 512 | ./waterfall -f 512 -c viridis
```

TODO
====

- Zoombox support.
