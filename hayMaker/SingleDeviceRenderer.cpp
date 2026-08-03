// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/SingleDeviceRenderer.h"

namespace hm {

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
    int dataRank
    std::cout << "#hanari: creating tethered device #"
              << tetherIndex << "/" << tetherCount
              << " on gpu ID #" << gpuID
              << " and w/ data rank "
              << myPartition->dataRank << "/" << myPartition->dataSize
              << std::endl;
    device = anari::newDevice(library, "default");
    anari::setParameter(device, device,
                        "tetherIndex", (int)devIdx);
    anari::setParameter(device, device,
                        "tetherCount", (int)numGPUs);
    anari::setParameter(device, device,
                        "cudaDevice", (int)gpuID);
        
    auto &dg = base->localModel.dataGroups[slot];
    anari::setParameter(device, device,
                        "dataGroupID", (int)dg.dataGroupID);
    if (devIdx > 0) {
      anari::setParameter(device, device,
                          "tetherDevice",
                          // (uint64_t)
                          devices[0]);
    }
    anari::commitParameters(device, device);
    
    model = anari::newObject<anari::World>(device);
    anari::commitParameters(device, model);
  
    auto rendererObj = anari::newObject<anari::Renderer>(device, "default");

    anari::setParameter(device, rendererObj, "ambientRadiance",
                        global->base->ambientRadiance
                        );
    anari::setParameter(device, rendererObj, "pixelSamples", global->base->pixelSamples);
#if HS_USE_MULTI_SCATTERING
    anari::setParameter(device, rendererObj, "volumeMultiScatter", (bool)true);
    anari::setParameter(device, rendererObj, "maxVolumeBounces", 8);
#endif
    if (isnan(global->base->bgColor.x) || global->base->bgColor.x < 0.f) { 
      std::vector<vec4f> bgGradient = {
        vec4f(.9f,.9f,.9f,1.f),
        vec4f(0.15f, 0.25f, .8f,1.f),
      };
      anari::setAndReleaseParameter
        (device,rendererObj,"background",
         anari::newArray2D(device,
                           (const anari::math::float4*)bgGradient.data(),
                           1,2));
    } else {
      anari::setParameter
        (device,rendererObj,"background",
         (const anari::math::float4 &)global->base->bgColor);
    }
    anari::commitParameters(device, rendererObj);

    renderer = rendererObj;
    frame = anari::newObject<anari::Frame>(device);
    anari::setParameter(device, frame, "world",    model);
    anari::setParameter(device, frame, "renderer", renderer);
    anari::setParameter(device, frame, "denoise",  (bool)true);

    this->camera = anari::newObject<anari::Camera>(device, "perspective");

    anari::setParameter(device, frame, "camera",   camera);
    anari::commitParameters(device, frame);
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
    // auto model = global->model;
    
    for (auto vol : rootVolumes) {
#if HS_USE_MULTI_SCATTERING
      auto principledIt = principledScatterByVolume.find(vol);
      const bool isPrincipled = principledIt != principledScatterByVolume.end();
      if (isPrincipled && isUnsetTransferFunctionDomain(xf.domain))
        continue;
#else
      const bool isPrincipled = false;
#endif
#if 1
      int N = xf.colorMap.size();
      auto colorArray = anari::newArray1D(device,ANARI_FLOAT32_VEC3,N);
      auto alphaArray = anari::newArray1D(device,ANARI_FLOAT32,N);
      vec3f *colors = (vec3f*)anariMapArray(device,colorArray);
      float *alphas = (float*)anariMapArray(device,alphaArray);
      for (int i=0;i<N;i++) {
        auto c = xf.colorMap[i];
        colors[i] = vec3f(c.x,c.y,c.z);
        alphas[i] = c.w;
      }
      anariUnmapArray(device,colorArray);
      anariUnmapArray(device,alphaArray);
      anari::setAndReleaseParameter
        (device,vol,"color",colorArray);
      anari::setAndReleaseParameter
        (device,vol,"opacity",alphaArray);
#else
      std::vector<anari::math::float3> colors;
      std::vector<float> opacities;

      for (int i=0;i<xf.colorMap.size();i++) {
        auto c = xf.colorMap[i];
        colors.emplace_back(c.x,c.y,c.z);
        opacities.emplace_back(c.w);
      }
      anari::setAndReleaseParameter
        (device,vol,"color",
         anari::newArray1D(device, colors.data(), colors.size()));
      anari::setAndReleaseParameter
        (device,vol,"opacity",
         anari::newArray1D(device, opacities.data(), opacities.size()));
#endif
      // float unitDist
      //   = 100.f/xf.baseDensity;
      // unitDist
      //   = (xf.baseDensity >= 100)
      //   ? (1.f/(xf.baseDensity-99))
      //   : powf(1.03f,100-xf.baseDensity);
       
      // PRINT(unitDist);
      // > 100
      //   ? (1.f/(xf.baseDensity-100))

      float unitDist = powf(1.05f,xf.baseDensity - 100.f);
      PRINT(xf.baseDensity);
      PRINT(unitDist);
      anari::setParameter(device, vol,
                          "unitDistance",
                          unitDist
                          // xf.baseDensity
                          );
      range1f valueRange = xf.domain;
#if HS_USE_MULTI_SCATTERING
      if (isPrincipled && isUnsetTransferFunctionDomain(valueRange))
        valueRange = {0.f, 1.f};
#endif
      anariSetParameter(device, vol, "valueRange",
                        ANARI_FLOAT32_BOX1,
                        &valueRange.lower);

      anari::commitParameters(device, vol);
    }
  }
  
}
