# PHTree Demo

A slightly more interesting demo of the [phtree-c](https://github.com/DDexxeDD/phtree-c) implementation.

An explanation of the PH-Tree can be found [here](https://tzaeschke.github.io/phtree-site/).


## About

This is a demonstration of a multimap PH-Tree.  The entries in the tree represent 64x64 pixel cells, which store references to 2D points.  Points are randomly generated, and placed in the proper cell on creation.

The PH-Tree implementation is in the `external/phtree` folder.


## Building

You will need [meson](https://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) to build this project.

You will also need [Raylib](https://github.com/raysan5/raylib).  Build Raylib and place `libraylib.a` in `external/raylib`.

**Build**

```
meson setup build
meson compile -C build
```

This will create the '`build`' directory.
The executable `phtree` will be in the `build` directory.

This was only tested on linux, so no idea if it works properly on anything else.


## Running

```
./build/phtree
```

Click or Click and drag to select points.  Points inside of the selection box will turn red.  Cells which the selection box queried will be lighter colored.  Cells with no points in them do not exist and will remain black spaces inside of the selection box.

Press space to clear the selection box.


## License

The code is released under MIT license.

Licenses of external code are provided in their respective folders.
