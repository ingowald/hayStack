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
#include "hayStack/LocalModel.h"
#include "hayStack/MPIRenderer.h"
#if HANARI
# include <anari/anari_cpp.hpp>
# include "anari/anari_cpp/ext/linalg.h"
#else
# include "barney/barney.h"
#endif

namespace hs {

  /*! base class for anything that can do (data-parallel) rendering of
      haystack data */
  struct HayMaker : public Renderer {
    BoundsData getWorldBounds() const;
    
    HayMaker(Comm &world,
             Comm &workers,
             int   pixelSamples,
             float ambientRadiance,
             vec4f bgColor,
             LocalModel &localModel,
             const std::vector<int> &gpuIDs,
             bool verbose);

    virtual void buildSlots() = 0;

    /*! creates a renderer that uses anari to render; will throw if
        anari support was not built in */
    static HayMaker *createAnariImplementation(Comm &world,
                                               Comm &workers,
                                               int pathsPerPixel,
                                               float ambientRadiance,
                                               vec4f bgColor,
                                               LocalModel &localModel,
                                               const std::vector<int> &gpuIDs,
                                               bool verbose);
    /*! creates a "native" barney renderer */
    static HayMaker *createBarneyImplementation(Comm &world,
                                                Comm &workers,
                                                int pathsPerPixel,
                                                float ambientRadiance,
                                                vec4f bgColor,
                                                LocalModel &localModel,
                                                const std::vector<int> &gpuIDs,
                                                bool verbose);

    Comm        &world;
    Comm         workers;
    LocalModel   localModel;
    bool         verbose;
    vec4f        bgColor { NAN, NAN, NAN, NAN };
    const float  ambientRadiance;
    const int    pixelSamples;
    const std::vector<int> gpuIDs;
  };
  
  /*! keeps track of which frontend textures have already been
      created on the backend, and returns handle to already created
      backend variant if it exists -- or creates one if this is not
      yet the case */
  template<typename Backend>
  struct TextureLibrary
  {
    using TextureHandle = typename Backend::TextureHandle;
    
    TextureLibrary(typename Backend::Slot *backend);
    TextureHandle getOrCreate(mini::Texture::SP miniTex);
    
  private:
    //TextureHandle create(mini::Texture::SP miniTex);
    typename Backend::Slot *const backend;
    std::map<mini::Texture::SP,TextureHandle> alreadyCreated;
  };

  /*! keeps track of which frontend materials have already been
      created on the backend, and returns handle to already created
      backend variant if it exists -- or creates one if this is not
      yet the case */
  template<typename Backend>
  struct MaterialLibrary {
    using MaterialHandle = typename Backend::MaterialHandle;
    
    MaterialLibrary(typename Backend::Slot *backend);
    ~MaterialLibrary();
    MaterialHandle getOrCreate(mini::Material::SP miniMat,
                               bool colorMapped = false,
                               bool scalarMapped = false);

  private:
    std::map<std::tuple<mini::Material::SP,bool,bool>,MaterialHandle> alreadyCreated;
    typename Backend::Slot *const backend;
  };
  
  /*! implementation of the haymaker 'api' using one or more
      (templated) funtions to do individual basic opertaion. In
      general, it is this class that maintains material and template
      libraries, does scene graph traversal, keeps track of which
      volumes have been created (to be able to assign a new transfer
      function), etc; then this class refers to the (template-param)
      "backend" to do individual operations like "create structured
      volume" or "create disney material", etc */
  template<typename Backend>
  struct HayMakerT : public HayMaker
  {
    HayMakerT(Comm &world,
              Comm &workers,
              int pathsPerPixel,
              float ambientRadiance,
              vec4f bgColor,
              LocalModel &localModel,
              const std::vector<int> &gpuIDs,
              bool verbose);
    
    //void init();
    void terminate() override { global.terminate(); }
    

    void buildSlots() override;
    
    void resize(const vec2i &fbSize, uint32_t *hostRGBA) override
    { global.resize(fbSize,hostRGBA); }
    
    void setTransferFunction(const TransferFunction &xf) override
    {
      for (auto slot : perSlot)
        slot->setTransferFunction(xf);
    }
    void renderFrame() override
    {
      buildSlots();
      global.renderFrame();
    }
    
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
      
      Slot(Global *global, int mySlotIndex, int localDataRankThisSlotUses)
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
      
      LightHandle envLight;
      std::vector<LightHandle> lights;

      GroupHandle volumeGroup = 0;

      void setTransferFunction(const TransferFunction &xf)
      {
        currentXF = xf;
        // Backend::Slot::setTransferFunction(rootVolumes,xf);
        dirty = true;
      }
      
      void renderAll();
      void renderMiniScene(mini::Scene::SP miniScene);
      GroupHandle render(const mini::Object::SP &miniObject);
      void render(const mini::QuadLight &ml);
      void render(const mini::DirLight &ml);
      void render(const mini::EnvMapLight::SP &ml);
      // std::vector<GeomHandle> render(SphereSet::SP content);
      // std::vector<GeomHandle> render(Cylinders::SP content);
      
      TextureLibrary<Backend>  textureLibrary;
      MaterialLibrary<Backend> materialLibrary;

      TransferFunction currentXF;
      bool dirty = true;
    };
    
    std::vector<Slot *> perSlot;
  };

}
