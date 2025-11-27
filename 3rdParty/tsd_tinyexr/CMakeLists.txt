## Copyright 2024 NVIDIA Corporation
## SPDX-License-Identifier: Apache-2.0

add_library(ext_tinyexr INTERFACE)
target_include_directories(ext_tinyexr INTERFACE ${CMAKE_CURRENT_LIST_DIR})

message("adding ext_tinyexr_impl")
add_library(ext_tinyexr_impl tinyexr.cpp)
target_link_libraries(ext_tinyexr_impl PUBLIC ext_tinyexr)
