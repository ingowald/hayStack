# SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
# SPDX-License-Identifier: Apache-2.0

find_package(anari 0.13.0 REQUIRED)

option(HS_EXP_ALTFRAME "Haystack second-frame test" OFF)

add_library(hayMaker
  AnariDeviceRenderer.h
  AnariDeviceRenderer.cpp
  HayMaker.h
  HayMaker.cpp
  TextureLibrary.h
  TextureLibrary.cpp
  MaterialLibrary.h
  MaterialLibrary.cpp

  MPIRenderEngine.h
  MPIRenderEngine.cpp
)

target_link_libraries(hayMaker
  PUBLIC
  hayStack
  anari::anari
)
if (HS_EXP_ALTFRAME)
  target_compile_definitions(hayMaker PUBLIC HS_EXP_ALTFRAME=1)
endif()
