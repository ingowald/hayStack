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

# ------------------------------------------------------------------
if(HS_VIEWER)
  add_executable(hsViewer
    ${SOURCES}
    ${TFE_SOURCES}
    )
  target_link_libraries(hsViewer
    PUBLIC
    hayStack
    owl_viewer
    )
  target_include_directories(hsViewer SYSTEM PRIVATE external/imgui)
  target_compile_definitions(hsViewer PUBLIC -DHS_VIEWER=1)

  if(OpenMP_CXX_FOUND)
      target_link_libraries(hsViewer PUBLIC OpenMP::OpenMP_CXX)
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

if(OpenMP_CXX_FOUND)
    target_link_libraries(hsOffline PUBLIC OpenMP::OpenMP_CXX)
endif()  
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

    if(OpenMP_CXX_FOUND)
      target_link_libraries(hsViewerQT PUBLIC OpenMP::OpenMP_CXX)
    endif()      
  
endif()

# ------------------------------------------------------------------
# blender
# ------------------------------------------------------------------
set(BLENDER_SUBDIRECTORY ON)
add_subdirectory(blender)

add_executable(hsBlender
  blender.cpp
  blender/renderengine_tcp.cpp
  blender/renderengine_tcp.h
  blender/renderengine_data.h

  DataLoader.h
  DataLoader.cpp
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

target_compile_definitions(hsBlender PUBLIC -DBLENDER_CLIENT)
target_include_directories(hsBlender PUBLIC blender)

target_link_libraries(hsBlender
  PUBLIC
  hayStack
  )

if(WITH_CLIENT_GPUJPEG)
  target_include_directories(hsBlender PUBLIC ${GPUJPEG_INCLUDE_DIR})
  target_compile_definitions(hsBlender PUBLIC -DWITH_CLIENT_GPUJPEG)
  target_link_libraries(hsBlender PUBLIC ${GPUJPEG_LIBRARIES})
endif()

if(OpenMP_CXX_FOUND)
  target_link_libraries(hsBlender PUBLIC OpenMP::OpenMP_CXX)
endif()      

