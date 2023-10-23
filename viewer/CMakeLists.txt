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
  content/TSTris.h
  content/TSTris.cpp
  )  

# ------------------------------------------------------------------
add_executable(hayThere
  ${SOURCES}
  )
target_link_libraries(hayThere
  PUBLIC
  hayStack
  owl_viewer
  )
target_compile_definitions(hayThere PUBLIC -DHS_VIEWER=1)

# ------------------------------------------------------------------
add_executable(hayThereOffline
  ${SOURCES}
  )
target_link_libraries(hayThereOffline
  PUBLIC
  hayStack
  )

# ------------------------------------------------------------------

if (HT_CUTEE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_COMPILE_OPTIONS_PIE "-fPIC")
  
  set(CMAKE_AUTOMOC OFF)
  add_executable(hayThereQT
    ${SOURCES}
    )
  target_link_libraries(hayThereQT
    PUBLIC
    hayStack
    #  owl_viewer
    qtOWL
    )
  target_compile_definitions(hayThereQT PUBLIC -DHS_CUTEE=1)
  set_target_properties(hayThereQT
    PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
    CUDA_USE_STATIC_CUDA_RUNTIME ON
    CUDA_ARCHITECTURES OFF
    )

  
endif()

# ------------------------------------------------------------------


# add_executable(hsViewerMPI
#   main.cpp
#   DataLoader.h
#   DataLoader.cpp
#   )
# target_compile_definitions(hsViewerMPI PUBLIC -DHS_MPI=1)
# target_link_libraries(hsViewerMPI
#   hayStack
#   )

