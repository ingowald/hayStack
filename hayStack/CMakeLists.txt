# ======================================================================== #
# Copyright 2023-2024 Ingo Wald                                            #
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

option(HANARI "Use ANARI device?" OFF)

if (HANARI)
  find_package(anari 0.8.0 REQUIRED #COMPONENTS viewer
    )
  find_package(MPI REQUIRED)
endif()


add_library(hayStack
  MPIWrappers.h
  MPIWrappers.cpp
  
  HayStack.h
  HayStack.cpp
  DataGroup.h
  DataGroup.cpp
  Cylinders.h
  Cylinders.cpp
  Spheres.h
  Spheres.cpp
  StructuredVolume.h
  StructuredVolume.cpp

  Renderer.h
  MPIRenderer.h
  MPIRenderer.cpp
  HayMaker.h
  HayMaker.cpp
  TransferFunction.h
  TransferFunction.cpp
  )

if (WIN32)
target_link_libraries(hayStack
  PUBLIC
  owl
  miniScene
  umesh
  barney
#  mpiRenderInf
  )
else()
target_link_libraries(hayStack
  PUBLIC
  owl
  miniScene
  umesh
  barney_mpi
#  mpiRenderInf
  )
endif()

target_include_directories(hayStack PUBLIC ${PROJECT_SOURCE_DIR})

if (WIN32)
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
endif()

if (HANARI)
  target_compile_definitions(hayStack
    PUBLIC
    -DHANARI=1
    )
  target_link_libraries(hayStack
    PUBLIC
#    anari::anari_viewer
    anari::anari
    MPI::MPI_CXX
    )
endif()
