// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "HayMaker.h"
#if HS_USE_MULTI_SCATTERING
# include "hayStack/NanoVDBVolume.h"
#endif

namespace hs {
  namespace hm {

    struct PerDevice {
      PerDevice(PerRank *const perRank,
                int const cudaDevice,
                int const dataGroupRank,
                int const dataGroupSize);

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
      anari::Surface createAMRIsoSurface(const TAMRVolume::SP &v,
                                         MaterialLibrary<AnariBackend> *materialLib);
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
    };

    
  };
  
} // ::hs
