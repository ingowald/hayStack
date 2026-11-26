// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
// current rank's parition(s) of distributed: model
#include "hayStack/LocalPartitions.h"
#include "viewer/render/MPIRenderEngine.h"

#include <anari/anari_cpp.hpp>
#include <anari/anari_cpp/ext/linalg.h>

namespace hm {
  /*! the actual anari renderer part for the a given gpu/device */
  struct AnariDeviceRenderer;

  struct DeviceConfig {
    /*! which gpu to use. which actual gpu that refers to depends on
        the backend; for a cuda/optix based backend this'll be the
        cuda device id */
    int gpuID = 0;
    
    /*! given the one or more partitions that have been loaded on the
      current rank/process, which one to use. Each partition will
      itself know which logical partition out of how many total
      logical partitions it is; this value only refers to which of the
      current rank's partitions to use. */
    int localPartitionIndex = 0;
  };
    
  struct HayMaker : public RenderEngineInterface {
    HayMaker(Comm::SP world,
             hs::LocalPartitions *localPartitions,
             const std::vector<DeviceConfig> &deviceConfigs);

    void resize(const vec2i &fbSize,
                uint32_t *fbColor,
                /* iw - whether we need to render with depth
                   channel. mind we can NOT get that from testing
                   fbDepth for being a nullptr, because on client it
                   may be null anyway */
                bool hasDepth,
                float *fbDepth) override;
    // void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame() override;
    void resetAccumulation();
    void setCamera(const Camera &camera);
    void finalizeRender();
    /*! clean up and shut down */
    void terminate() {}

    void setTransferFunction(const std::string &xfName,
                             const hs::TransferFunction &xf) override;
    void setEmissionTransferFunction(const std::string &xfName,
                                     const hs::TransferFunction &xf) override;
    void setVolumeScatterSettings(const std::string &xfName,
                                  const hs::VolumeScatterSettings &settings) override;
    // hs::VolumeScatterSettings getVolumeScatterSettings() const override;

    BoundsData getWorldBounds() override;
    std::set<std::string> gatherXFNames() override;
    
    /*! go over all input content, and 'render' this into an
        anari::world; later renderFrame()'s can then simply use that
        frame with updated camera */
    void renderInitialAnariWorld();
    
    inline int numDevices() const { return perDevice.size(); }

    uint32_t     *hostColor = 0;
    float        *hostDepth = 0;
    vec2i         fbSize;
    
    /*! whether we have to re-commit the model next frame */
    bool          dirty = true;

    std::vector<AnariDeviceRenderer *> perDevice;

    // the library used to create the device(s)
    anari::Library library;

    Comm::SP world;
    hs::LocalPartitions *const localPartitions;
    const std::vector<DeviceConfig> deviceConfigs;
#if HS_EXP_ALTFRAME
    vec2i alt_fbSize{2*1600,1*1200};
#endif
  private:
    // void computeWorldBounds();
  };

}
