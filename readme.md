Raster
======

Raster plot of incoming ZMQ data. Data expected to be floats, with the number
in a given buffer corresponding to the width in pixels of the raster plot.

Options
-------

- `s` Starts a thread to push data via ZMQ to raster plot for testing.
- `c` Set raster plot colormap. { "inferno" (default), "viridis", "turbo", "grey" }.

### Example

```sh
$ ./raster -s -c viridis
```

TODO
----

- Zoombox support.
