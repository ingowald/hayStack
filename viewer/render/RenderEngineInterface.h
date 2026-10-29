// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/TransferFunction.h"
#include "hayStack/VolumeScatter.h"

/* parallel renderer abstraction */
namespace hm {
  using namespace hs;

  /*! base abstraction for any renderer - no matter whether it's a
      single node or multiple workers on the back */
  struct RenderEngineInterface {

    virtual void setTransferFunction(const hs::TransferFunction &xf) {}
    virtual void setEmissionTransferFunction(const hs::TransferFunction &xf) {}
    virtual void setVolumeScatterSettings(const hs::VolumeScatterSettings &settings) {}

    void setTransferFunctionSet(const VolumeTransferFunctionSet &xf)
    {
      setTransferFunction(xf.opacity);
      setTransferFunction(xf.emission);
      setVolumeScatterSettings(xf.scatterSettings);
    }
    
    virtual void renderFrame() {}
    virtual void resize(const vec2i &fbSize, uint32_t *hostRgba) {}
    virtual void resetAccumulation() {}
    virtual void setCamera(const hs::Camera &camera) {}
    virtual void screenShot() {}
    virtual void terminate() {}
    virtual void setLights(float ambient,
                           const std::vector<hs::PointLight> &pointLights,
                           const std::vector<hs::DirLight> &dirLights) {}


    // ==================================================================
    // scene query interface - how the viewer/remoteViewer/offlineApp
    // can figure out what's there to set/modify
    // ==================================================================

    /*! global world boudns across all surfaces and volumes */
    BoundsData worldBounds;
  };

}
