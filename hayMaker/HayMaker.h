// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
// current rank's parition(s) of distributed: model
#include "hayStack/LocalPartitions.h"
#include "hayMaker/MPIRenderEngine.h"

#include <anari/anari_cpp.hpp>
#include <anari/anari_cpp/ext/linalg.h>

namespace hm {
  /*! the actual anari renderer part for the a given gpu/device */
  struct AnariDeviceRenderer;

  struct GlobalRenderSettings {
    int   samplesPerPixel = 1;
    float ambientRadiance = .8f;
    
    // invalid value: leave this to the renderer, allowing to create
    // the default gradient
    vec4f bgColor { -1.f };
    
    /*! default color map index to use */
    int defaultColorMapIndex = 0;
  };
  
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
    HayMaker(Comm &world,
             Comm &workers,
             GlobalRenderSettings &globalRenderSettings,
             hs::LocalPartitions *localPartitions,
             const std::vector<DeviceConfig> &deviceConfigs);

    void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame();
    void resetAccumulation();
    void setCamera(const Camera &camera);
    void finalizeRender();
    /*! clean up and shut down */
    void terminate() {}

    /*! go over all input content, and 'render' this into an
        anari::world; later renderFrame()'s can then simply use that
        frame with updated camera */
    void renderInitialAnariWorld();
    
    inline int numDevices() const { return perDevice.size(); }
    BoundsData getWorldBounds() const;

    uint32_t     *hostRGBA   = 0;
    vec2i         fbSize;
    /*! whether we have to re-commit the model next frame */
    bool          dirty = true;

    std::vector<AnariDeviceRenderer *> perDevice;

    // the library used to create the device(s)
    anari::Library library;
      
    Comm            &world;
    Comm            &workers;
    hs::LocalPartitions *const localPartitions;
    GlobalRenderSettings globalRenderSettings;
    const std::vector<DeviceConfig> deviceConfigs;
  };

}
