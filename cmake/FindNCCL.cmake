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

# FindNCCL.cmake

# Try to find NCCL
find_path(NCCL_INCLUDE_DIR NAMES nccl.h)
find_library(NCCL_LIBRARY NAMES nccl)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NCCL_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(NCCL DEFAULT_MSG NCCL_LIBRARY NCCL_INCLUDE_DIR)

mark_as_advanced(NCCL_INCLUDE_DIR NCCL_LIBRARY)

# Set NCCL_VERSION if it can be detected
file(READ "${NCCL_INCLUDE_DIR}/nccl.h" NCCL_H_CONTENTS)
string(REGEX MATCH "#define NCCL_MAJOR ([0-9]+)" _ ${NCCL_H_CONTENTS})
set(NCCL_VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define NCCL_MINOR ([0-9]+)" _ ${NCCL_H_CONTENTS})
set(NCCL_VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define NCCL_PATCH ([0-9]+)" _ ${NCCL_H_CONTENTS})
set(NCCL_VERSION_PATCH "${CMAKE_MATCH_1}")
set(NCCL_VERSION "${NCCL_VERSION_MAJOR}.${NCCL_VERSION_MINOR}.${NCCL_VERSION_PATCH}")

if(NCCL_FOUND)
    # Set the NCCL_INCLUDE_DIRS and NCCL_LIBRARIES variables
    set(NCCL_INCLUDE_DIRS "${NCCL_INCLUDE_DIR}")
    set(NCCL_LIBRARIES "${NCCL_LIBRARY}")

    # Set an environment variable (optional)
    set(ENV{NCCL_LIB_DIR} "${NCCL_LIBRARY}")
endif()
