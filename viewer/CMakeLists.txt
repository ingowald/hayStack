# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(render)

add_library(viewer-config INTERFACE)

set(SOURCES
  main.cpp
)

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


# ------------------------------------------------------------------
if (HS_IMGUI)
  add_executable(hsViewer ${SOURCES})
  target_link_libraries(hsViewer
    PUBLIC
    hayStack
    hayStackDataLoader
    hayMaker
    owlGLFWViewer
    viewer-config
    )
  target_compile_definitions(hsViewer PUBLIC -DHS_IMGUI=1)
  
endif()
