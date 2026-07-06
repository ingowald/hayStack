# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0104 NEW)

cmake_minimum_required(VERSION 3.24)

set(CMAKE_BUILD_TYPE_INIT "Release")
project(HayStack VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HS_MPI    "Enable MPI support in haystack" OFF)

if (WIN32)
  option(HS_CUTEE "Use QT Viewer and XFEditor" OFF)
else()
  option(HS_CUTEE "Use QT Viewer and XFEditor" ON)
endif()
option(HS_USE_IMGUI "Use Imgui in Non-QT viewer?" OFF)
option(HS_USE_MULTI_SCATTERING "NanoVDB volumes for Barney multi-scattering" OFF)
option(HS_VIEWER "Build hsViewer" ON)

if (NOT (TARGET miniScene))
  add_subdirectory(submodules/miniScene    EXCLUDE_FROM_ALL)
endif()

if (NOT (TARGET tinyAMR))
  add_subdirectory(submodules/tinyAMR      EXCLUDE_FROM_ALL)
endif()

if (NOT (TARGET umesh))
  add_subdirectory(submodules/umesh        EXCLUDE_FROM_ALL)
endif()

if (HS_CUTEE)
  add_subdirectory(submodules/cuteeOWL     EXCLUDE_FROM_ALL)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

add_subdirectory(hayStack)
add_subdirectory(hayMaker)
add_subdirectory(3rdParty/tsd_tinyexr      EXCLUDE_FROM_ALL)
add_subdirectory(viewer)
add_subdirectory(tools)

