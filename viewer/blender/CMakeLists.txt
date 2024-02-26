# ======================================================================== //
# Copyright 2022-2022 Ingo Wald                                            //
# Copyright 2022-2023 IT4Innovations, VSB - Technical University of Ostrava//
#                                                                          //
# Licensed under the Apache License, Version 2.0 (the "License");          //
# you may not use this file except in compliance with the License.         //
# You may obtain a copy of the License at                                  //
#                                                                          //
#     http://www.apache.org/licenses/LICENSE-2.0                           //
#                                                                          //
# Unless required by applicable law or agreed to in writing, software      //
# distributed under the License is distributed on an "AS IS" BASIS,        //
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
# See the License for the specific language governing permissions and      //
# limitations under the License.                                           //
# ======================================================================== //

if(NOT BLENDER_SUBDIRECTORY)
    cmake_policy(SET CMP0048 NEW)
    project(hayStack VERSION 0.0.1 LANGUAGES C CXX)

    cmake_minimum_required(VERSION 3.16)
    #set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    option(WITH_CLIENT_GPUJPEG "Enable GPUJPEG" OFF)

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.18)
        cmake_policy(SET CMP0104 NEW)
    endif()
endif()

if(WITH_CLIENT_GPUJPEG)
	set(GPUJPEG_INCLUDE_DIR "" CACHE PATH "")
    set(GPUJPEG_LIBRARIES "" CACHE FILEPATH "")

	add_definitions(-DWITH_CLIENT_GPUJPEG)
endif()

set(INC
	 .
     ${GPUJPEG_INCLUDE_DIR}
)

set(SRC
	renderengine.cpp
    renderengine_tcp.cpp
)

set(SRC_HEADERS
    renderengine_api.h
    renderengine_data.h
    
    renderengine_tcp.h
)

include_directories(${INC})
add_library(hayStack-renderengine SHARED ${SRC} ${SRC_HEADERS})
#target_link_libraries(hayStack-renderengine stb_image)

if(WITH_CLIENT_GPUJPEG)
    target_link_libraries(hayStack-renderengine ${GPUJPEG_LIBRARIES})
endif()
  
install (TARGETS hayStack-renderengine DESTINATION addons/hayStack)
install (FILES addons/hayStack/__init__.py DESTINATION addons/hayStack)
