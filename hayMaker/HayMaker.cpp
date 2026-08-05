// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/ColorMap.h"
#include "hayStack/TransferFunction.h"
#include "hayMaker/HayMaker.h"
#include "hayMaker/AnariDeviceRenderer.h"

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
    // ------------------------------------------------------------------
    // initialize anari *library* - need to load before we can even
    // create device(s)
    // ------------------------------------------------------------------
    char *envlib = getenv("ANARI_LIBRARY");
    std::string libname = envlib ? "environment" :
#if HS_MPI
      "barney_mpi"
#else
      "barney"
#endif
      ;
    library = anari::loadLibrary(libname.c_str(), anariStatusFunc);
    if (!library)
      throw std::runtime_error("could not create anari library '"+libname+"' - bailing out");

    // ------------------------------------------------------------------
    // create anari *device(s)*
    // ------------------------------------------------------------------
    assert(!deviceConfigs.empty());
    for (int devIdx=0;devIdx<deviceConfigs.size();devIdx++) {
      auto dc = deviceConfigs[devIdx];
      // devicerender params:
      //   int gpuID,
      //   int tetherIndex,
      //   int tetherCount,
      //   HayMaker     *hayMaker,
      //   OnePartition *myPartition
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
    // other global inits
    // ------------------------------------------------------------------

    BoundsData bb = getWorldBounds();
    if (!bb.mapped.empty()) {
      hs::ColorMap::init();
      int cmID = globalRenderSettings.defaultColorMapIndex
        % hs::ColorMap::maps.size();
      std::cout << "#hs: using scalar-mapping color map #" << cmID
                << " : " << hs::ColorMap::maps[cmID].first << std::endl;
      for (auto pd : perDevice)
        pd->createDefaultColorMapper(bb.mapped,
                                     hs::ColorMap::maps[cmID].second);
    }
  }

  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    for (auto dev : perDevice) {
      auto device = dev->anari.device;
      auto frame = dev->anari.frame;
      anari::setParameter(device, frame,
                          "size",
                          (const anari::math::uint2&)fbSize);
      anari::setParameter(device, frame,
                          "channel.color",
                          ANARI_UFIXED8_RGBA_SRGB);
      static bool have_depth = getenv("HS_HAVE_DEPTH");
      if (have_depth)
        anari::setParameter(device, frame,
                            "channel.depth", ANARI_FLOAT32);
#ifdef TEST_IDCHANNEL
      anari::setParameter(device, frame,
                          TEST_IDCHANNEL, ANARI_UINT32);
#endif

      anari::commitParameters(device, frame);
    }
  }
  

  void HayMaker::renderFrame()
  {
    assert(!perDevice.empty());
    if (perDevice[0]->anari.world == 0)
      renderInitialAnariWorld();
    
    const char *channelName = "channel.color";
#ifdef TEST_IDCHANNEL
    channelName = TEST_IDCHANNEL;
#endif
    
    for (auto dev : perDevice)
      dev->renderFrame();

    auto dev0 = perDevice[0];
    auto fb = anari::map<uint32_t>(dev0->anari.device,
                                   dev0->anari.frame,
                                   channelName);

    if (fb.width != fbSize.x || fb.height != fbSize.y)
      std::cout << "resized frame or unsupported channel type!?" << std::endl;
    else {
      PING; PRINT(hostRGBA);
      if (hostRGBA) {
#ifdef TEST_IDCHANNEL
        const uint64_t FNV_basis = 0xcbf29ce484222325ULL;
        const uint64_t FNV_prime = 0x100000001b3ULL;
        for (int i=0;i<fb.width*fb.height;i++) {
          uint32_t ID = fb.data[i];
          uint64_t s = FNV_basis + FNV_prime * ID;
          
          s = s * FNV_prime ^ ID;
          int r = s & 0xff;
          s = s * FNV_prime ^ ID;
          int g = s & 0xff;
          s = s * FNV_prime ^ ID;
          int b = s & 0xff;
          uint32_t rgba = b<<0 | g<<8 | r<<16 | 0xff<<24;
          hostRGBA[i] = rgba;
        }
#else
        memcpy(hostRGBA,fb.data,fbSize.x*fbSize.y*sizeof(uint32_t));
#endif
      }
    }
    anari::unmap(dev0->anari.device,dev0->anari.frame,channelName);
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

  void HayMaker::renderInitialAnariWorld()
  {
    PING;
    for (auto dev : perDevice)
      dev->renderInitialAnariWorld();
  }
  
}

