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

    struct Slot;

    static const int slotPerDevice = true; 
    static const int slotPerDataRank = false;

    struct Global {
      Global(HayMaker *base);

      void init();
      void resize(const vec2i &fbSize, uint32_t *hostRgba);
      void renderFrame();
      void resetAccumulation();
      void setCamera(const Camera &camera);
      void finalizeRender();
      /*! clean up and shut down */
      void terminate() {}
      
      HayMaker *const base;

      uint32_t     *hostRGBA   = 0;
      vec2i         fbSize;
      /*! whether we have to re-commit the model next frame */
      bool          dirty = true;

      /*! points to the first slot, which is the only slot in
          non-data-parallel, and the master slot in data-parallel */
      std::vector<Slot *> slots;
      inline int numDevices() const { return devices.size(); }
      std::vector<anari::Device> devices;
      // the library used to create the device(s)
      anari::Library library;
    };

    struct Slot {
      Slot(Global *global,
           int mySlotIndex,
           int localDataSlotWeWorkOn,
           typename HayMakerT<AnariBackend>::Slot *impl);
    
      void applyTransferFunction(const TransferFunction &xf);
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
      anari::Volume create(const std::pair<umesh::UMesh::SP,box3f> &v);

      void setColorMapping(anari::Material mat, const std::string &colorName);
      void setScalarMapping(anari::Material mat, const std::string &colorName);
      
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

      void createColorMapper(const range1f &inputRange,
                             const std::vector<vec3f> &colors);
      
      typename HayMakerT<AnariBackend>::Slot *const impl;
      Global *const global;
      // int     const slot;
      int const mySlotIndex;
      int const localDataSlotWeWorkOn;
      
      anari::Sampler scalarMapper = 0;
      anari::Frame  frame  = 0;
      anari::Device device = 0;
      anari::World  model  = 0;
      anari::Camera camera = 0;
    };

    
  };
  
} // ::hs
