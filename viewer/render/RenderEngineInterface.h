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

    virtual BoundsData getWorldBounds() = 0;
    
    virtual void renderFrame() {}
    virtual void resize(const vec2i &fbSize,
                        uint32_t *fbPointer) {}
    // { fb.color.resize(fbSize.x*fbSize.y); fb.color.size = fbSize; }
    virtual void resetAccumulation() {}
    virtual void setCamera(const hs::Camera &camera) {}
    virtual void terminate() {}
    virtual void setLights(float ambient,
                           const std::vector<hs::PointLight> &pointLights,
                           const std::vector<hs::DirLight> &dirLights) {}


    // struct {
    //   std::vector<uint32_t> color;
    //   vec2i size;
    // } fb;
    
    // ==================================================================
    // scene query interface - how the viewer/remoteViewer/offlineApp
    // can figure out what's there to set/modify
    // ==================================================================

  };


  struct PassthroughREI : public RenderEngineInterface {
    PassthroughREI(RenderEngineInterface *pt)
      : pt(pt)
    {}

    void setTransferFunction(const hs::TransferFunction &xf)
      override
    { pt->setTransferFunction(xf); }
    void setEmissionTransferFunction(const hs::TransferFunction &xf) 
      override
    { pt->setEmissionTransferFunction(xf); }
    
    void setVolumeScatterSettings(const hs::VolumeScatterSettings &settings)
      override
    { pt->setVolumeScatterSettings(settings); }
    
    BoundsData getWorldBounds()
      override
    { return pt->getWorldBounds(); }
    
    void renderFrame()
      override
    { pt->renderFrame(); }
    void resize(const vec2i &fbSize,
                uint32_t *fbPointer)
      override
    { pt->resize(fbSize,fbPointer); }
    void resetAccumulation()
      override
    { pt->resetAccumulation(); }
    void setCamera(const hs::Camera &camera)
      override
    { pt->setCamera(camera); }
    void terminate()
      override
    { pt->terminate(); }
    void setLights(float ambient,
                   const std::vector<hs::PointLight> &pointLights,
                   const std::vector<hs::DirLight> &dirLights)
      override
    { pt->setLights(ambient,pointLights,dirLights); }
    
    RenderEngineInterface *const pt;
  };
  
}
