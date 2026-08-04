// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/TransferFunction.h"
#include "hayStack/NanoVDBVolume.h"

/* parallel renderer abstraction */
namespace hm {
  
  /*! base abstraction for any renderer - no matter whether it's a
      single node or multiple workers on the back */
  struct RenderEngineInterface {

    virtual void setTransferFunction(const hs::TransferFunction &xf) {}
    virtual void setVolumeScatterSettings(const hs::VolumeScatterSettings &settings) {}
    virtual hs::VolumeScatterSettings getVolumeScatterSettings() const { return {}; }
    
    virtual void renderFrame() {}
    virtual void resize(const vec2i &fbSize, uint32_t *hostRgba) {}
    virtual void resetAccumulation() {}
    virtual void setCamera(const hs::Camera &camera) {}
    // virtual void setXF(const range1f &domain,
    //                    const std::vector<vec4f> &colors) {}
    virtual void screenShot() {}
    virtual void terminate() {}
    virtual void setLights(float ambient,
                           const std::vector<hs::PointLight> &pointLights,
                           const std::vector<hs::DirLight> &dirLights) {}
  };

}
