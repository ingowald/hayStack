// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"

namespace hs {

  struct VolumeScatterParams {
    float anisotropy = 0.6f;
    float emissionStrength = 0.f;
  };

  struct VolumeScatterSettings {
    void read(std::istream &in);
    void write(std::ostream &out) const;
    
    bool enabled = true;
    int maxVolumeBounces = 8;
    VolumeScatterParams medium;
  };

} // namespace hs
