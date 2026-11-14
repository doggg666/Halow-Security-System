cmake_minimum_required(VERSION 3.16)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.defaults.halow" CACHE STRING "Fieldview SDK defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(halow_camera_hmi)
