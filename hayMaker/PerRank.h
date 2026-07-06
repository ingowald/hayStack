// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/LocalModel.h"

namespace hm {

    struct PerDevice {
      
    PerDevice(PerRank *perRank,
           int mySlotIndex, int localDataRankThisSlotUses)
      : Backend::Slot(global,mySlotIndex,localDataRankThisSlotUses,this),
        textureLibrary(this),
        materialLibrary(this)
    {}

    struct {
      std::vector<affine3f>    xfms;
      std::vector<GroupHandle> groups;
    } rootInstances;
    GroupHandle rootGroup;

    std::vector<VolumeHandle> rootVolumes;
    std::vector<GeomHandle>   rootGeoms;
#if HS_USE_MULTI_SCATTERING
    std::unordered_map<VolumeHandle, VolumeScatterParams> principledScatterByVolume;
#endif
      
    LightHandle envLight;
    std::vector<LightHandle> lights;

    GroupHandle volumeGroup = 0;

    void setTransferFunction(const TransferFunction &xf)
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
      this->applyTransferFunction(xf);
    }
      
    void renderAll();
    void renderMiniScene(mini::Scene::SP miniScene);
    GroupHandle render(const mini::Object::SP &miniObject);
    void render(const mini::QuadLight &ml);
    void render(const mini::DirLight &ml);
    void render(const mini::EnvMapLight::SP &ml);
      
    TextureLibrary<Backend>  textureLibrary;
    MaterialLibrary<Backend> materialLibrary;

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
  };
    

}
