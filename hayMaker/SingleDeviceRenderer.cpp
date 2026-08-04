// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/SingleDeviceRenderer.h"
#include "hayMaker/HayMaker.h"

namespace hm {
  using namespace hs;
  
  SingleDeviceRenderer::SingleDeviceRenderer(int gpuID,
                                             int tetherIndex,
                                             int tetherCount,
                                             HayMaker     *hayMaker,
                                             OnePartition *myPartition)
    : hayMaker(hayMaker),
      myPartition(myPartition),
      textureLibrary(this),
      materialLibrary(this)
  {
    std::cout << "#hanari: creating tethered device #"
              << tetherIndex << "/" << tetherCount
              << " on gpu ID #" << gpuID
              << " and w/ data rank "
              << myPartition->partitionsRank
              << "/" << myPartition->partitionsCount
              << std::endl;
    
    
    anari.device = anari::newDevice(hayMaker->library, "default");
    anari::setParameter(anari.device, anari.device,
                        "tetherIndex", (int)tetherIndex);
    anari::setParameter(anari.device, anari.device,
                        "tetherCount", (int)tetherCount);
    anari::setParameter(anari.device, anari.device,
                        "cudaDevice", (int)gpuID);
    anari::setParameter(anari.device, anari.device,
                        "dataGroupID", (int)myPartition->partitionsRank);
    if (tetherIndex > 0) {
      anari::setParameter(anari.device, anari.device,
                          "tetherDevice",
                          // (uint64_t)
                          hayMaker->perDevice[0]->anari.device);
    }
    anari::commitParameters(anari.device, anari.device);
    
    anari.world = anari::newObject<anari::World>(anari.device);
    anari::commitParameters(anari.device, anari.world);
  
    anari.renderer = anari::newObject<anari::Renderer>(anari.device, "default");

    anari::setParameter(anari.device, anari.renderer,
                        "ambientRadiance",
                        hayMaker->globalRenderSettings.ambientRadiance);
    anari::setParameter(anari.device, anari.renderer,
                        "pixelSamples",
                        hayMaker->globalRenderSettings.samplesPerPixel);
#if HS_USE_MULTI_SCATTERING
    anari::setParameter(anari.device, anari.renderer,
                        "volumeMultiScatter", (bool)true);
    anari::setParameter(anari.device, anari.renderer,
                        "maxVolumeBounces", 8);
#endif
    auto &bgColor = hayMaker->globalRenderSettings.bgColor;
    if (isnan(bgColor.x) ||
        bgColor.x < 0.f) { 
      std::vector<vec4f> bgGradient = {
        vec4f(.9f,.9f,.9f,1.f),
        vec4f(0.15f, 0.25f, .8f,1.f),
      };
      anari::setAndReleaseParameter
        (anari.device, anari.renderer,
         "background",
         anari::newArray2D(anari.device,
                           (const anari::math::float4*)bgGradient.data(),
                           1,2));
    } else {
      anari::setParameter
        (anari.device, anari.renderer,"background",
         (const anari::math::float4 &)bgColor);
    }
    anari::commitParameters(anari.device, anari.renderer);

    anari.frame = anari::newObject<anari::Frame>(anari.device);
    anari::setParameter(anari.device, anari.frame, "world",    anari.world);
    anari::setParameter(anari.device, anari.frame, "renderer", anari.renderer);
    anari::setParameter(anari.device, anari.frame, "denoise",  (bool)true);

    anari.camera = anari::newObject<anari::Camera>(anari.device, "perspective");

    anari::setParameter(anari.device, anari.frame, "camera",   anari.camera);
    anari::commitParameters(anari.device, anari.frame);
  }
  
  void SingleDeviceRenderer::setTransferFunction(const TransferFunction &xf)
  {
    currentXF = xf;
    if (rootInstances.groups.empty()) {
      dirty = true;
      return;
    }
#if HS_USE_MULTI_SCATTERING
    if (isUnsetTransferFunctionDomain(xf.domain)
        && !rootVolumes.empty()
        && principledScatterByVolume.size() == rootVolumes.size())
      return;
#endif
      
    if (rootVolumes.empty())
      return;
    
    // auto device = device;
    // auto world = global->world;
    
    for (auto vol : rootVolumes) {
#if HS_USE_MULTI_SCATTERING
      auto principledIt = principledScatterByVolume.find(vol);
      const bool isPrincipled = principledIt != principledScatterByVolume.end();
      if (isPrincipled && isUnsetTransferFunctionDomain(xf.domain))
        continue;
#else
      const bool isPrincipled = false;
#endif
// #if 1
      int N = xf.colorMap.size();
      auto colorArray = anari::newArray1D(anari.device,ANARI_FLOAT32_VEC3,N);
      auto alphaArray = anari::newArray1D(anari.device,ANARI_FLOAT32,N);
      vec3f *colors = (vec3f*)anariMapArray(anari.device,colorArray);
      float *alphas = (float*)anariMapArray(anari.device,alphaArray);
      for (int i=0;i<N;i++) {
        auto c = xf.colorMap[i];
        colors[i] = vec3f(c.x,c.y,c.z);
        alphas[i] = c.w;
      }
      anariUnmapArray(anari.device,colorArray);
      anariUnmapArray(anari.device,alphaArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"color",colorArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"opacity",alphaArray);

      float unitDist = powf(1.05f,xf.baseDensity - 100.f);
      PRINT(xf.baseDensity);
      PRINT(unitDist);
      anari::setParameter(anari.device, vol,
                          "unitDistance",
                          unitDist
                          // xf.baseDensity
                          );
      range1f valueRange = xf.domain;
#if HS_USE_MULTI_SCATTERING
      if (isPrincipled && isUnsetTransferFunctionDomain(valueRange))
        valueRange = {0.f, 1.f};
#endif
      anariSetParameter(anari.device, vol, "valueRange",
                        ANARI_FLOAT32_BOX1,
                        &valueRange.lower);

      anari::commitParameters(anari.device, vol);
    }
  }
  
}
