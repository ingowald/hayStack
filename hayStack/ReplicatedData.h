// SPDX-FileCopyrightText: Copyright (c) 2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include <map>
#include <string>

namespace hs {

  /*! render settings that are the same for all ranks */
  struct GlobalRenderSettings {
    int   samplesPerPixel = 1;
    float ambientRadiance = .8f;
    
    // invalid value: leave this to the renderer, allowing to create
    // the default gradient
    vec4f bgColor { -1.f };
    
    /*! default color map index to use */
    int defaultColorMapIndex = 0;

    bool denoise = true;
    bool enablePrincipledVolumes = true;
  };

  /*! light sources that are shared by (ie, get added to) all
      partitions */
  struct SharedLights {
    std::vector<mini::DirLight>  directional;
    std::vector<mini::QuadLight> quad;
    std::string envMap;
  };

  /*! data that is *not* partitioned across data groups, but that
      every rank/process has a full copy of (ie, the result of loading
      ReplicatedContent) */
  struct ReplicatedData {
    GlobalRenderSettings globalRenderSettings;

    SharedLights sharedLights;
  };

}
