// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "HayMaker.h"

namespace hs {

  struct AnariBackend {
    typedef anari::Material MaterialHandle;
    typedef anari::Sampler  TextureHandle;
    typedef anari::Group    GroupHandle;
    typedef anari::Light    LightHandle;
    typedef anari::Surface  GeomHandle;
    typedef anari::Volume   VolumeHandle;

    struct Global {
      Global(HayMaker *base);

      void init();
      void resize(const vec2i &fbSize, uint32_t *hostRgba);
      void renderFrame(int pathsPerPixel);
      void resetAccumulation();
      void setCamera(const Camera &camera);
      void finalizeRender();

      HayMaker *const base;

      anari::Device device = 0;
      anari::Frame  frame  = 0;
      anari::World  model  = 0;
      anari::Camera camera = 0;
      uint32_t *hostRGBA   = 0;
      vec2i        fbSize;
    };

    struct Slot {
      Slot(Global *global, int slot,
           typename HayMakerT<AnariBackend>::Slot *impl)
        : global(global), slot(slot), impl(impl) {}
    
      void setTransferFunction(const std::vector<VolumeHandle> &volumes,
                               const TransferFunction &xf);
      
      anari::Light create(const mini::QuadLight &ml) { return {}; }
      anari::Light create(const mini::DirLight &ml);
      anari::Light create(const mini::EnvMapLight &ml);
      
      anari::Group createGroup(const std::vector<anari::Surface> &geoms,
                               const std::vector<anari::Volume> &volumes);

      anari::Material create(mini::Plastic::SP plastic);
      anari::Material create(mini::Velvet::SP velvet);
      anari::Material create(mini::Matte::SP matte);
      anari::Material create(mini::Metal::SP metal);
      anari::Material create(mini::ThinGlass::SP thinGlass);
      anari::Material create(mini::Dielectric::SP dielectric);
      anari::Material create(mini::MetallicPaint::SP metallicPaint);
      anari::Material create(mini::DisneyMaterial::SP disney);
      anari::Material create(mini::Material::SP miniMat);

      anari::Volume create(const StructuredVolume::SP &v);
      anari::Volume create(const std::pair<umesh::UMesh::SP,box3f> &v);

      std::vector<anari::Surface>
      createSpheres(SphereSet::SP content,
                    MaterialLibrary<AnariBackend> *materialLib);
      
      std::vector<anari::Surface>
      createCylinders(Cylinders::SP content);

      std::vector<anari::Surface>
      createCapsules(hs::Capsules::SP caps,
                     MaterialLibrary<AnariBackend> *materialLib) { return {}; }
      
      void setInstances(const std::vector<anari::Group> &groups,
                        const std::vector<affine3f> &xfms);
      void setLights(anari::Group rootGroup,
                     const std::vector<anari::Light> &lights);
      
      anari::Sampler create(mini::Texture::SP miniTex);
      GeomHandle create(mini::Mesh::SP miniMesh,
                        MaterialLibrary<AnariBackend> *materialLib);

      inline void release(anari::Sampler t) { anari::release(global->device, t); }
      inline void release(anari::Material m) { anari::release(global->device, m); }
      
      void finalizeSlot() { PING; }
      
      typename HayMakerT<AnariBackend>::Slot *const impl;
      Global *const global;
      int     const slot;
      // std::vector<anari::Volume> rootVolumes;
      // anari::Group volumeGroup = 0;
    };

    // void buildDataGroup(int dgID);
    
    // struct PerDG {
    // };
    // std::vector<PerDG> perDG;
  };
  
} // ::hs
