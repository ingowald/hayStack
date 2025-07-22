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

add_library(hayStack
  MPIWrappers.h
  MPIWrappers.cpp
  
  HayStack.h

  LocalModel.h
  LocalModel.cpp
  DataRank.h
  DataRank.cpp
  Cylinders.h
  Cylinders.cpp
  Capsules.h
  Capsules.cpp
  Spheres.h
  Spheres.cpp
  StructuredVolume.h
  StructuredVolume.cpp
  TAMRVolume.h
  TAMRVolume.cpp
  ColorMap.h
  ColorMap.cpp

  Renderer.h
  MPIRenderer.h
  MPIRenderer.cpp
  HayMaker.h
  HayMaker.cpp
  BarneyBackend.h
  BarneyBackend.cpp
  AnariBackend.h
  AnariBackend.cpp
  TransferFunction.h
  TransferFunction.cpp
  TriangleMesh.h
  TriangleMesh.cpp
)

# link to miniscene and umesh - we need those in either config
target_link_libraries(hayStack
  PUBLIC
  miniScene
  umesh
  tinyAMR
)
target_include_directories(hayStack PUBLIC
  ${PROJECT_SOURCE_DIR}
)

if (HS_MPI)
  find_package(MPI REQUIRED)
  target_compile_definitions(hayStack PUBLIC -DHS_MPI=1)
  target_link_libraries(hayStack PUBLIC MPI::MPI_CXX)
else()
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
endif()

# link to respective version(s) of barney/barney_mpi
if (HS_BARNEY)
  find_package(barney REQUIRED)
  if (HS_MPI)
    if (NOT BARNEY_FEATURE_MPI)
      message(FATAL_ERROR "HS_MPI is ON, but the barney we found doesn't offer MPI support")
    endif()
    message("linking to barney_mpi")
    target_link_libraries(hayStack PUBLIC barney::barney_mpi -DBARNEY_MPI=ON)
  else()
    message("linking to barney")
    target_link_libraries(hayStack PUBLIC barney::barney)
  endif()
else()
  # explicitly don't link to barney - we'll get everything through
  # anari.
  target_compile_definitions(hayStack PUBLIC -DNO_BARNEY=1)
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
endif()


if (HS_ANARI)
  find_package(anari 0.14.0 REQUIRED)
  target_compile_definitions(hayStack
    PUBLIC
    -DHANARI=1
  )
  target_link_libraries(hayStack
    PUBLIC
    anari::anari
  )
endif()

if (HS_HAVE_CUDA)
  target_link_libraries(hayStack PUBLIC CUDA::cudart)
endif()
