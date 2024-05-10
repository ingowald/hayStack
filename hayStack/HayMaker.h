// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
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

/*! the hay-*maker* is the actual render for hay-stack type data,
    using barney */

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/DataGroup.h"
#include "hayStack/MPIRenderer.h"
#if HANARI
# include <anari/anari_cpp.hpp>
# include "anari/anari_cpp/ext/linalg.h"
#else
# include "barney.h"
#endif

namespace hs {

  struct HayMaker : public Renderer {
    BoundsData getWorldBounds() const;
    
    HayMaker(Comm &world,
             Comm &workers,
             ThisRankData &thisRankData,
             bool verbose);

    virtual void buildSlots() = 0;
    
    Comm        &world;
    Comm         workers;
    ThisRankData rankData;
    bool         verbose;
  };
  
  template<typename Backend>
  struct TextureLibrary
  {
    using TextureHandle = typename Backend::TextureHandle;
    
    TextureLibrary(typename Backend::Slot *backend);
    TextureHandle getOrCreate(mini::Texture::SP miniTex);
    
  private:
    TextureHandle create(mini::Texture::SP miniTex);
    typename Backend::Slot *const backend;
    std::map<mini::Texture::SP,TextureHandle> alreadyCreated;
  };
  
  template<typename Backend>
  struct MaterialLibrary {
    using MaterialHandle = typename Backend::MaterialHandle;
    
    MaterialLibrary(typename Backend::Slot *backend);
    ~MaterialLibrary();
    MaterialHandle getOrCreate(mini::Material::SP miniMat);

  private:
    std::map<mini::Material::SP,MaterialHandle> alreadyCreated;
    typename Backend::Slot *const backend;
  };
  
  
  template<typename Backend>
  struct HayMakerT : public HayMaker
  {
    HayMakerT(Comm &world,
             Comm &workers,
             ThisRankData &thisRankData,
             bool verbose);

    void init();

    void buildSlots() override;
    
    void resize(const vec2i &fbSize, uint32_t *hostRGBA) override
    { global.resize(fbSize,hostRGBA); }
    void setTransferFunction(const TransferFunction &xf) override
    {
      for (auto slot : perSlot)
        slot->setTransferFunction(xf);
    }
    void renderFrame(int pathsPerPixel) override
    { global.renderFrame(pathsPerPixel); }
    
    void resetAccumulation() override
    { global.resetAccumulation(); }
    
    void setCamera(const Camera &camera) override
    { global.setCamera(camera); }

    using Global = typename Backend::Global;

    Global              global;

    struct Slot : public Backend::Slot {
      using GroupHandle = typename Backend::GroupHandle;
      using LightHandle = typename Backend::LightHandle;
      using VolumeHandle = typename Backend::VolumeHandle;
      using GeomHandle = typename Backend::GeomHandle;
      
      Slot(Global *global, int slot)
        : Backend::Slot(global,slot,this),
          textureLibrary(this),
          materialLibrary(this)
      {}

      struct {
        std::vector<affine3f>    xfms;
        std::vector<GroupHandle> groups;
      } rootInstances;
      GroupHandle rootGroup;

      LightHandle envLight;
      std::vector<LightHandle> lights;

      // list of created volumes, so we can set transfer function
      std::vector<VolumeHandle> createdVolumes;
      GroupHandle volumeGroup = 0;

      void setTransferFunction(const TransferFunction &xf)
      { Backend::Slot::setTransferFunction(createdVolumes,xf); }
      
      void renderAll();
      void renderMiniScene(mini::Scene::SP miniScene);
      GroupHandle render(const mini::Object::SP &miniObject);
      void render(const mini::QuadLight &ml);
      void render(const mini::DirLight &ml);
      void render(const mini::EnvMapLight::SP &ml);
      
      TextureLibrary<Backend>  textureLibrary;
      MaterialLibrary<Backend> materialLibrary;
    };
    
    std::vector<Slot *> perSlot;
  };

  struct BarneyBackend {
    typedef BNMaterial MaterialHandle;
    typedef BNSampler  TextureHandle;
    typedef BNGroup    GroupHandle;
    typedef BNLight    LightHandle;
    typedef BNVolume   VolumeHandle;
    typedef BNGeom     GeomHandle;
    
