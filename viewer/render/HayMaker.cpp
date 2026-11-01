// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/ColorMap.h"
#include "hayStack/TransferFunction.h"
#include "viewer/render/HayMaker.h"
#include "viewer/render/AnariDeviceRenderer.h"

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

  HayMaker::HayMaker(Comm::SP world,
                     GlobalRenderSettings &globalRenderSettings,
                     hs::LocalPartitions *localPartitions,
                     const std::vector<DeviceConfig> &deviceConfigs)
    : world(world),
      globalRenderSettings(globalRenderSettings),
      localPartitions(localPartitions)
  {
    // computeWorldBounds();
    
    // ------------------------------------------------------------------
    // initialize anari *library* - need to load before we can even
    // create device(s)
    // ------------------------------------------------------------------
    char *envlib = getenv("ANARI_LIBRARY");
    std::string libname = envlib ? "environment" : "barney";
    library = anari::loadLibrary(libname.c_str(), anariStatusFunc);
    if (!library)
      throw std::runtime_error("could not create anari library '"+libname+"' - bailing out");

    // ------------------------------------------------------------------
    // create anari *device(s)*, and tether them - do NOT yet set the
    // partition, this is only allowed after all devices are fully
    // tethered
    // ------------------------------------------------------------------
    assert(!deviceConfigs.empty());
    for (int devIdx=0;devIdx<deviceConfigs.size();devIdx++) {
      auto dc = deviceConfigs[devIdx];
      int gpuID       = dc.gpuID;
      int tetherIndex = devIdx;
      int tetherCount = deviceConfigs.size();
      OnePartition *partition
        = localPartitions->get(dc.localPartitionIndex);
      perDevice.push_back(new AnariDeviceRenderer(gpuID,
                                                  tetherIndex,
                                                  tetherCount,
                                                  this,
                                                  partition));
    }
    
    // ------------------------------------------------------------------
    // all device(s) created and thethered; can now set their
    // partition data and let them create their world etc.
    // ------------------------------------------------------------------
    for (auto dev : perDevice)
      dev->initAnariGlobals();
    
    // ------------------------------------------------------------------
    // other global inits
    // ------------------------------------------------------------------

    // BoundsData bb = worldBounds;//getWorldBounds();
    BoundsData bb = getWorldBounds();
    if (!bb.mapped.empty()) {
      hs::ColorMapLibrary::init();
      int cmID = globalRenderSettings.defaultColorMapIndex;
      std::cout << "#hs: using scalar-mapping color map #" << cmID
                << " : " << hs::ColorMapLibrary::getMapName(cmID) << std::endl;
      for (auto pd : perDevice)
        pd->createDefaultColorMapper(bb.mapped,ColorMapLibrary::getMap(cmID));
    }
  }

  void HayMaker::resize(const vec2i &fbSize,
                        uint32_t *hostColor,
                        float *hostDepth)
  {
    this->fbSize = fbSize;
    this->hostColor = hostColor;
    this->hostDepth = hostDepth;
    for (auto dev : perDevice) {
      auto device = dev->anari.device;
      auto frame = dev->anari.frame;
      dev->fbSize = fbSize;
      anari::setParameter(device, frame,
                          "size",
                          (const anari::math::uint2&)fbSize);
      anari::setParameter(device, frame,
                          "channel.color",
                          ANARI_UFIXED8_RGBA_SRGB);
      if (hostDepth)
        anari::setParameter(device, frame,
                            "channel.depth", ANARI_FLOAT32);
#ifdef TEST_IDCHANNEL
      anari::setParameter(device, frame,
                          TEST_IDCHANNEL, ANARI_UINT32);
#endif

      anari::commitParameters(device, frame);
    }
  }

  BoundsData HayMaker::getWorldBounds()
  {
    BoundsData bb = localPartitions->getBounds();

    bb.spatial.lower = world->allReduceMin(bb.spatial.lower);
    bb.spatial.upper = world->allReduceMax(bb.spatial.upper);
    bb.scalars.lower = world->allReduceMin(bb.scalars.lower);
    bb.scalars.upper = world->allReduceMax(bb.scalars.upper);
    bb.mapped.lower = world->allReduceMin(bb.mapped.lower);
    bb.mapped.upper = world->allReduceMax(bb.mapped.upper);
    
    if (bb.spatial.empty()) {
      bb.spatial = {vec3f(-1.f),vec3f(+1.f)};
    }  
    return bb;
  }
  
  void HayMaker::renderFrame()
  {
    assert(!perDevice.empty());
    if (perDevice[0]->anari.world == 0)
      renderInitialAnariWorld();
    
    for (auto dev : perDevice)
      dev->renderFrame();
    
    auto dev0 = perDevice[0];
    auto fb_color = anari::map<uint32_t>(dev0->anari.device,
                                         dev0->anari.frame,
                                         "channel.color");
    auto fb_depth = anari::map<float>(dev0->anari.device,
                                      dev0->anari.frame,
                                      "channel.depth");
    

    if (fb_color.width != fbSize.x || fb_color.height != fbSize.y)
      std::cout << "resized frame or unsupported channel type!?" << std::endl;
    else {
      if (hostColor) 
        memcpy(hostColor,fb_color.data,fbSize.x*fbSize.y*sizeof(uint32_t));
      if (hostDepth)
        memcpy(hostDepth,fb_depth.data,fbSize.x*fbSize.y*sizeof(float));
    }
    anari::unmap(dev0->anari.device,dev0->anari.frame,"channel.color");
    anari::unmap(dev0->anari.device,dev0->anari.frame,"channel.depth");
  }
  
  void HayMaker::resetAccumulation()
  {
    for (auto dev : perDevice)
      anari::commitParameters(dev->anari.device, dev->anari.frame);
  }

  void HayMaker::setCamera(const Camera &camera) 
  {
    for (auto dev : perDevice)
      dev->setCamera(camera); 
  }
  
  void HayMaker::finalizeRender()
  {
    for (auto dev : perDevice) {
      anari::setParameter(dev->anari.device, dev->anari.frame,
                          "world", dev->anari.world);
      anari::commitParameters(dev->anari.device, dev->anari.frame);
    }
  }
  
  void HayMaker::renderInitialAnariWorld()
  {
    for (auto dev : perDevice)
      dev->renderInitialAnariWorld();
  }

  void HayMaker::setTransferFunction(const hs::TransferFunction &xf)
  {
    for (auto dev : perDevice)
      dev->setTransferFunction(xf);
  }

  void HayMaker::setEmissionTransferFunction(const hs::TransferFunction &xf)
  {
    for (auto dev : perDevice)
      dev->setEmissionTransferFunction(xf);
  }

  void HayMaker::setVolumeScatterSettings(const hs::VolumeScatterSettings &settings)
  {
    for (auto dev : perDevice)
      dev->setVolumeScatterSettings(settings);
  }
  
}

