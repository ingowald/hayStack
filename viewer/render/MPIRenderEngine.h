// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/MPIWrappers.h"
#include "viewer/render/RenderEngineInterface.h"

/* parallel renderer abstraction */
namespace hm {
  using namespace hs;
  
  // using hs::mpi::Comm;
  
  /*! base abstraction for any renderer - no matter whether its a
    single node or multiple workers on the back */
  struct MPIRenderEngine : public RenderEngineInterface {
    MPIRenderEngine(std::shared_ptr<Comm> comm,
                    RenderEngineInterface *passThrough = 0);
    
    void renderFrame() override;
    void resize(const vec2i &fbSize,
                uint32_t *fbPointer) override;
    void resetAccumulation() override;
    void setCamera(const Camera &camera) override;
    void setTransferFunction(const TransferFunction &xf) override;
    void setEmissionTransferFunction(const TransferFunction &xf) override;
    void setVolumeScatterSettings(const VolumeScatterSettings &settings) override;
    void terminate() override;
    void setLights(float ambient,
                   const std::vector<hs::PointLight> &pointLights,
                   const std::vector<hs::DirLight> &dirLights) override;

    static void runWorker(std::shared_ptr<Comm> comm,
                          RenderEngineInterface *client);

    BoundsData getWorldBounds() override;
  private:
    template<typename T>
    void sendToWorkers(const std::vector<T> &t);
    
    template<typename T>
    void sendToWorkers(const T &t);

    void checkEndOfMessage();
    void sendEndOfMessage();

    /*! passthrough-renderer on master node */
    RenderEngineInterface *passThrough = 0;

    int eomIdentifierBase = 0x12345;
  protected:
    std::shared_ptr<Comm> comm;
    
  };

}
