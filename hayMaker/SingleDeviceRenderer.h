// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/OnePartition.h"
#include "hayMaker/TextureLibrary.h"
#include "hayMaker/MaterialLibrary.h"
#include "hayStack/TransferFunction.h"

namespace hs {
  struct NanoVDBVolume;
};

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
    void renderInitialAnariWorld();    
    void renderFrame();
    
    struct {
      std::vector<affine3f>     xfms;
      std::vector<anari::Group> groups;
    } rootInstances;
    anari::Group rootGroup;

    std::vector<anari::Volume>  rootVolumes;
    std::vector<anari::Surface> rootGeoms;
#if HS_USE_MULTI_SCATTERING
    std::map<anari::Volume, VolumeScatterParams> principledScatterByVolume;
#endif
      
    anari::Light envLight;
    std::vector<anari::Light> lights;

    anari::Group volumeGroup = 0;
    HayMaker     *const hayMaker;
    OnePartition *const myPartition;
    
    void setTransferFunction(const TransferFunction &xf);
      
    // void renderAll();
    void renderMiniScene(mini::Scene::SP miniScene);
    anari::Group render(const mini::Object::SP &miniObject);

    void applyTransferFunction(const TransferFunction &xf);

    /*! creates *default* color mapper that maps from global scalar
        min/max (across all per-vertex arrays and scalarfield types),
        to the chosen default color map */
    void createDefaultColorMapper(/*! input range across all objects,
                                    across all rankgs */
                                  const range1f &inRange,
                                  const std::vector<vec4f> &outColors);
    
    /*! default scalar to color mapper - this spans the total input
        range of all scalar fields in the model (globally reduced
        across all ranks), for the selected haystack's default color
        map. if input range is empty (ie, no scalar field or
        per-vertex scalar attributes this becomes 0 */
    anari::Sampler defaultColorMapper = 0;
    
    // ==================================================================
    // create() functions: create anari object for given piece of
    // haystack input //
    // ==================================================================
    
    anari::Group createGroup(const std::vector<anari::Surface> &geoms,
                             const std::vector<anari::Volume>  &volumes);
    anari::Volume create(const hs::StructuredVolume &vol);
    anari::Volume create(const hs::NanoVDBVolume &vol);
    anari::Volume create(const hs::TAMRVolume &input);
    
    anari::Volume
    create(const std::pair<umesh::UMesh::SP,box3f> &meshAndDomain);

    using Surfaces = std::vector<anari::Surface>;
    Surfaces create(const hs::SphereSet &content);
    Surfaces create(const hs::TriangleMesh &content);
    Surfaces create(const hs::Capsules &caps);
    Surfaces create(const hs::Cylinders &content);
    
    anari::Surface create(const mini::Mesh::SP &mesh);

    void createAndAdd(const mini::QuadLight &ml);
    void createAndAdd(const mini::DirLight &ml);
    void createAndAdd(const mini::EnvMapLight &ml);

    // anari::Sampler createScalarMapper(const std::vector<float> &scalars);
    

    
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


    void setInstances(const std::vector<anari::Group> &groups,
                      const std::vector<affine3f> &xfms);
    void setLights(anari::Group rootGroup,
                   const std::vector<anari::Light> &lights);
    
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
