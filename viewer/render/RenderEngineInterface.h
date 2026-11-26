// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/TransferFunction.h"
#include "hayStack/VolumeScatter.h"
#include <set>
#include <string>

/* parallel renderer abstraction */
namespace hm {
  using namespace hs;

  /*! base abstraction for any renderer - no matter whether it's a
    single node or multiple workers on the back */
  struct RenderEngineInterface {

    /*! @{ set the respective transfer function / scatter settings
        for all volumes whose xfName matches the given one */
    virtual void setTransferFunction(const std::string &xfName,
                                     const hs::TransferFunction &xf) {}
    virtual void setEmissionTransferFunction(const std::string &xfName,
                                             const hs::TransferFunction &xf) {}
    virtual void setVolumeScatterSettings(const std::string &xfName,
                                          const hs::VolumeScatterSettings &settings) {}
    /* @} */

    /*! sets the transfer function set that all volumes with given
        xfName should use */
    void setTransferFunctionSet(const std::string &xfName,
                                const VolumeTransferFunctionSet &xf);

    virtual void renderFrame() {}
    virtual void resize(const vec2i &fbSize,
                        uint32_t *fbColor,
                        bool needDepth,
                        float *fbDepth)
    {}
    // { fb.color.resize(fbSize.x*fbSize.y); fb.color.size = fbSize; }
    virtual void resetAccumulation() {}
    virtual void setCamera(const hs::Camera &camera) {}
    virtual void terminate() {}
    virtual void setLights(float ambient,
                           const std::vector<hs::PointLight> &pointLights,
                           const std::vector<hs::DirLight> &dirLights) {}

    // ==================================================================
    // scene query interface - how the viewer/remoteViewer/offlineApp
    // can figure out what's there to set/modify
    // ==================================================================
    virtual BoundsData getWorldBounds() = 0;

    /*! returns all (distinct) transfer function names used by any
        of the volumes in the scene, together with the range of
        scalar values of all volumes using that name; sorted by name */
    virtual std::vector<std::pair<std::string,range1f>> gatherXFNames() = 0;
    
  };


  struct PassthroughREI : public RenderEngineInterface {
    PassthroughREI(RenderEngineInterface *pt)
      : pt(pt)
    {}

    void setTransferFunction(const std::string &xfName,
                             const hs::TransferFunction &xf)
      override
    { pt->setTransferFunction(xfName,xf); }
    void setEmissionTransferFunction(const std::string &xfName,
                                     const hs::TransferFunction &xf) 
      override
    { pt->setEmissionTransferFunction(xfName,xf); }
    
    void setVolumeScatterSettings(const std::string &xfName,
                                  const hs::VolumeScatterSettings &settings)
      override
    { pt->setVolumeScatterSettings(xfName,settings); }
    
    BoundsData getWorldBounds()
      override
    { return pt->getWorldBounds(); }

    std::vector<std::pair<std::string,range1f>> gatherXFNames()
      override
    { return pt->gatherXFNames(); }
    
    void renderFrame()
      override
    { pt->renderFrame(); }
    void resize(const vec2i &fbSize,
                uint32_t *fbColor,
                bool needDepth,
                float *fbDepth)
      override
    { pt->resize(fbSize,fbColor,needDepth,fbDepth); }
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
  
  inline void RenderEngineInterface
  ::setTransferFunctionSet(const std::string &xfName,
                           const VolumeTransferFunctionSet &xf)
  {
    setTransferFunction(xfName,xf.opacity);
    setEmissionTransferFunction(xfName,xf.emission);
    setVolumeScatterSettings(xfName,xf.scatterSettings);
  }
}
