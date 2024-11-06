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

set(SOURCES
  main.cpp
  DataLoader.h
  DataLoader.cpp
  content/MiniContent.h
  content/OBJContent.h
  content/OBJContent.cpp
  content/SpheresFromFile.h
  content/SpheresFromFile.cpp
  content/MaterialsTest.h
  content/MaterialsTest.cpp
  content/BoxesFromFile.h
  content/BoxesFromFile.cpp
  content/CylindersFromFile.h
  content/CylindersFromFile.cpp
  content/Capsules.h
  content/Capsules.cpp
  content/TSTris.h
  content/TSTris.cpp
  content/RAWVolumeContent.h
  content/RAWVolumeContent.cpp
  content/UMeshContent.h
  content/UMeshContent.cpp
  )  

set(TFE_SOURCES
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_widgets.cpp
  imgui_tfe/imgui_impl_glfw_gl3.h
  imgui_tfe/imgui_impl_glfw_gl3.cpp
  imgui_tfe/ColorMaps.h
  imgui_tfe/ColorMaps.cpp
  imgui_tfe/RGBAEditor.h
  imgui_tfe/RGBAEditor.cpp
  imgui_tfe/TFEditor.h
  imgui_tfe/TFEditor.cpp
)

if(WIN32)
  set(GLAD_SOURCES
    external/glad/glad.cpp
    external/glad/glad.h
    external/glad/khrplatform.h
  )
endif()

if (HS_VIEWER)
# ------------------------------------------------------------------
add_executable(hsViewer
  ${SOURCES}
  ${TFE_SOURCES}
  ${GLAD_SOURCES}
  )
target_link_libraries(hsViewer
  PUBLIC
  hayStack
  owl_viewer
  )
target_compile_definitions(hsViewer PUBLIC -DHS_VIEWER=1)
if (WIN32)
  target_include_directories(hsViewer SYSTEM PRIVATE external/imgui external/glad)
else()
  target_include_directories(hsViewer SYSTEM PRIVATE external/imgui)
endif()
if (HS_USE_IMGUI)
  target_compile_definitions(hsViewer PUBLIC -DHS_HAVE_IMGUI=1)
endif()
endif()

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


# ------------------------------------------------------------------
add_executable(hsDLAFtoPCR
  content/dlafToPCR.cpp
  )
target_link_libraries(hsDLAFtoPCR
  PUBLIC
  hayStack
  )
