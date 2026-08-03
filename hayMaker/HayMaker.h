// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
// current rank's parition(s) of distributed: model
#include "hayStack/LocalPartitions.h"
#include "hayMaker/MPIRenderer.h"

namespace hm {
  /*! the actual anari renderer part for the a given gpu/device */
  struct SingleDeviceRenderer;

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
    
  struct HayMaker : public Renderer {
    HayMaker(Comm &world,
             Comm &workers,
             hs::LocalPartitions *localPartitions,
             const std::vector<DeviceConfig> &deviceConfigs);

    void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame();
    void resetAccumulation();
    void setCamera(const Camera &camera);
    void finalizeRender();
    /*! clean up and shut down */
    void terminate() {}

    /*! perform initial rendering of local model partion(s) to created
        anari device(s) */
    void initialBuild();
    
    inline int numDevices() const { return perDevice.size(); }
    BoundsData getWorldBounds() const;

    uint32_t     *hostRGBA   = 0;
    vec2i         fbSize;
    /*! whether we have to re-commit the model next frame */
    bool          dirty = true;

    std::vector<SingleDeviceRenderer *> perDevice;

    // the library used to create the device(s)
    anari::Library library;
      
    Comm            &world;
    Comm            &workers;
    hs::LocalPartitions *const localPartitions;
    GlobalModelData  globalModelData;
    const std::vector<DeviceConfig> deviceConfigs;
    
    /*! default color map index to use */
    static int colorMapIndex;
  };

}
