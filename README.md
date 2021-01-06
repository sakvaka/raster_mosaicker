# raster_mosaicker
A tool for mosaicking together a set of raster tiles so that the output is only provided in areas where there is data in a user-provided validity mask.

Most effective when the validity mask is a tiled sparse GeoTIFF, as the code goes through the validity mask block by block and does not attempt mosaicking at the empty blocks in the validity mask.

## Usage

`raster_mosaicker.exe -m [MASK.TIF] -i [TILE_INPUT_PATH] -l [TILE_LIST.TXT] -o [DST_FILENAME.TIF] [-v]`

Mosaics the tiles located at `TILE_INPUT_PATH` based on the tile filepaths (relative to `TILE_INPUT_PATH`) given in `TILE_LIST` (whose path is also relative to `TILE_INPUT_PATH`). Only mosaics areas that contain a valid value in `MASK.TIF` raster.

## Prerequisites

Tested on VS 2019, GDAL 3.3.0dev (2020-12-15 nightly build), and PROJ 8.0, but probably works on older versions, too. The dependencies can be installed with the OSGeow4 installer; remember to change the paths if you are using CMake to build the project.

## Troubleshooting

If you get the error `ERROR 1: PROJ: proj_create_from_database: Cannot find proj.db`, you have to set the environment `PROJ_LIB` to point to the build output directory. That is where the `proj.db` database file is copied during the build.