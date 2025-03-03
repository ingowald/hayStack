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

find_package(anari 0.8.0)

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
)

# link to miniscene and umesh - we need those in either config
target_link_libraries(hayStack
  PUBLIC
  miniScene
  umesh
)
target_include_directories(hayStack PUBLIC
  ${PROJECT_SOURCE_DIR}
  )

# link to respective version(s) of barney/barney_mpi,banari
if (NO_BARNEY)
  # explicitly don't link to barney - we'll get everything through
  # anari.
  target_compile_definitions(hayStack PUBLIC -DNO_BARNEY=1)
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
elseif (WIN32)
  # windows: we don't support mpi, anyway, always link to barney
  target_link_libraries(hayStack PUBLIC barney::barney)
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
elseif (TARGET barney::barney_mpi)
  message("we HAVE MPI in barney")
  # we're NOT on windows, so we _can_ use mpi mode. check if we have
  # it, and if so, use it.
  find_package(MPI REQUIRED)
  target_link_libraries(hayStack PUBLIC barney::barney_mpi MPI::MPI_CXX)
else()
  # we're on linux, but the barney we foudn doesn't feature any mpi
  # support - use non-mpi barney.
  target_link_libraries(hayStack PUBLIC barney::barney)
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
endif()

if (TARGET anari::anari)
  target_compile_definitions(hayStack
    PUBLIC
    -DHANARI=1
  )
  target_link_libraries(hayStack
    PUBLIC
    anari::anari
  )
  #  if (MPI_FOUND)
  #    target_link_libraries(hayStack
  #      PUBLIC
  #      MPI::MPI_CXX
  #    )
  #  endif()
endif()

target_link_libraries(hayStack PUBLIC CUDA::cudart)
