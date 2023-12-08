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

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

set(SOURCES
  main.cpp
  DataLoader.h
  DataLoader.cpp
  content/MiniContent.h
  content/OBJContent.h
  content/OBJContent.cpp
  content/SpheresFromFile.h
  content/SpheresFromFile.cpp
  content/CylindersFromFile.h
  content/CylindersFromFile.cpp
  content/TSTris.h
  content/TSTris.cpp
  content/RAWVolumeContent.h
  content/RAWVolumeContent.cpp
  )  

# ------------------------------------------------------------------
add_executable(hsViewer
  ${SOURCES}
  )
target_link_libraries(hsViewer
  PUBLIC
  hayStack
  owl_viewer
  )
target_compile_definitions(hsViewer PUBLIC -DHS_VIEWER=1)

# ------------------------------------------------------------------
add_executable(hsOffline
  ${SOURCES}
  )
target_link_libraries(hsOffline
  PUBLIC
  hayStack
  )

# ------------------------------------------------------------------

if (HT_CUTEE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_COMPILE_OPTIONS_PIE "-fPIC")
  
  set(CMAKE_AUTOMOC OFF)
  add_executable(hsViewerQT
    ${SOURCES}
    )
  target_link_libraries(hsViewerQT
    PUBLIC
    hayStack
    qtOWL
    )
  target_compile_definitions(hsViewerQT PUBLIC -DHS_CUTEE=1)
  set_target_properties(hsViewerQT
    PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
    CUDA_USE_STATIC_CUDA_RUNTIME ON
    CUDA_ARCHITECTURES OFF
    )
  
endif()

