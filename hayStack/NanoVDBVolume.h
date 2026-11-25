// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/Volume.h"

namespace hs {
  /*! One rank's piece of a (possibly split) NanoVDB grid. @c data holds the
      raw NanoVDB buffer passed to Barney/ANARI as a UINT8 array1D. */
  struct NanoVDBVolume : public Volume {
    typedef std::shared_ptr<NanoVDBVolume> SP;

    NanoVDBVolume(const std::string &xfName)
      : Volume(xfName)
    {}

    std::vector<uint8_t> data;
    std::string fileName;
    int partID = 0;
    vec3i fullIndexDims{0};
    box3i cellRange;
    box3f bounds;
    range1f valueRange{0.f, 1.f};

    box3f getBounds() const override { return bounds; }
    range1f getValueRange() const override { return valueRange; }
  };

}
