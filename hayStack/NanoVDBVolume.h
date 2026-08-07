// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"

namespace hs {
  /*
    \todo iw - this should be some global config setting across all of
    haystack, that different volume types can use; not just nanovdb
    volume. not clear how to best organize that yet but this needs cleanup
  */
  struct VolumeScatterParams {
    float anisotropy = 0.6f;
    float scatteringAlbedo = 0.9f;
    vec3f scatterColor = {0.8f, 0.8f, 0.8f};
    vec3f absorptionColor = {0.f, 0.f, 0.f};
    float density = 1.f;
    float densityThreshold = 0.f;
    float emissionStrength = 0.f;
    vec3f emissionColor = {1.f, 1.f, 1.f};
    float blackbodyIntensity = 0.f;
    vec3f blackbodyTint = {1.f, 1.f, 1.f};
    float temperature = 0.f;
  };

  /*
    \todo iw - this should be some global config setting across all of
    haystack, similar to samples per pixel, enabling denoiser,
    etc. not 100% sure that's part of hay-_stack_ (vs hay _maker_?),
    but either way it shouldn't be in here
  */
  struct VolumeScatterSettings {
    bool enabled = true;
    int maxVolumeBounces = 8;
    VolumeScatterParams medium;
  };

  /*! One rank's piece of a (possibly split) NanoVDB grid. @c data holds the
      raw NanoVDB buffer passed to Barney/ANARI as a UINT8 array1D. */
  struct NanoVDBVolume {
    typedef std::shared_ptr<NanoVDBVolume> SP;

    std::vector<uint8_t> data;
    std::string fileName;
    int partID = 0;
    vec3i fullIndexDims{0};
    box3i cellRange;
    box3f bounds;
    range1f valueRange{0.f, 1.f};
    VolumeScatterParams scatter;

    box3f getBounds() const { return bounds; }
    range1f getValueRange() const { return valueRange; }
  };

}
