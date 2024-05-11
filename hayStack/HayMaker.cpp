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

#include "HayMaker.h"
#include "BarneyBackend.h"
#if HANARI
#include "AnariBackend.h"
#endif
// #include "hayStack/TransferFunction.h"
// #include <map>

namespace hs {

  HayMaker::HayMaker(Comm &world,
                     Comm &workers,
                     ThisRankData &thisRankData,
                     bool verbose)
    : world(world),
      workers(workers),
      rankData(std::move(thisRankData)),
      verbose(verbose)
  {}

  template<typename Backend>
  HayMakerT<Backend>::HayMakerT(Comm &world,
                                Comm &workers,
                                ThisRankData &thisRankData,
                                bool verbose)
    : HayMaker(world,workers,thisRankData,verbose),
      global(this)
  {
    for (int i=0;i<this->rankData.size();i++)
      perSlot.push_back(new Slot(&global,i));
  }

  BoundsData HayMaker::getWorldBounds() const
  {
    BoundsData bb = rankData.getBounds();
    bb.spatial.lower = world.allReduceMin(bb.spatial.lower);
    bb.spatial.upper = world.allReduceMax(bb.spatial.upper);
    bb.scalars.lower = world.allReduceMin(bb.scalars.lower);
    bb.scalars.upper = world.allReduceMax(bb.scalars.upper);
    return bb;
  }

  template<typename Backend>
  TextureLibrary<Backend>::TextureLibrary(typename Backend::Slot *backend)
    : backend(backend)
  {}

  template<typename Backend>
  typename Backend::TextureHandle
  TextureLibrary<Backend>::getOrCreate(mini::Texture::SP miniTex)
  {
    auto it = alreadyCreated.find(miniTex);
    if (it != alreadyCreated.end()) return it->second;

    auto bnTex = backend->create(miniTex);
    alreadyCreated[miniTex] = bnTex;
    return bnTex;
  }

  template<typename Backend>
  MaterialLibrary<Backend>::MaterialLibrary(typename Backend::Slot *backend)
    : backend(backend)
  {}

  template<typename Backend>
  MaterialLibrary<Backend>::~MaterialLibrary()
  {
    for (auto it : alreadyCreated)
      backend->release(it.second);
  }

  template<typename Backend>
  typename Backend::MaterialHandle
  MaterialLibrary<Backend>::getOrCreate(mini::Material::SP miniMat)
  {
    if (alreadyCreated.find(miniMat) != alreadyCreated.end())
      return alreadyCreated[miniMat];

    auto mat = backend->create(miniMat);
    alreadyCreated[miniMat] = mat;
    return mat;
  }

  template<typename Backend>
  void HayMakerT<Backend>::Slot::renderAll()
  {
    if (!rootInstances.groups.empty())
      // already rendererd ...
      return;
    auto impl = this->impl;

    // ==================================================================
    // first, "render" all content in the sense that we create
    // geometries, lights, instances, etc, and simply 'append' them to
    // two global lists for all lights and all instances, respectively
    // ==================================================================

    // ------------------------------------------------------------------
    // render all mini::Scene formatted geometry - however many there
    // may be; this also includes lights because those are currently
    // stored in mini::Scene'
    // -----------------------------------------------------------------
    auto &myData = this->global->base->rankData.dataGroups[this->slot];
    for (auto miniScene : myData.minis)
      renderMiniScene(miniScene);

    // ------------------------------------------------------------------
    // render all structured voluems
    // -----------------------------------------------------------------
    for (auto vol : myData.structuredVolumes) {
      VolumeHandle createdVolume = impl->create(vol);
      if (createdVolume) rootVolumes.push_back(createdVolume);
    }

    // ==================================================================
    // now that all light and instances have been _created_ and
    // appended to the respective arrays, add these to the model
    // ==================================================================

    rootGroup = impl->createGroup(rootGeoms,{});
    rootInstances.groups.push_back(rootGroup);
    rootInstances.xfms.push_back(affine3f{});

    
    // 'attach' the lights to the root group
    impl->setLights(rootGroup,lights);

    // attach volumes to instances
    
    volumeGroup = impl->createGroup({},rootVolumes);
    rootInstances.groups.push_back(volumeGroup);
    rootInstances.xfms.push_back(affine3f{});
    
    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // -----------------------------------------------------------------
    this->setInstances(rootInstances.groups,rootInstances.xfms);
  }

  template<typename Backend>
  void HayMakerT<Backend>::Slot::render(const mini::QuadLight &ml)
  {
    auto light = this->create(ml);
    if (light) lights.push_back(light);
  }

  template<typename Backend>
  void HayMakerT<Backend>::Slot::render(const mini::DirLight &ml)
  {
    auto light = this->create(ml);
    if (light) lights.push_back(light);
  }

  template<typename Backend>
  void HayMakerT<Backend>::Slot::render(const mini::EnvMapLight::SP &ml)
  {
    if (!ml) return;
    auto light = this->create(*ml);
    if (light) lights.push_back(light);
  }

  template<typename Backend>
  typename Backend::GroupHandle HayMakerT<Backend>::Slot::render(const mini::Object::SP &object)
  {
    std::vector<typename Backend::GeomHandle> meshes;
    for (auto mesh : object->meshes) {
      auto handle = this->create(mesh,&this->materialLibrary);
      if (handle) meshes.push_back(handle);
    }
    return this->createGroup(meshes,{});
  }


  template<typename Backend>
  void HayMakerT<Backend>::Slot::renderMiniScene(mini::Scene::SP mini)
  {
    // ------------------------------------------------------------------
    // set light(s) for given mini scene
    // ------------------------------------------------------------------
    for (auto ml : mini->quadLights)
      render(ml);
    for (auto dl : mini->dirLights)
      render(dl);
    if (mini->envMapLight)
      render(mini->envMapLight);

    // ------------------------------------------------------------------
    // render all (possibly instanced) triangle meshes from mini format
    // ------------------------------------------------------------------
    std::map<mini::Object::SP, GroupHandle> miniGroups;
    for (auto inst : mini->instances) {
      if (!miniGroups[inst->object])
        miniGroups[inst->object] = render(inst->object);
      if (miniGroups[inst->object]) {
        rootInstances.groups.push_back(miniGroups[inst->object]);
        rootInstances.xfms.push_back((const affine3f&)inst->xfm);
      }
    }
  }

  template<typename Backend>
  void HayMakerT<Backend>::buildSlots()
  {
    for (auto slot : perSlot)
      slot->renderAll();
    PING;
    global.finalizeRender();
  }

  HayMaker *HayMaker::createAnariImplementation(Comm &world,
                                                Comm &workers,
                                               ThisRankData &thisRankData,
                                                bool verbose)
  {
#if HANARI
    return new HayMakerT<AnariBackend>(world,
                                       /* the workers */workers,
                                       thisRankData,
                                       verbose);
#else
    throw std::runtime_error("ANARI support not compiled in");
#endif
  }
  
  HayMaker *HayMaker::createBarneyImplementation(Comm &world,
                                                Comm &workers,
                                                ThisRankData &thisRankData,
                                                bool verbose)
  {
    return new HayMakerT<BarneyBackend>(world,
                                        /* the workers */workers,
                                        thisRankData,
                                        verbose);
  }
  
  template struct HayMakerT<BarneyBackend>;
  template struct TextureLibrary<BarneyBackend>;
  template struct MaterialLibrary<BarneyBackend>;
#if HANARI
  template struct HayMakerT<AnariBackend>;
  template struct TextureLibrary<AnariBackend>;
  template struct MaterialLibrary<AnariBackend>;
#endif
}
