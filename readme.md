Plots
=====

Plots of incoming ZMQ data. Data expected to be floats, with the number in a
given buffer corresponding to the width in pixels of the waterfall plot.

Options
-------

- `s` Starts a thread to push data via ZMQ to waterfall plot for testing.
- `c` Set waterfall plot colormap. { "inferno" (default), "viridis", "turbo", "grey" }.

### Example

```sh
$ ./waterfall -s -c viridis
```

TODO
----

- Zoombox support.
