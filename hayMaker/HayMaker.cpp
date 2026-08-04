// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/ColorMap.h"
#include "hayStack/TransferFunction.h"
#include "hayMaker/HayMaker.h"
#include "hayMaker/SingleDeviceRenderer.h"

namespace hm {

  static void anariStatusFunc(const void * /*userData*/,
                              ANARIDevice /*device*/,
                              ANARIObject source,
                              ANARIDataType /*sourceType*/,
                              ANARIStatusSeverity severity,
                              ANARIStatusCode /*code*/,
                              const char *message)
  {
    if (severity == ANARI_SEVERITY_FATAL_ERROR) {
      fprintf(stderr, "[FATAL][%p] %s\n", source, message);
      std::exit(1);
    } else if (severity == ANARI_SEVERITY_ERROR) {
      fprintf(stderr, "[ERROR][%p] %s\n", source, message);
    } else if (severity == ANARI_SEVERITY_WARNING) {
      fprintf(stderr, "[WARN ][%p] %s\n", source, message);
    } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
      fprintf(stderr, "[PERF ][%p] %s\n", source, message);
    }
    // Ignore INFO/DEBUG messages
  }

  HayMaker::HayMaker(Comm &world,
                     Comm &workers,
                     GlobalRenderSettings &globalRenderSettings,
                     hs::LocalPartitions *localPartitions,
                     const std::vector<DeviceConfig> &deviceConfigs)
    : world(world),
      workers(workers),
      globalRenderSettings(globalRenderSettings),
      localPartitions(localPartitions)
  {
    assert(!deviceConfigs.empty());
    BoundsData bb = getWorldBounds();
    if (!bb.mapped.empty()) {
      hs::ColorMap::init();
      int cmID = globalRenderSettings.defaultColorMapIndex
        % hs::ColorMap::maps.size();
      std::cout << "#hs: using scalar-mapping color map #" << cmID
                << " : " << hs::ColorMap::maps[cmID].first << std::endl;
      for (auto pd : perDevice)
        pd->createColorMapper(bb.mapped,
                              hs::ColorMap::maps[cmID].second);
    }
    
    char *envlib = getenv("ANARI_LIBRARY");
    std::string libname = envlib ? "environment" :
#if HS_MPI
      "barney_mpi"
#else
      "barney"
#endif
      ;
    library = anari::loadLibrary(libname.c_str(), anariStatusFunc);
  }

  void HayMaker::initialBuild()
  {
    for (auto dev : perDevice)
      dev->renderAll();
    
    // for (auto dev : perDevice)
    //   dev->finalizeRender();
  }

  BoundsData HayMaker::getWorldBounds() const
  {
    BoundsData bb = localPartitions->getBounds();
    bb.spatial.lower = world.allReduceMin(bb.spatial.lower);
    bb.spatial.upper = world.allReduceMax(bb.spatial.upper);
    bb.scalars.lower = world.allReduceMin(bb.scalars.lower);
    bb.scalars.upper = world.allReduceMax(bb.scalars.upper);
    bb.mapped.lower = world.allReduceMin(bb.mapped.lower);
    bb.mapped.upper = world.allReduceMax(bb.mapped.upper);
    
    if (bb.spatial.empty()) {
      bb.spatial = {vec3f(-1.f),vec3f(+1.f)};
    }  
    
    return bb;
  }


#if 0
  //void init();
  void terminate() override { global.terminate(); }
    

  void buildPartitions() override;
    
  void resize(const vec2i &fbSize, uint32_t *hostRGBA) override
  { global.resize(fbSize,hostRGBA); }
    
  void setTransferFunction(const TransferFunction &xf) override
  {
    for (auto partition : perPartition)
      partition->setTransferFunction(xf);
  }
#if HS_USE_MULTI_SCATTERING
  void setVolumeScatterSettings(const VolumeScatterSettings &settings) override
  {
    for (auto partition : perPartition)
      partition->setVolumeScatterSettings(settings);
  }
  VolumeScatterSettings getVolumeScatterSettings() const override
  {
    if (perPartition.empty())
      return {};
    return perPartition[0]->volumeScatterSettings;
  }
#endif
  void renderFrame() override
  {
    buildPartitions();
    global.renderFrame();
  }
    
  void resetAccumulation() override
  { global.resetAccumulation(); }
    
  void setCamera(const Camera &camera) override
  { global.setCamera(camera); }

  
  // template<typename Backend>
  // HayMakerT<Backend>::HayMakerT(Comm &world,
  //                               Comm &workers,
  //                               int pathsPerPixel,
  //                               float ambientRadiance,
  //                               vec4f bgColor,
  //                               LocalPartitions &localPartitions,
  //                               const std::vector<int> &gpuIDs,
  //                               bool verbose)
  //   : HayMaker(world,workers,pathsPerPixel,ambientRadiance,bgColor,localPartitions,gpuIDs,verbose),
  //     global(this)
  // {
  // }


#endif
  
}

