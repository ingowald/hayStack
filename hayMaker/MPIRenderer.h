// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayMaker/common.h"
#include "hayStack/MPIWrappers.h"
#include "hayMaker/Renderer.h"

/* parallel renderer abstraction */
namespace hm {
  using namespace hs;
  
  using hs::mpi::Comm;
  
  /*! base abstraction for any renderer - no matter whether its a
    single node or multiple workers on the back */
  struct MPIRenderer : public Renderer {
    MPIRenderer(Comm &comm,
                Renderer *passThrough = 0);
    
    void renderFrame() override;
    void resize(const vec2i &fbSize, uint32_t *hostRgba) override;
    void resetAccumulation() override;
    void setCamera(const Camera &camera) override;
    // void setXF(const range1f &domain,
    //            const std::vector<vec4f> &colors) override;
    void setTransferFunction(const TransferFunction &xf) override;
#if HS_USE_MULTI_SCATTERING
    void setVolumeScatterSettings(const VolumeScatterSettings &settings) override;
#endif
    void screenShot() override;
    void terminate() override;
    void setLights(float ambient,
                   const std::vector<hs::PointLight> &pointLights,
                   const std::vector<hs::DirLight> &dirLights) override;

    static void runWorker(Comm &comm,
                          Renderer *client);

  private:
    template<typename T>
    void sendToWorkers(const std::vector<T> &t);
    
    template<typename T>
    void sendToWorkers(const T &t);

    void checkEndOfMessage();
    void sendEndOfMessage();
    
    Comm &comm;
    
    /*! passthrough-renderer on master node */
    Renderer *passThrough = 0;

    int eomIdentifierBase = 0x12345;
  };

}
