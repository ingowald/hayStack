# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

cmake_policy(SET CMP0048 NEW)

cmake_minimum_required(VERSION 3.16)
project(HayStack VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CUDA_USE_STATIC_CUDA_RUNTIME ON)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.18)
  cmake_policy(SET CMP0104 NEW)
endif()

option(HS_MPI    "Enable MPI support in haystack" OFF)
option(HS_ANARI  "Enable ANARI Backend" ON)

find_package(barney)
if (barney_FOUND)
  option(HS_BARNEY "Enable Barney Native Backend" ON)
else()
  set(HS_BARNEY OFF)
endif()

  
if (APPLE)
  set(HS_HAVE_CUDA OFF)
else()
  option(HS_HAVE_CUDA "Have CUDA?" ON)
endif()

if (WIN32)
  option(HS_CUTEE "Use QT Viewer and XFEditor" OFF)
else()
  option(HS_CUTEE "Use QT Viewer and XFEditor" ON)
endif()
option(HS_USE_IMGUI "Use Imgui in Non-QT viewer?" OFF)
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
add_subdirectory(3rdParty/tsd_tinyexr      EXCLUDE_FROM_ALL)
add_subdirectory(viewer)
add_subdirectory(tools)

