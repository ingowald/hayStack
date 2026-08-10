# SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_library(hs-config INTERFACE)
target_include_directories(hs-config INTERFACE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/3rdParty/nanovdb
)

#  target_compile_definitions(hayStack PUBLIC HS_USE_MULTI_SCATTERING=1)
#  target_include_directories(hayStack PUBLIC ${NANOVDB_INCLUDE_DIR})
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
else()
  target_compile_definitions(hs-config INTERFACE -DHS_FAKE_MPI=1)
endif()

set(LOADER_SOURCES
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
  loader/GESTS.h
  loader/GESTS.cpp
  loader/RAWVolumeContent.h
  loader/RAWVolumeContent.cpp
  loader/TAMRContent.h
  loader/TAMRContent.cpp
  loader/UMeshContent.h
  loader/UMeshContent.cpp
)

option(HS_VTK "build VTK/VTU loader for unstructured grids with polyhedral cells" OFF)
if (HS_VTK)
  find_package(VTK COMPONENTS
    CommonCore
    CommonDataModel
    IOXML
    QUIET)
  if (NOT VTK_FOUND)
    # Try legacy component names (VTK < 9.2)
    find_package(VTK COMPONENTS
      vtkCommonCore
      vtkCommonDataModel
      vtkIOXML
      QUIET)
  endif()
  if (VTK_FOUND)
    list(APPEND SOURCES content/VTUContent.h content/VTUContent.cpp)
    target_compile_definitions(hs-config INTERFACE -DHS_VTK=1)
    target_link_libraries(hs-config INTERFACE ${VTK_LIBRARIES})
    if (VTK_VERSION VERSION_GREATER_EQUAL "8.90.0")
      vtk_module_autoinit(
        TARGETS hs-config
        MODULES ${VTK_LIBRARIES}
      )
    else()
      include(${VTK_USE_FILE})
    endif()
    message(STATUS "VTK support enabled (version ${VTK_VERSION})")
  else()
    message(WARNING "HS_VTK enabled but VTK not found - disabling VTU loader")
  endif()
endif()

option(HS_USD "build usd loader" OFF)
if (HS_USD)
  list(APPEND SOURCES loader/USD.cpp)
  
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


