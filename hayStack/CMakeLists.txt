# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

find_package(anari 0.13.0 REQUIRED)

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
  target_link_libraries(hayStack PUBLIC MPI::MPI_C MPI::MPI_CXX)
else()
  target_compile_definitions(hayStack PUBLIC -DHS_FAKE_MPI=1)
endif()

target_link_libraries(hayStack PUBLIC 
  anari::anari
)

target_include_directories(hayStack PUBLIC ${PROJECT_SOURCE_DIR})


