# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0104 NEW)

cmake_minimum_required(VERSION 3.24)

set(CMAKE_BUILD_TYPE_INIT "Release")
project(HayStack VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

option(HS_MPI    "Enable MPI support in haystack" OFF)

if (NOT (TARGET miniScene))
  add_subdirectory(submodules/miniScene    EXCLUDE_FROM_ALL)
endif()

if (NOT (TARGET tinyAMR))
  add_subdirectory(submodules/tinyAMR      EXCLUDE_FROM_ALL)
endif()
add_subdirectory(3rdParty/tsd_tinyexr build-tinyexr)

if (NOT (TARGET umesh))
  add_subdirectory(submodules/umesh        EXCLUDE_FROM_ALL)
endif()

# ------------------------------------------------------------------
# include stb - also need that in other places, probably move this to
# haystack
# ------------------------------------------------------------------
#if (NOT(TARGET stb_image))
  add_subdirectory(3rdParty/stb_image)
#endif()


#if (HS_VIEWER)
#  add_subdirectory(submodules/owl   EXCLUDE_FROM_ALL)
#  add_subdirectory(submodules/glfwViewer   EXCLUDE_FROM_ALL)
#endif()

add_subdirectory(hayStack)
#if (HS_VIEWER)
add_subdirectory(viewer)
#endif()
add_subdirectory(tools)

