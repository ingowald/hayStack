// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

// #include "barney.h"
#include "hayStack/HayStack.h"

namespace hs {

  /*! like all other things in haystack, this is designed to be able
      to store *ONE RANK'S PART* of what may - across all ranks - be a
      logically much larger volume */
  struct StructuredVolume {
    typedef std::shared_ptr<StructuredVolume> SP;
    
    //    typedef enum { FLOAT, UINT8, UINT16 } ScalarType;

    StructuredVolume(vec3i dims,
                     const std::string &texelFormat,//BNDataType texelFormat,
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
    const std::string// BNDataType
    texelFormat;
    vec3f gridOrigin, gridSpacing;
  };

  inline size_t sizeOf(const std::string &type)
  {
    if (type == "float") return sizeof(float);
    if (type == "uint8_t") return sizeof(uint8_t);
    if (type == "uint16_t") return sizeof(uint16_t);
    // switch(type) {
    // case BN_FLOAT:
    //   return sizeof(float); 
    // case BN_UFIXED16:
    //   return sizeof(uint16_t);
    // case BN_UFIXED8:
    //   return sizeof(uint8_t);
    // case StructuredVolume::FLOAT: return sizeof(float); 
    // case StructuredVolume::UINT16: return sizeof(uint8_t);
    // case StructuredVolume::UINT8: return sizeof(uint8_t);
    // default:
      throw std::runtime_error("un-handled scalar type");
    // };
  }
}
