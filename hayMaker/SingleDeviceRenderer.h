// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/OnePartition.h"
#include "hayMaker/TextureLibrary.h"
#include "hayMaker/MaterialLibrary.h"
#include "hayStack/TransferFunction.h"

namespace hm {
  using hs::OnePartition;
  using hs::TransferFunction;
  
  struct HayMaker;
  
  /*! implements rendering operations for one logical partition, on
    one single device */
  struct SingleDeviceRenderer {
      
    SingleDeviceRenderer(int gpuID,
                         int tetherIndex,
                         int tetherCount,
                         HayMaker     *hayMaker,
                         OnePartition *myPartition);

    struct {
      std::vector<affine3f>     xfms;
      std::vector<anari::Group> groups;
    } rootInstances;
    anari::Group rootGroup;

    std::vector<anari::Volume>   rootVolumes;
    std::vector<anari::Geometry> rootGeoms;
#if HS_USE_MULTI_SCATTERING
    std::map<anari::Volume, VolumeScatterParams> principledScatterByVolume;
#endif
      
    anari::Light envLight;
    std::vector<anari::Light> lights;

    anari::Group volumeGroup = 0;
    HayMaker     *const hayMaker;
    OnePartition *const myPartition;
    
    void setTransferFunction(const TransferFunction &xf);
      
    void renderAll();
    void renderMiniScene(mini::Scene::SP miniScene);
    anari::Group render(const mini::Object::SP &miniObject);
    void render(const mini::QuadLight &ml);
    void render(const mini::DirLight &ml);
    void render(const mini::EnvMapLight::SP &ml);
    void createColorMapper(range1f domain, const std::vector<vec3f> &values);
    
    TextureLibrary  textureLibrary;
    MaterialLibrary materialLibrary;

    TransferFunction currentXF;
#if HS_USE_MULTI_SCATTERING
    VolumeScatterSettings volumeScatterSettings;
    void setVolumeScatterSettings(const VolumeScatterSettings &settings)
    {
      volumeScatterSettings = settings;
      for (auto &entry : principledScatterByVolume)
        entry.second = settings.medium;
      this->applyVolumeScatterSettings(settings);
    }
#endif
    bool dirty = true;

    struct {
      anari::Device device;
      anari::World  world;
      anari::Renderer renderer;
      anari::Frame    frame;
      anari::Camera   camera;
    } anari;
  };
    

}
