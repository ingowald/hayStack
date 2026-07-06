// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/LocalModel.h"
#include "hayMaker/MPIRenderer.h"
#include "hayMaker/Anari.h"
# include "hayStack/NanoVDBVolume.h"
#include <unordered_map>

namespace hm {
  struct PerRank;
  struct PerDevice;

  struct DeviceConfig {
    int gpuID;
    int localDataRank;
  };
    
  struct HayMaker {
    HayMaker(Comm &world,
             Comm &workers,
             hs::LocalModel &localModel,
             const std::vector<DeviceConfig> &deviceConfigs);

    void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame();
    void resetAccumulation();
    void setCamera(const Camera &camera);
    void finalizeRender();
    /*! clean up and shut down */
    void terminate() {}
      
    HayMaker *const base;

    uint32_t     *hostRGBA   = 0;
    vec2i         fbSize;
    /*! whether we have to re-commit the model next frame */
    bool          dirty = true;

    /*! points to the first slot, which is the only slot in
      non-data-parallel, and the master slot in data-parallel */
    std::vector<PerDevice *> perDevice;
    inline int numDevices() const { return perDevice.size(); }
    BoundsData getWorldBounds() const;

    // the library used to create the device(s)
    anari::Library library;
      
    Comm       &world;
    Comm       &workers;
    LocalModel &localModel;
    const std::vector<DeviceConfig> deviceConfigs;
  };

}
