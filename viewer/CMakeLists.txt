# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_library(viewer-config INTERFACE)

set(SOURCES
  main.cpp
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

# ------------------------------------------------------------------
add_executable(hsOffline
  ${SOURCES}
  )
target_link_libraries(hsOffline
  PUBLIC
  hayStack
  hayStackDataLoader
  hayMaker
  viewer-config
  )

# ------------------------------------------------------------------

if (HS_CUTEE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_COMPILE_OPTIONS_PIE "-fPIC")
  
  set(CMAKE_AUTOMOC OFF)
  add_executable(hsViewerQT ${SOURCES})
  if (HS_USE_MULTI_SCATTERING)
    target_sources(hsViewerQT PRIVATE
      VolumeScatterPanel.cpp
      VolumeScatterPanel.h)
  endif()
  target_link_libraries(hsViewerQT
    PUBLIC
    hayStack
  hayMaker
    qtOWL
    viewer-config
    )
  target_compile_definitions(hsViewerQT PUBLIC -DHS_CUTEE=1)
  set_target_properties(hsViewerQT
    PROPERTIES
    AUTOMOC ON
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
    CUDA_USE_STATIC_CUDA_RUNTIME ON
    CUDA_ARCHITECTURES OFF
    )
  if (APPLE)
    set_target_properties(hsViewerQT
      PROPERTIES
      MACOSX_BUNDLE ON
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
      )
  endif()
  
endif()

# ------------------------------------------------------------------
#add_executable(hsDLAFtoPCR
#  content/dlafToPCR.cpp
#  )
#target_link_libraries(hsDLAFtoPCR
#  PUBLIC
#  hayStack
#  )
