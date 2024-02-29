// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "barney.h"
#include "hayStack/HayStack.h"

namespace hs {

  /*! like all other things in haystack, this is designed to be able
      to store *ONE RANK'S PART* of what may - across all ranks - be a
      logically much larger volume */
  struct NanoVDBVolume {
    typedef std::shared_ptr<NanoVDBVolume> SP;

    NanoVDBVolume(vec3i dims,
                     BNTexelFormat texelFormat,
                     std::vector<uint8_t> &rawData,
                     const vec3f &gridOrigin,
                     const vec3f &gridSpacing)
      : dims(dims),
        texelFormat(texelFormat),
        rawData(std::move(rawData)),
        gridOrigin(gridOrigin),
        gridSpacing(gridSpacing)
    {}

    box3f getBounds() const;
    range1f getValueRange() const;

    /*! dimensions of grid of scalars in NanoVDB */
    vec3i      dims;
    std::vector<uint8_t> rawData;
    // ScalarType scalarType;
    const BNTexelFormat texelFormat;
    vec3f gridOrigin, gridSpacing;
  };

  inline size_t sizeOfNanoVDBTexel(BNTexelFormat type)
  {
    switch(type) {
    case BN_TEXEL_FORMAT_NANOVDB_FLOAT:
      return sizeof(float);
    default: throw std::runtime_error("un-handled scalar type");
    };
  }
  
}
