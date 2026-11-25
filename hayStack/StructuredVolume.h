// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

// #include "barney.h"
#include "hayStack/HayStack.h"
#include "hayStack/Volume.h"

namespace hs {

  /*! like all other things in haystack, this is designed to be able
      to store *ONE RANK'S PART* of what may - across all ranks - be a
      logically much larger volume */
  struct StructuredVolume : public Volume {
    typedef std::shared_ptr<StructuredVolume> SP;
    
    //    typedef enum { FLOAT, UINT8, UINT16 } ScalarType;

    StructuredVolume(vec3i dims,
                     const std::string &texelFormat,//BNDataType texelFormat,
                     // ScalarType scalarType,
                     std::vector<uint8_t> &rawData,
                     std::vector<uint8_t> &rawDataRGB,
                     const vec3f &gridOrigin,
                     const vec3f &gridSpacing,
                     const std::string &xfName = "defaultVolume")
      : Volume(xfName),
        dims(dims),
        texelFormat(texelFormat),//scalarType(scalarType),
        rawData(std::move(rawData)),
        rawDataRGB(std::move(rawDataRGB)),
        gridOrigin(gridOrigin),
        gridSpacing(gridSpacing)
    {}

    box3f getBounds() const override;
    range1f getValueRange() const override;

    /*! dimensions of grid of scalars in rawData */
    vec3i      dims;
    std::vector<uint8_t> rawData;
    /*! either empty, or 3xuint8_t (RGB) for each voxel */
    std::vector<uint8_t> rawDataRGB;
    const std::string    texelFormat;
    vec3f gridOrigin, gridSpacing;
  };

  inline size_t sizeOf(const std::string &type)
  {
    if (type == "float") return sizeof(float);
    if (type == "uint8_t") return sizeof(uint8_t);
    if (type == "uint16_t") return sizeof(uint16_t);
    throw std::runtime_error("un-handled scalar type");
  }
}
