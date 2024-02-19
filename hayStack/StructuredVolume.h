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
  struct StructuredVolume {
    typedef std::shared_ptr<StructuredVolume> SP;
    
    //    typedef enum { FLOAT, UINT8, UINT16 } ScalarType;

    StructuredVolume(vec3i dims,
                     BNTexelFormat texelFormat,
                     // ScalarType scalarType,
                     std::vector<uint8_t> &rawData,
                     std::vector<uint8_t> &rawDataRGB,
                     const vec3f &gridOrigin,
                     const vec3f &gridSpacing)
      : dims(dims),
        texelFormat(texelFormat),//scalarType(scalarType),
        rawData(std::move(rawData)),
        rawDataRGB(std::move(rawDataRGB)),
        gridOrigin(gridOrigin),
        gridSpacing(gridSpacing)
    {}

    box3f getBounds() const;
    range1f getValueRange() const;

    /*! dimensions of grid of scalars in rawData */
    vec3i      dims;
    std::vector<uint8_t> rawData;
    /*! either empty, or 3xuint8_t (RGB) for each voxel */
    std::vector<uint8_t> rawDataRGB;
    // ScalarType scalarType;
    const BNTexelFormat texelFormat;
    vec3f gridOrigin, gridSpacing;
  };

  inline size_t sizeOf(BNTexelFormat type)
  {
    switch(type) {
    case BN_TEXEL_FORMAT_R32F:
      return sizeof(float); 
    case BN_TEXEL_FORMAT_R16:
      return sizeof(uint16_t);
    case BN_TEXEL_FORMAT_R8:
      return sizeof(uint8_t);
    // case StructuredVolume::FLOAT: return sizeof(float); 
    // case StructuredVolume::UINT16: return sizeof(uint8_t);
    // case StructuredVolume::UINT8: return sizeof(uint8_t);
    default: throw std::runtime_error("un-handled scalar type");
    };
  }
  
}
