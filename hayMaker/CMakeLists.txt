# SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
# SPDX-License-Identifier: Apache-2.0

find_package(anari 0.13.0 REQUIRED)

add_library(hayMaker
  HayMaker.h
  HayMaker.cpp
  TextureLibrary.h
  TextureLibrary.cpp
  MaterialLibrary.h
  MaterialLibrary.cpp
#  AnariBackend.h
#  AnariBackend.cpp
  MPIRenderer.h
  MPIRenderer.cpp
  
  SingleDeviceRenderer.h
  SingleDeviceRenderer.cpp
)

target_link_libraries(hayMaker
  PUBLIC
  hayStack
  anari::anari
)
