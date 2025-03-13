Raster
======

Raster plot of incoming ZMQ data. Data expected to be floats, with the number
in a given buffer corresponding to the width in pixels of the raster plot.

Options
-------

- `s` Starts a thread to push data via ZMQ to raster plot for testing.
- `w` Set raster plot width in pixels.
- `h` Set raster plot height in pixels.
- `c` Set raster plot colormap. { "inferno" (default), "viridis", "turbo" }.

### Example

```sh
$ ./raster -s -w 320 -h 640
```

TODO
----

- Cursor crosshair with (x, y) labels and picker.
- Zoombox support.
