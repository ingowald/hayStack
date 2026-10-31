# SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_library(hs-config INTERFACE)
target_include_directories(hs-config INTERFACE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/3rdParty/nanovdb/include
)

if (HS_USE_MULTI_SCATTERING)
  target_compile_definitions(hs-config INTERFACE HS_USE_MULTI_SCATTERING=1)
else()
  target_compile_definitions(hs-config INTERFACE HS_USE_MULTI_SCATTERING=0)
endif()
# link to miniscene and umesh - we need those in either config
target_link_libraries(hs-config
  INTERFACE
  miniScene
  umesh
  tinyAMR
)
if (HS_MPI)
  find_package(MPI REQUIRED)
  target_compile_definitions(hs-config INTERFACE -DHS_MPI=1)
  target_link_libraries(hs-config INTERFACE MPI::MPI_C MPI::MPI_CXX)
endif()

set(LOADER_SOURCES
  loader/ColorMapper.h
  loader/ColorMapper.cpp
  loader/DataLoader.h
  loader/DataLoader.cpp
  loader/MiniContent.h
  loader/IsoDump.h
  loader/IsoDump.cpp
  loader/OBJContent.h
  loader/OBJContent.cpp
  loader/SpheresFromFile.h
  loader/SpheresFromFile.cpp
  loader/MaterialsTest.h
  loader/MaterialsTest.cpp
  loader/BoxesFromFile.h
  loader/BoxesFromFile.cpp
  loader/CylindersFromFile.h
  loader/CylindersFromFile.cpp
  loader/Capsules.h
  loader/Capsules.cpp
  loader/TSTris.h
  loader/TSTris.cpp
  loader/DGEF.h
  loader/DGEFContent.h
  loader/DGEFContent.cpp
  loader/TriangleMesh.h
  loader/TriangleMesh.cpp
  loader/VTPContent.h
  loader/VTPContent.cpp
  loader/VTUContent.h
  loader/VTUContent.cpp
  loader/GESTS.h
  loader/GESTS.cpp
  loader/RAWVolumeContent.h
  loader/RAWVolumeContent.cpp
  loader/TAMRContent.h
  loader/TAMRContent.cpp
  loader/UMeshContent.h
  loader/UMeshContent.cpp
)

option(HS_DISABLE_VTK "explicitly DISable VTK based reader(s)" OFF)

if (NOT HS_DISABLE_VTK)
  find_package(VTK COMPONENTS
    CommonCore
    CommonDataModel
    IOLegacy
    IOXML
    QUIET)
  if (VTK_FOUND)
    message(STATUS "#hs:VTK found all right, trying to use it.")
    message(STATUS "#hs:If this leads to any problems, try `-DHS_DISABLE_VTK=ON`")
  else()
    message(WARNING "#hs:VTK support enabled in cmake, but VTK could not be found.")
  endif()
  if (VTK_VERSION VERSION_GREATER_EQUAL "8.90.0")
    # all good
  else()
    message(WARNING "#hs:VTK found, but too old. disabling.")
    set(VTK_FOUND OFF)
  endif()
else()
  message(STATUS "#hs:VTK support explicitly disabled by user request")
  set(VTK_FOUND OFF)
endif()

if (VTK_FOUND)
  target_compile_definitions(hs-config INTERFACE -DHS_VTK=1)
  target_link_libraries(hs-config INTERFACE# ${VTK_LIBRARIES}
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::IOXML
    VTK::IOLegacy
  )
  vtk_module_autoinit(
    TARGETS hs-config
    MODULES ${VTK_LIBRARIES}
  )
endif()

option(HS_USD "build usd loader" OFF)
if (HS_USD)
  list(APPEND LOADER_SOURCES loader/USD.cpp)
  find_package(OpenGL)
  find_package(X11)
  find_package(pxr REQUIRED usd usdGeom tf gf usdShade usdLux)
  target_link_libraries(hs-config INTERFACE ${PXR_LIBRARIES})
  target_link_libraries(hs-config INTERFACE ext_tinyexr_impl)
  target_compile_definitions(hs-config INTERFACE -DHS_USD=1)
endif()


add_library(hayStackDataLoader
  ${LOADER_SOURCES}
)
target_link_libraries(hayStackDataLoader PUBLIC hs-config)
  
add_library(hayStack
  MPIWrappers.h
  MPIWrappers.cpp
  
  HayStack.h
  HayStack.cpp
  
  # one logical parition of the data
  OnePartition.h
  OnePartition.cpp
  # whatever partition(s) the local rank/process owns
  LocalPartitions.h
  LocalPartitions.cpp
  
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
  NanoVDBVolume.h
  VolumeScatter.h
  ColorMap.h
  ColorMap.cpp

  TransferFunction.h
  TransferFunction.cpp
  TriangleMesh.h
  TriangleMesh.cpp
)

target_link_libraries(hayStack PUBLIC hs-config)


#target_link_libraries(hayStack PUBLIC)

#target_include_directories(hayStack PUBLIC ${PROJECT_SOURCE_DIR})


