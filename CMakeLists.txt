cmake_minimum_required(VERSION 3.14)

# project name
project(raster_mosaicker)

# add executable
add_executable(${PROJECT_NAME} "raster_mosaicker.cpp" parse.cpp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)

add_library(gdal SHARED IMPORTED)
set_target_properties(gdal PROPERTIES
    IMPORTED_LOCATION "D:/osgeo_develop/apps/gdal-dev/bin/gdal303.dll"
    IMPORTED_IMPLIB "D:/osgeo_develop/apps/gdal-dev/lib/gdal_i.lib"
    INTERFACE_INCLUDE_DIRECTORIES "D:/osgeo_develop/apps/gdal-dev/include"
)

target_link_libraries(${PROJECT_NAME} gdal)

set(ADDITIONAL_DLL_PATH "D:/osgeo_develop/apps/gdal-dev/bin" "D:/osgeo_develop/apps/proj-dev/bin" "D:/osgeo_develop/bin")
install(CODE "
     include(BundleUtilities)
     fixup_bundle(\"$<TARGET_FILE:${PROJECT_NAME}>\" \"\" \"${ADDITIONAL_DLL_PATH}\")
     configure_file(\"D:/osgeo_develop/apps/proj-dev/share/proj/proj.db\" \"$<TARGET_FILE_DIR:${PROJECT_NAME}>\" COPYONLY)
  "
  COMPONENT Runtime)

