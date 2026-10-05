// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/LocalModel.h"

namespace hm {

  struct PerDevice {
      
    PerDevice(PerRank *perRank,
              int mySlotIndex,
              int const cudaDevice)
      : textureLibrary(this),
        materialLibrary(this),
        cudaDevice(cudaDevice)
    {}

    void applyTransferFunction(const TransferFunction &xf);
#if HS_USE_MULTI_SCATTERING
    void applyVolumeScatterSettings(const VolumeScatterSettings &settings);
#endif
    // void setTransferFunction(const std::vector<VolumeHandle> &volumes,
    //                          const TransferFunction &xf);
      
    anari::Light create(const mini::QuadLight &ml) { return {}; }
    anari::Light create(const mini::DirLight &ml);
    anari::Light create(const mini::EnvMapLight &ml);
      
    anari::Group createGroup(const std::vector<anari::Surface> &geoms,
                             const std::vector<anari::Volume> &volumes);

    std::pair<anari::Material,std::string>create(mini::Plastic::SP plastic);
    std::pair<anari::Material,std::string>create(mini::Velvet::SP velvet);
    std::pair<anari::Material,std::string>create(mini::Matte::SP matte);
    std::pair<anari::Material,std::string>create(mini::Metal::SP metal);
    std::pair<anari::Material,std::string>create(mini::ThinGlass::SP thinGlass);
    std::pair<anari::Material,std::string>create(mini::Dielectric::SP dielectric);
    std::pair<anari::Material,std::string>create(mini::MetallicPaint::SP metallicPaint);
    std::pair<anari::Material,std::string>create(mini::DisneyMaterial::SP disney);
    std::pair<anari::Material,std::string>create(mini::Material::SP miniMat);
    std::pair<anari::Material,std::string>create(mini::ANARIMaterial::SP disney);
      
    anari::Volume create(const TAMRVolume::SP &v);
    anari::Volume create(const StructuredVolume::SP &v);
#if HS_USE_MULTI_SCATTERING
    anari::Volume create(const NanoVDBVolume::SP &v);
#endif
    anari::Volume create(const std::pair<umesh::UMesh::SP,box3f> &v);

    // void setColorMapping(anari::Material mat, const std::string &colorName);
    // void setScalarMapping(anari::Material mat, const std::string &colorName);
      
    std::vector<anari::Surface>
    createSpheres(SphereSet::SP content,
                  MaterialLibrary<AnariBackend> *materialLib);
      
    std::vector<anari::Surface>
    createCylinders(Cylinders::SP content,
                    MaterialLibrary<AnariBackend> *materialLib);
    std::vector<anari::Surface>
    createTriangleMesh(TriangleMesh::SP content,
                       MaterialLibrary<AnariBackend> *materialLib);

    std::vector<anari::Surface>
    createCapsules(hs::Capsules::SP caps,
                   MaterialLibrary<AnariBackend> *materialLib);
      
    void setInstances(const std::vector<anari::Group> &groups,
                      const std::vector<affine3f> &xfms);
    void setLights(anari::Group rootGroup,
                   const std::vector<anari::Light> &lights);
      
    anari::Sampler create(mini::Texture::SP miniTex);
    GeomHandle create(mini::Mesh::SP miniMesh,
                      MaterialLibrary<AnariBackend> *materialLib);

    inline void release(anari::Sampler t) { anari::release(device, t); }
    inline void release(anari::Material m) { anari::release(device, m); }
      
    void finalizeSlot() { PING; }

    // void createColorMapper(const range1f &inputRange,
    //                        const std::vector<vec3f> &colors);
      
      
    anari::Sampler  scalarMapper = 0;
    anari::Frame    frame  = 0;
    anari::Renderer renderer = 0;
    anari::World    model  = 0;
    anari::Camera   camera = 0;
      
    anari::Device   device = 0;

    PerRank *const perRank;
    int const cudaDevice;
    int const dataGroupRank;
    int const dataGroupSize;
    
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
