# ======================================================================== #
# Copyright 2023-2023 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

cmake_policy(SET CMP0048 NEW)

cmake_minimum_required(VERSION 3.16)
project(HayStack VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Enable debuging of kernels
#set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -g -G")

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.18)
  cmake_policy(SET CMP0104 NEW)
endif()

if (WIN32)
  option(HT_CUTEE "Use QT Viewer and XFEditor" OFF)
else()
  option(HT_CUTEE "Use QT Viewer and XFEditor" ON)
endif()
option(HS_USE_IMGUI "Use Imgui in Non-QT viewer?" OFF)
option(HS_VIEWER "Build hsViewer" ON)

# ------------------------------------------------------------------
# owl - do this first so miniScene can pick it up
# ------------------------------------------------------------------
option(NO_BARNEY "do not build barney?" OFF)
if (NOT (TARGET miniScene))
  add_subdirectory(submodules/miniScene    EXCLUDE_FROM_ALL)
endif()
if (NOT NO_BARNEY)
  enable_language(CUDA)
  add_subdirectory(../barney builddir_barney EXCLUDE_FROM_ALL)
endif()
if (NOT (TARGET umesh))
  add_subdirectory(submodules/umesh        EXCLUDE_FROM_ALL)
endif()
if (HT_CUTEE)
  add_subdirectory(submodules/cuteeOWL     EXCLUDE_FROM_ALL)
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

add_subdirectory(hayStack)
add_subdirectory(viewer)
add_subdirectory(tools)

