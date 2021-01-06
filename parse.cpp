#include <string>
#include <iostream>

extern int verbose;

static void usage() {
    std::cerr << "raster_mosaicker by S. Vakeva (2020)\n\nUsage: raster_mosaicker.exe -m [MASK.TIF] -i [TILE_INPUT_PATH] -l [TILE_LIST.TXT] -o [DST_FILENAME.TIF] [-v]\n";
    std::cerr << "\nMosaics the tiles located at TILE_INPUT_PATH based on the tile filenames given in TILE_LIST.\n";
    std::cerr << "\nOnly mosaics areas that contain a valid value in MASK.TIF raster.\n";
}

int parseargs(int argc, char* argv[], std::string & inputPath, std::string & seamask, std::string & ndviListFile, std::string & pszDstFilename) {
    if (argc <= 1) {
        usage();
        throw "No arguments provided!";
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            usage();
            return 0;
        }
        else if (arg == "-m") {
            if (i + 1 < argc) {
                seamask = argv[++i];
            }
            else {
                throw "-m requires one argument";
            }
        }
        else if (arg == "-i") {
            if (i + 1 < argc) {
                inputPath = argv[++i];
            }
            else {
                throw "-i requires one argument";
            }
        }
        else if (arg == "-l") {
            if (i + 1 < argc) {
                ndviListFile = argv[++i];
            }
            else {
                throw "-l requires one argument";
            }
        }
        else if (arg == "-o") {
            if (i + 1 < argc) {
                pszDstFilename = argv[++i];
            }
            else {
                throw "-o requires one argument";
            }
        }
        else if (arg == "-v") {
            verbose = 1;
        }
    }
    
    return 1;
}