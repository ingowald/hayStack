# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_library(glfwBaseViewer-config INTERFACE)
target_link_libraries(glfwBaseViewer-config INTERFACE hs-config)
target_include_directories(glfwBaseViewer-config INTERFACE ${CMAKE_CURRENT_LIST_DIR}/..)

# ------------------------------------------------------------------
# make sure we have GLFW - use installed version if possible, build
# your own if not
# ------------------------------------------------------------------
find_package(glfw3 3.3)
if (NOT(TARGET glfw))
  add_subdirectory(../../3rdParty/glfw build_glfwx)
endif()
target_link_libraries(glfwBaseViewer-config INTERFACE glfw)

set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED OPTIONAL_COMPONENTS OpenGL)
if(TARGET OpenGL::OpenGL)
  target_link_libraries(glfwBaseViewer-config INTERFACE OpenGL::OpenGL)
else()
  target_link_libraries(glfwBaseViewer-config INTERFACE OpenGL::GL)
endif()


# ------------------------------------------------------------------
# build imgui into a static lib
# ------------------------------------------------------------------
add_library(hs_imgui STATIC
  ../../3rdParty/imgui/imgui.cpp
  ../../3rdParty/imgui/imgui_demo.cpp
  ../../3rdParty/imgui/imgui_draw.cpp
  ../../3rdParty/imgui/imgui_tables.cpp
  ../../3rdParty/imgui/imgui_widgets.cpp
  ../../3rdParty/imgui/backends/imgui_impl_glfw.cpp
  ../../3rdParty/imgui/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(hs_imgui PUBLIC hs-config)
target_include_directories(hs_imgui PUBLIC
  ../../3rdParty/imgui
  ../../3rdParty/imgui/backends
  )


# ------------------------------------------------------------------
# base glfw viewer infrastructure, former owlViewer repo
# ------------------------------------------------------------------
add_library(glfwBaseViewer STATIC
  
  GLFWBaseViewer.h
  GLFWBaseViewer.cpp

#  ColorMaps.h
#  ColorMaps.cpp

#  XFEditor.h
#  XFEditor.cpp
  
  Camera.h 
  Camera.cpp
  
  InspectMode.h
  InspectMode.cpp
  
  FlyMode.h
  FlyMode.cpp
)

target_link_libraries(glfwBaseViewer
  PUBLIC
  hs_imgui
  glfwBaseViewer-config
)