    struct Global {
      Global(HayMaker *base);

      void init();
      void resize(const vec2i &fbSize, uint32_t *hostRgba);
      void renderFrame(int pathsPerPixel);
      void resetAccumulation();
      void setCamera(const Camera &camera);
      void finalizeRender();
      
      HayMaker *const base;
      BNContext     barney = 0;
      BNModel       model  = 0;
      BNFrameBuffer fb     = 0;
      BNCamera      camera = 0;
      vec2i        fbSize;
    };
    
    void resize(const vec2i &fbSize, uint32_t *hostRgba);
    void renderFrame(int pathsPerPixel);
    void resetAccumulation();
    void setCamera(const Camera &camera);
    

    // void buildDataGroup(int dgID);
    
    struct Slot {
      Slot(Global *global, int slot,
           typename HayMakerT<BarneyBackend>::Slot *impl)
        : global(global), slot(slot), impl(impl) {}
    
      void setTransferFunction(const std::vector<VolumeHandle> &volumes,
                               const TransferFunction &xf);

      BNLight create(const mini::QuadLight &ml);
      BNLight create(const mini::DirLight &ml);
      BNLight create(const mini::EnvMapLight &ml);

      BNGroup    createGroup(const std::vector<BNGeom> &geoms);
      BNMaterial create(mini::Plastic::SP plastic);
      BNMaterial create(mini::Velvet::SP velvet);
      BNMaterial create(mini::Matte::SP matte);
      BNMaterial create(mini::Metal::SP metal);
      BNMaterial create(mini::ThinGlass::SP thinGlass);
      BNMaterial create(mini::Dielectric::SP dielectric);
      BNMaterial create(mini::MetallicPaint::SP metallicPaint);
      BNMaterial create(mini::DisneyMaterial::SP disney);
      BNMaterial create(mini::Material::SP miniMat);

      BNSampler create(mini::Texture::SP miniTex);
      GeomHandle create(mini::Mesh::SP miniMesh,
                        MaterialLibrary<BarneyBackend> *materialLib);

      void setInstances(const std::vector<BNGroup> &groups,
                        const std::vector<affine3f> &xfms);
      void setLights(BNGroup rootGroup, const std::vector<BNLight> &lights);

      inline void release(BNSampler t) { bnRelease(t); }
      inline void release(BNMaterial m) { bnRelease(m); }

      typename HayMakerT<BarneyBackend>::Slot *const impl;
      Global *const global;
      int     const slot;
    };
  };

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
      anari::Light create(const mini::DirLight &ml) { return {}; }
      anari::Light create(const mini::EnvMapLight &ml) { return {}; }
      
      anari::Group createGroup(const std::vector<anari::Surface> &geoms);

      anari::Material create(mini::Plastic::SP plastic);
      anari::Material create(mini::Velvet::SP velvet);
      anari::Material create(mini::Matte::SP matte);
      anari::Material create(mini::Metal::SP metal);
      anari::Material create(mini::ThinGlass::SP thinGlass);
      anari::Material create(mini::Dielectric::SP dielectric);
      anari::Material create(mini::MetallicPaint::SP metallicPaint);
      anari::Material create(mini::DisneyMaterial::SP disney);
      anari::Material create(mini::Material::SP miniMat);

      void setInstances(const std::vector<anari::Group> &groups,
                        const std::vector<affine3f> &xfms);
      void setLights(anari::Group rootGroup, const std::vector<anari::Light> &lights);
      
      anari::Sampler create(mini::Texture::SP miniTex);
      GeomHandle create(mini::Mesh::SP miniMesh, MaterialLibrary<AnariBackend> *materialLib);

      inline void release(anari::Sampler t) { anari::release(global->device, t); }
      inline void release(anari::Material m) { anari::release(global->device, m); }
      
      
      typename HayMakerT<AnariBackend>::Slot *const impl;
      Global *const global;
      int     const slot;
      std::vector<anari::Volume> createdVolumes;
      anari::Group volumeGroup = 0;
    };

    // void buildDataGroup(int dgID);
    
    // struct PerDG {
    // };
    // std::vector<PerDG> perDG;
  };
  
}
