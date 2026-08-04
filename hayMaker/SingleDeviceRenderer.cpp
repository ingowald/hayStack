// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/SingleDeviceRenderer.h"
#include "hayMaker/HayMaker.h"
#include "hayStack/ColorMap.h"

namespace hm {
  using namespace hs;

  inline float average(vec3f v) { return (v.x+v.y+v.z)/3.f; }
  
  SingleDeviceRenderer::SingleDeviceRenderer(int gpuID,
                                             int tetherIndex,
                                             int tetherCount,
                                             HayMaker     *hayMaker,
                                             OnePartition *myPartition)
    : hayMaker(hayMaker),
      myPartition(myPartition),
      textureLibrary(this),
      materialLibrary(this)
  {
    std::cout << "#hanari: creating tethered device #"
              << tetherIndex << "/" << tetherCount
              << " on gpu ID #" << gpuID
              << " and w/ data rank "
              << myPartition->partitionsRank
              << "/" << myPartition->partitionsCount
              << std::endl;
    
    
    anari.device = anari::newDevice(hayMaker->library, "default");
    anari::setParameter(anari.device, anari.device,
                        "tetherIndex", (int)tetherIndex);
    anari::setParameter(anari.device, anari.device,
                        "tetherCount", (int)tetherCount);
    anari::setParameter(anari.device, anari.device,
                        "cudaDevice", (int)gpuID);
    anari::setParameter(anari.device, anari.device,
                        "dataGroupID", (int)myPartition->partitionsRank);
    if (tetherIndex > 0) {
      anari::setParameter(anari.device, anari.device,
                          "tetherDevice",
                          // (uint64_t)
                          hayMaker->perDevice[0]->anari.device);
    }
    anari::commitParameters(anari.device, anari.device);
    
    anari.world = anari::newObject<anari::World>(anari.device);
    anari::commitParameters(anari.device, anari.world);
  
    anari.renderer = anari::newObject<anari::Renderer>(anari.device, "default");

    anari::setParameter(anari.device, anari.renderer,
                        "ambientRadiance",
                        hayMaker->globalRenderSettings.ambientRadiance);
    anari::setParameter(anari.device, anari.renderer,
                        "pixelSamples",
                        hayMaker->globalRenderSettings.samplesPerPixel);
#if HS_USE_MULTI_SCATTERING
    anari::setParameter(anari.device, anari.renderer,
                        "volumeMultiScatter", (bool)true);
    anari::setParameter(anari.device, anari.renderer,
                        "maxVolumeBounces", 8);
#endif
    auto &bgColor = hayMaker->globalRenderSettings.bgColor;
    if (isnan(bgColor.x) ||
        bgColor.x < 0.f) { 
      std::vector<vec4f> bgGradient = {
        vec4f(.9f,.9f,.9f,1.f),
        vec4f(0.15f, 0.25f, .8f,1.f),
      };
      anari::setAndReleaseParameter
        (anari.device, anari.renderer,
         "background",
         anari::newArray2D(anari.device,
                           (const anari::math::float4*)bgGradient.data(),
                           1,2));
    } else {
      anari::setParameter
        (anari.device, anari.renderer,"background",
         (const anari::math::float4 &)bgColor);
    }
    anari::commitParameters(anari.device, anari.renderer);

    anari.frame = anari::newObject<anari::Frame>(anari.device);
    anari::setParameter(anari.device, anari.frame, "world",    anari.world);
    anari::setParameter(anari.device, anari.frame, "renderer", anari.renderer);
    anari::setParameter(anari.device, anari.frame, "denoise",  (bool)true);

    anari.camera = anari::newObject<anari::Camera>(anari.device, "perspective");

    anari::setParameter(anari.device, anari.frame, "camera",   anari.camera);
    anari::commitParameters(anari.device, anari.frame);
  }
  
  void SingleDeviceRenderer::setTransferFunction(const TransferFunction &xf)
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
      
    if (rootVolumes.empty())
      return;
    
    // auto device = device;
    // auto world = global->world;
    
    for (auto vol : rootVolumes) {
#if HS_USE_MULTI_SCATTERING
      auto principledIt = principledScatterByVolume.find(vol);
      const bool isPrincipled = principledIt != principledScatterByVolume.end();
      if (isPrincipled && isUnsetTransferFunctionDomain(xf.domain))
        continue;
#else
      const bool isPrincipled = false;
#endif
      // #if 1
      int N = xf.colorMap.size();
      auto colorArray = anari::newArray1D(anari.device,ANARI_FLOAT32_VEC3,N);
      auto alphaArray = anari::newArray1D(anari.device,ANARI_FLOAT32,N);
      vec3f *colors = (vec3f*)anariMapArray(anari.device,colorArray);
      float *alphas = (float*)anariMapArray(anari.device,alphaArray);
      for (int i=0;i<N;i++) {
        auto c = xf.colorMap[i];
        colors[i] = vec3f(c.x,c.y,c.z);
        alphas[i] = c.w;
      }
      anariUnmapArray(anari.device,colorArray);
      anariUnmapArray(anari.device,alphaArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"color",colorArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"opacity",alphaArray);

      float unitDist = powf(1.05f,xf.baseDensity - 100.f);
      PRINT(xf.baseDensity);
      PRINT(unitDist);
      anari::setParameter(anari.device, vol,
                          "unitDistance",
                          unitDist
                          // xf.baseDensity
                          );
      range1f valueRange = xf.domain;
#if HS_USE_MULTI_SCATTERING
      if (isPrincipled && isUnsetTransferFunctionDomain(valueRange))
        valueRange = {0.f, 1.f};
#endif
      anariSetParameter(anari.device, vol, "valueRange",
                        ANARI_FLOAT32_BOX1,
                        &valueRange.lower);

      anari::commitParameters(anari.device, vol);
    }
  }
  
  anari::Group SingleDeviceRenderer
  ::createGroup(const std::vector<anari::Surface> &geoms,
                const std::vector<anari::Volume>  &volumes)
  {
    anari::Group meshGroup
      = anari::newObject<anari::Group>(anari.device);
    anari::setParameterArray1D(anari.device, meshGroup,
                               "surface", geoms.data(),geoms.size());
    anari::setParameterArray1D(anari.device, meshGroup,
                               "volume", volumes.data(),volumes.size());
    anari::commitParameters(anari.device, meshGroup);
    return meshGroup;
  }

  anari::Group SingleDeviceRenderer::render(const mini::Object::SP &object)
  {
    std::vector<anari::Surface> meshes;
    for (auto mesh : object->meshes) {
      auto handle = create(mesh);
      if (handle) meshes.push_back(handle);
    }
    return this->createGroup(meshes,{});
  }


  void SingleDeviceRenderer::renderInitialAnariWorld()
  {
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
    auto &myData = *myPartition;
    for (auto miniScene : myData.minis)
      renderMiniScene(miniScene);
    
    // ------------------------------------------------------------------
    // render all spheres
    // -----------------------------------------------------------------
    for (auto content : myData.sphereSets)
      for (auto created : create(*content))
        rootGeoms.push_back(created);
    
    for (auto content : myData.capsuleSets)
      for (auto created : create(*content))
        rootGeoms.push_back(created);
    
    // ------------------------------------------------------------------
    // render all cylinders
    // -----------------------------------------------------------------
    for (auto content : myData.cylinderSets)
      for (auto created : create(*content))
        rootGeoms.push_back(created);
    
    // ------------------------------------------------------------------
    // render all individual meshes
    // -----------------------------------------------------------------
    for (auto content : myData.triangleMeshes) {
      auto created = create(*content);
      auto meshGroup = createGroup(created,{});
      rootInstances.groups.push_back(meshGroup);
      affine3f xfm;
      rootInstances.xfms.push_back((const affine3f&)xfm);
    }
    
    // ------------------------------------------------------------------
    // render all structured volumes
    // -----------------------------------------------------------------
    for (auto vol : myData.structuredVolumes) {
      anari::Volume createdVolume = create(*vol);
      if (createdVolume)
        rootVolumes.push_back(createdVolume);
    }
#if HS_USE_MULTI_SCATTERING
    for (auto vol : myData.nanovdbVolumes) {
      anari::Volume createdVolume = create(*vol);
      if (createdVolume) {
        rootVolumes.push_back(createdVolume);
        principledScatterByVolume[createdVolume] = vol->scatter;
      }
    }
#endif
    // ------------------------------------------------------------------
    // render all *UN*-structured volumes
    // -----------------------------------------------------------------
    for (auto vol : myData.unsts) {
      anari::Volume createdVolume = create(vol);
      if (createdVolume)
        rootVolumes.push_back(createdVolume);
    }
    // ------------------------------------------------------------------
    // render all *AMR* volumes
    // -----------------------------------------------------------------
    for (auto vol : myData.amr) {
      anari::Volume createdVolume = create(*vol);
      if (createdVolume)
        rootVolumes.push_back(createdVolume);
    }
    // ==================================================================
    // now that all light and instances have been _created_ and
    // appended to the respective arrays, add these to the model
    // ==================================================================
    
    rootGroup = createGroup(rootGeoms,{});
    rootInstances.groups.push_back(rootGroup);
    rootInstances.xfms.push_back(affine3f{});
    
    // 'attach' the lights to the root group
    setLights(rootGroup,lights);
    
    // attach volumes to instances
    volumeGroup = createGroup({},rootVolumes);
    
    rootInstances.groups.push_back(volumeGroup);
    rootInstances.xfms.push_back(affine3f{});
      
    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // -----------------------------------------------------------------

    // #if HS_USE_MULTI_SCATTERING
    //     const bool skipTfApply =
    //       isUnsetTransferFunctionDomain(currentXF.domain)
    //       && !rootVolumes.empty()
    //       && principledScatterByVolume.size() == rootVolumes.size();
    // #else
    //     const bool skipTfApply = false;
    // #endif
    //     if (!skipTfApply)
    applyTransferFunction(currentXF);
    
    setInstances(rootInstances.groups,rootInstances.xfms);
  }

  void SingleDeviceRenderer
  ::setInstances(const std::vector<anari::Group> &groups,
                 const std::vector<affine3f> &xfms)
  {
    // auto device = device;
    // auto model  = global->model;
    std::vector<anari::Instance> instances;
    for (int i=0;i<groups.size();i++) {
      anari::Instance inst
        = anari::newObject<anari::Instance>(anari.device,"transform");
      
      anari::setParameter(anari.device, inst, "id", i);
      anari::setParameter(anari.device, inst, "group", groups[i]);

      // vec3f rc = randomColor(i);
      // anari::math::float4 instColor(rc.x,rc.y,rc.z,1.f);
      // anari::setParameter(anari.device, inst, "color", instColor);

      const affine3f xfm = xfms[i];

      anari::math::mat4 axf = anari::math::identity;
      axf[0].x = xfm.l.vx.x;
      axf[0].y = xfm.l.vx.y;
      axf[0].z = xfm.l.vx.z;
      axf[1].x = xfm.l.vy.x;
      axf[1].y = xfm.l.vy.y;
      axf[1].z = xfm.l.vy.z;
      axf[2].x = xfm.l.vz.x;
      axf[2].y = xfm.l.vz.y;
      axf[2].z = xfm.l.vz.z;
      axf[3].x = xfm.p.x;
      axf[3].y = xfm.p.y;
      axf[3].z = xfm.p.z;
      anari::setParameter(anari.device, inst, "transform", axf);
      anari::commitParameters(anari.device, inst);
      instances.push_back(inst);
    }
    anari::setAndReleaseParameter
      (anari.device,
       anari.world,
       "instance",
       anari::newArray1D(anari.device,
                         instances.data(),instances.size()));
    anari::commitParameters(anari.device, anari.world);    
  }

  void SingleDeviceRenderer
  ::setLights(anari::Group rootGroup,
              const std::vector<anari::Light> &lights)
  {
    if (!lights.empty()) {
      anari::setParameterArray1D
        (anari.device, anari.world,
         "light", lights.data(),lights.size());
    }
    anari::commitParameters(anari.device,anari.world);
  }

  std::vector<anari::Surface>
  SingleDeviceRenderer::create(const hs::Cylinders &content)
  {
    bool hasColors = content.colors.size();
      
    anari::Material material
      = materialLibrary.getOrCreate(content.material,hasColors);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(anari.device, "cylinder");
    anari::setParameterArray1D
      (anari.device, geom, "vertex.position",
       (const anari::math::float3*)content.vertices.data(),
       content.vertices.size());
    if (!content.indices.empty()) {
      anari::setParameterArray1D
        (anari.device, geom, "primitive.index",
         (const anari::math::uint2*)content.indices.data(),
         content.indices.size());
    }
    if (content.radii.empty()) {
      std::vector<float> radii;
      for (int i=0;i<content.vertices.size();i++)
        radii.push_back((float)content.radius);
      anari::setParameterArray1D
        (anari.device, geom, "primitive.radius",
         (const float*)radii.data(),
         radii.size());
    } else {
      anari::setParameterArray1D
        (anari.device, geom, "primitive.radius",
         (const float*)content.radii.data(),
         content.radii.size());
    }

    if (hasColors) {
      if (!content.colors.empty()) {
        std::vector<vec4f> color;
        for (auto col : content.colors)
          color.push_back(vec4f(col.x,col.y,col.z,1.f));
        if (color.size() == content.vertices.size()) {
          anari::setParameterArray1D
            (anari.device, geom, "vertex.color",
             (const anari::math::float4*)color.data(),
             color.size());
        } else {
          anari::setParameterArray1D
            (anari.device, geom, "primitive.color",
             (const anari::math::float4*)color.data(),
             color.size());
        }
      }
    }
    
    anari::commitParameters(anari.device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(anari.device);
    anari::setAndReleaseParameter(anari.device, surface, "geometry", geom);
    anari::setParameter(anari.device, surface, "material", material);
    anari::commitParameters(anari.device, surface);

    return { surface };
  }
  
  
  std::vector<anari::Surface>
  SingleDeviceRenderer::create(const hs::SphereSet &content)
  {
    bool hasColor = !content.colors.empty();
    anari::Material material
      = materialLibrary.getOrCreate(content.material,hasColor);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(anari.device, "sphere");
    anari::setParameterArray1D
      (anari.device, geom, "vertex.position",
       (const anari::math::float3*)content.origins.data(),
       content.origins.size());
    if (!content.colors.empty()) {
      anari::setParameterArray1D
        (anari.device, geom, "vertex.color",
         (const anari::math::float3*)content.colors.data(),
         content.origins.size());
    }
    if (content.radii.empty()) {
      anari::setParameter(anari.device,geom,"radius",(float)content.radius);
    } else {
      anari::setParameterArray1D
        (anari.device, geom, "vertex.radius",
         (const float*)content.radii.data(),
         content.radii.size());
    }

    anari::commitParameters(anari.device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(anari.device);
    anari::setAndReleaseParameter(anari.device, surface, "geometry", geom);
    anari::setParameter(anari.device, surface, "material", material);
    anari::commitParameters(anari.device, surface);

    return { surface };
  }

  void SingleDeviceRenderer::applyTransferFunction(const TransferFunction &xf)
  {
    if (rootVolumes.empty())
      return;

    for (auto vol : rootVolumes) {
#if HS_USE_MULTI_SCATTERING
      auto principledIt = principledScatterByVolume.find(vol);
      const bool isPrincipled = principledIt != principledScatterByVolume.end();
      if (isPrincipled && isUnsetTransferFunctionDomain(xf.domain))
        continue;
#else
      const bool isPrincipled = false;
#endif
      int N = xf.colorMap.size();
      auto colorArray = anari::newArray1D(anari.device,ANARI_FLOAT32_VEC3,N);
      auto alphaArray = anari::newArray1D(anari.device,ANARI_FLOAT32,N);
      vec3f *colors = (vec3f*)anariMapArray(anari.device,colorArray);
      float *alphas = (float*)anariMapArray(anari.device,alphaArray);
      for (int i=0;i<N;i++) {
        auto c = xf.colorMap[i];
        colors[i] = vec3f(c.x,c.y,c.z);
        alphas[i] = c.w;
      }
      anariUnmapArray(anari.device,colorArray);
      anariUnmapArray(anari.device,alphaArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"color",colorArray);
      anari::setAndReleaseParameter
        (anari.device,vol,"opacity",alphaArray);

      float unitDist = powf(1.05f,xf.baseDensity - 100.f);
      PRINT(xf.baseDensity);
      PRINT(unitDist);
      anari::setParameter(anari.device, vol,
                          "unitDistance",
                          unitDist
                          // xf.baseDensity
                          );
      range1f valueRange = xf.domain;
      if (isPrincipled && isUnsetTransferFunctionDomain(valueRange))
        valueRange = {0.f, 1.f};
      anariSetParameter(anari.device, vol, "valueRange",
                        ANARI_FLOAT32_BOX1,
                        &valueRange.lower);

      anari::commitParameters(anari.device, vol);
    }
  }
  
  void SingleDeviceRenderer::renderMiniScene(mini::Scene::SP mini)
  {
    // ------------------------------------------------------------------
    // set light(s) for given mini scene
    // ------------------------------------------------------------------
    for (auto ml : mini->quadLights)
      createAndAdd(ml);
    for (auto dl : mini->dirLights)
      createAndAdd(dl);
    if (mini->envMapLight)
      createAndAdd(*mini->envMapLight);

    // ------------------------------------------------------------------
    // render all (possibly instanced) triangle meshes from mini format
    // ------------------------------------------------------------------
    std::map<mini::Object::SP, anari::Group> miniGroups;
    for (auto inst : mini->instances) {
      if (!miniGroups[inst->object])
        miniGroups[inst->object] = render(inst->object);
      if (miniGroups[inst->object]) {
        rootInstances.groups.push_back(miniGroups[inst->object]);
        rootInstances.xfms.push_back((const affine3f&)inst->xfm);
      }
    }
  }

  
  void SingleDeviceRenderer::createAndAdd(const mini::QuadLight &ml)
  { return; }
  
  void SingleDeviceRenderer::createAndAdd(const mini::DirLight &ml)
  {
    anari::Light light = anari::newObject<anari::Light>
      (anari.device,"directional");
    assert(light);
    anari::setParameter(anari.device,light,"direction",(const anari::math::float3&)ml.direction);
    anari::setParameter(anari.device,light,"irradiance",2.f*average(ml.radiance));
    anari::commitParameters(anari.device,light);
    lights.push_back(light);
  }
        
  void SingleDeviceRenderer::createAndAdd(const mini::EnvMapLight &ml)
  {
    std::cout << MINI_TERMINAL_YELLOW
              << "#hs: creating env-map light ..."
              << MINI_TERMINAL_DEFAULT << std::endl;
    // auto device = device;
    vec2i size = ml.texture->size;
    anari::Array2D radiance
      = anariNewArray2D(anari.device, nullptr,nullptr,nullptr,
                        ANARI_FLOAT32_VEC3,
                        (size_t)size.x,(size_t)size.y);
    vec3f *as3f = (vec3f*)anariMapArray(anari.device,radiance);
    for (int i=0;i<size.x*size.y;i++) {
      as3f[i]
        = (const vec3f&)((vec4f*)ml.texture->data.data())[i];
    }
    anariUnmapArray(anari.device,radiance);
    anari::commitParameters(anari.device,radiance);
    
    anari::Light light = anari::newObject<anari::Light>(anari.device,"hdri");
    anari::setAndReleaseParameter(anari.device,light,"radiance",radiance);
    vec3f up = ml.transform.l.vz;
    vec3f dir = - ml.transform.l.vx;

    std::cout << "setting HDRI orientation dir = " << dir << ", up = " << up << std::endl;
    anari::setParameter(anari.device,light,"up",
                        (const anari::math::float3&)up);
    anari::setParameter(anari.device,light,"direction",
                        (const anari::math::float3&)dir);
    anari::setParameter(anari.device,light,"scale",1.f);
    anari::commitParameters(anari.device,light);
    lights.push_back(light);
  }

  anari::Volume SingleDeviceRenderer::create(const StructuredVolume &vol)
  {
    anari::math::int3 volumeDims = (const anari::math::int3&)vol.dims;
      
    auto field = anari::newObject<anari::SpatialField>
      (anari.device, "structuredRegular");
    anari::setParameter(anari.device, field, "origin",
                        (const anari::math::float3&)vol.gridOrigin);
    anari::setParameter(anari.device, field, "spacing",
                        (const anari::math::float3&)vol.gridSpacing);
    if (vol.texelFormat == "float") {
      anari::setParameterArray3D
        (anari.device, field, "data", (const float *)vol.rawData.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else if (vol.texelFormat == "uint8_t") {
      anari::setParameterArray3D
        (anari.device, field, "data", (const uint8_t *)vol.rawData.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else if (vol.texelFormat == "uint16_t") {
      std::cout << "volume with uint16s, converting to float" << std::endl;
      static std::vector<float> volumeAsFloats(vol.rawData.size()/2);
      for (size_t i=0;i<volumeAsFloats.size();i++)
        volumeAsFloats[i] = ((uint16_t*)vol.rawData.data())[i]
          * (1.f/((1<<16)-1));
      anari::setParameterArray3D
        (anari.device, field, "data", (const float *)volumeAsFloats.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else {
      throw std::runtime_error("un-supported scalar type in haymaker"
                               " structured volume");
    }
        
    anari::commitParameters(anari.device, field);

    auto volume = anari::newObject<anari::Volume>
      (anari.device, "transferFunction1D");
    anari::setAndReleaseParameter(anari.device, volume,
                                  "value", field);
    anari::commitParameters(anari.device, volume);

    return volume;
  }

  void SingleDeviceRenderer
  ::createDefaultColorMapper(const range1f &inputRange,
                                     const std::vector<vec4f> &colorMap)
  {
    if (inputRange.empty()) return;
    
    anari::Sampler scalarMapper
      = anari::newObject<anari::Sampler>(anari.device,"image1D");
    anari::setParameterArray1D(anari.device, scalarMapper, "image",
                               (const anari::math::float4*)colorMap.data(),
                               colorMap.size());
    float scale = 1.f / (inputRange.upper-inputRange.lower);
    struct {
      vec4f v0,v1,v2,v3;
    } xfm;
    xfm.v0={scale,0.f,0.f,-inputRange.lower/scale};
    xfm.v1={0.f,1.f,0.f,0.f};
    xfm.v2={0.f,0.f,1.f,0.f};
    xfm.v3={0.f,0.f,0.f,1.f};
    anariSetParameter(anari.device,scalarMapper,
                      "inTransform",
                      ANARI_FLOAT32_MAT4,
                      &xfm);
    anari::setParameter(anari.device,scalarMapper,
                        "inAttribute","attribute1");
    anari::setParameter(anari.device,scalarMapper,
                        "filter","linear");
    anari::commitParameters(anari.device,scalarMapper);
    std::cout << "color mapper created" << std::endl;
    defaultColorMapper = scalarMapper;
  }

  anari::Surface
  SingleDeviceRenderer::create(const mini::Mesh::SP &miniMesh)
  {
    anari::Material material
      = materialLibrary.getOrCreate(miniMesh->material);
    anari::Geometry mesh
      = anari::newObject<anari::Geometry>(anari.device, "triangle");
    anari::setParameterArray1D(anari.device, mesh, "vertex.position",
                               (const anari::math::float3*)miniMesh->vertices.data(),
                               miniMesh->vertices.size());
    anari::setParameterArray1D(anari.device, mesh, "primitive.index",
                               (const anari::math::uint3*)miniMesh->indices.data(),
                               miniMesh->indices.size());
    if (!miniMesh->texcoords.empty())
      if (miniMesh->texcoords.size() == miniMesh->vertices.size()) {
        anari::setParameterArray1D(anari.device, mesh, "vertex.attribute0",
                                   (const anari::math::float2*)miniMesh->texcoords.data(),
                                   miniMesh->texcoords.size());
      } else if (miniMesh->texcoords.size() == 3*miniMesh->indices.size()) {
        anari::setParameterArray1D(anari.device, mesh, "faceVarying.attribute0",
                                   (const anari::math::float2*)miniMesh->texcoords.data(),
                                   miniMesh->texcoords.size());
      } else  {
        PING;
        PRINT(miniMesh->texcoords.size());
        PRINT(miniMesh->vertices.size());
        PRINT(miniMesh->indices.size());
      }
#if 1
    if (!miniMesh->normals.empty()) {
      if (miniMesh->normals.size() == miniMesh->vertices.size()) {
        anari::setParameterArray1D(anari.device, mesh, "vertex.normal",
                                   (const anari::math::float3*)miniMesh->normals.data(),
                                   miniMesh->normals.size());
      } else if (miniMesh->normals.size() == 3*miniMesh->indices.size()) {
        anari::setParameterArray1D(anari.device, mesh, "faceVarying.normal",
                                   (const anari::math::float3*)miniMesh->normals.data(),
                                   miniMesh->normals.size());
      } else  {
        PING;
        PRINT(miniMesh->normals.size());
        PRINT(miniMesh->vertices.size());
        PRINT(miniMesh->indices.size());
      }
    }
#endif
    anari::commitParameters(anari.device, mesh);

    anari::Surface  surface = anari::newObject<anari::Surface>(anari.device);
    anari::setAndReleaseParameter(anari.device, surface, "geometry", mesh);
    anari::setParameter(anari.device, surface, "material", material);
    anari::commitParameters(anari.device, surface);

    return surface;
  }
  
  std::vector<anari::Surface>
  SingleDeviceRenderer::create(const hs::TriangleMesh &content)
  {
    bool colorMapped = content.colors.size();
    
    anari::Sampler scalarMapper
      = content.scalars.perVertex.empty()
      ? anari::Sampler{}
      : defaultColorMapper;
      // will become null if perVertex is empty
    anari::Material material
      = materialLibrary.getOrCreate(content.material,colorMapped,scalarMapper);
    // ,
    //                              content.scalars.perVertex.size()>0);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(anari.device, "triangle");
    anari::setParameterArray1D
      (anari.device, geom, "vertex.position",
       (const anari::math::float3*)content.vertices.data(),
       content.vertices.size());
    if (!content.normals.empty()) {
      if (content.normals.size() == content.vertices.size()) {
        anari::setParameterArray1D
          (anari.device, geom, "vertex.normal",
           (const anari::math::float3*)content.normals.data(),
           content.normals.size());
      } else  if (content.normals.size() == 3*content.indices.size()) {
        anari::setParameterArray1D
          (anari.device, geom, "faceVarying.normal",
           (const anari::math::float3*)content.normals.data(),
           content.normals.size());
      } else {
        PING;
        PRINT(content.normals.size());
        PRINT(content.vertices.size());
        PRINT(content.indices.size());
      }
    }
    anari::setParameterArray1D
      (anari.device, geom, "primitive.index",
       (const anari::math::uint3*)content.indices.data(),
       content.indices.size());
    if (!content.colors.empty()) {
      anari::setParameterArray1D
        (anari.device, geom, "vertex.color",
         (const anari::math::float3*)content.colors.data(),
         content.colors.size());
    }
    if (!content.scalars.perVertex.empty()) {
      anari::setParameterArray1D
        (anari.device, geom, "vertex.attribute1",
         (const float*)content.scalars.perVertex.data(),
         content.scalars.perVertex.size());
    }
          
    anari::commitParameters(anari.device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(anari.device);
    anari::setAndReleaseParameter(anari.device, surface, "geometry", geom);
    anari::setParameter(anari.device, surface, "material", material);
    anari::commitParameters(anari.device, surface);

    return {surface};
  }
  
  std::vector<anari::Surface>
  SingleDeviceRenderer::create(const hs::Capsules &caps)
  {
    bool hasColorAttribute = caps.colors.size()>0;
    anari::Material material
      = materialLibrary.getOrCreate(caps.material,hasColorAttribute);
    std::vector<vec3f> position;
    std::vector<float> radius;
    std::vector<vec4f> color;
    std::vector<uint32_t> index;
    for (auto idx : caps.indices) {
      index.push_back(position.size());
      position.push_back((const vec3f&)caps.vertices[idx.x]);
      position.push_back((const vec3f&)caps.vertices[idx.y]);
      radius.push_back(caps.vertices[idx.x].w);
      radius.push_back(caps.vertices[idx.y].w);
      if (!caps.colors.empty()) {
        color.push_back(caps.colors[idx.x]);
        color.push_back(caps.colors[idx.y]);
      }
    }
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(anari.device, "curve");
    anari::setParameterArray1D
      (anari.device, geom, "vertex.position",
       (const anari::math::float3*)position.data(),
       position.size());
    anari::setParameterArray1D
      (anari.device, geom, "vertex.radius",
       (const float*)radius.data(),
       radius.size());
    anari::setParameterArray1D
      (anari.device, geom, "primitive.index",
       (const uint32_t*)index.data(),
       index.size());
    if (!caps.colors.empty()) {
      anari::setParameterArray1D
        (anari.device, geom, "vertex.color",
         (const anari::math::float4*)color.data(),
         color.size());
    }
    anari::commitParameters(anari.device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(anari.device);
    anari::setAndReleaseParameter(anari.device, surface, "geometry", geom);
    anari::setParameter(anari.device, surface, "material", material);
    anari::commitParameters(anari.device, surface);

    return { surface };
  }
  
  anari::Volume SingleDeviceRenderer
  ::create(const TAMRVolume &input)
  {
    std::cout << "skipping amr volume ..." << std::endl;
    return 0;
  }
  
  anari::Volume SingleDeviceRenderer
  ::create(const std::pair<umesh::UMesh::SP,box3f> &meshAndDomain)
  {
    auto mesh = meshAndDomain.first;
    assert(mesh);

    auto field = anari::newObject<anari::SpatialField>(anari.device, "unstructured");

    anari::setParameterArray1D
      (anari.device, field, "vertex.position",
       (const anari::math::float3 *)mesh->vertices.data(),
       mesh->vertices.size());
    anari::setParameterArray1D
      (anari.device, field, "vertex.data",
       (const float *)mesh->perVertex->values.data(),
       mesh->perVertex->values.size());
    std::vector<uint8_t>  cellTypeData;
    std::vector<uint32_t> cellBeginData;
    std::vector<uint32_t> indexData;

    // this isn't fully spec'ed yet
    enum { _VTK_TET = 10, _VTK_HEX=12, _VTK_WEDGE=13, _VTK_PYR=14, _VTK_POLYHEDRON=42 };
    enum { _ANARI_TET = 0, _ANARI_HEX=1, _ANARI_WEDGE=2, _ANARI_PYR=3 };
    for (auto prim : mesh->tets) {
      cellTypeData.push_back(_VTK_TET);
      cellBeginData.push_back((uint32_t)indexData.size());
      for (int i=0;i<prim.numVertices;i++)
        indexData.push_back(prim[i]);
    }
    for (auto prim : mesh->pyrs) {
      cellTypeData.push_back(_VTK_PYR);
      cellBeginData.push_back((uint32_t)indexData.size());
      for (int i=0;i<prim.numVertices;i++)
        indexData.push_back(prim[i]);
    }
    for (auto prim : mesh->wedges) {
      cellTypeData.push_back(_VTK_WEDGE);
      cellBeginData.push_back((uint32_t)indexData.size());
      for (int i=0;i<prim.numVertices;i++)
        indexData.push_back(prim[i]);
    }
    for (auto prim : mesh->hexes) {
      cellTypeData.push_back(_VTK_HEX);
      cellBeginData.push_back((uint32_t)indexData.size());
      for (int i=0;i<prim.numVertices;i++)
        indexData.push_back(prim[i]);
    }
    for (int i=0;i<(int)mesh->polyOffsets.size();i++) {
      cellTypeData.push_back(_VTK_POLYHEDRON);
      cellBeginData.push_back((uint32_t)indexData.size());
      // copy this polyhedron's face stream into the index array
      int streamOfs = mesh->polyOffsets[i];
      int numFaces = mesh->polyFaceStream[streamOfs];
      int pos = streamOfs;
      int streamEnd;
      if (i+1 < (int)mesh->polyOffsets.size())
        streamEnd = mesh->polyOffsets[i+1];
      else
        streamEnd = (int)mesh->polyFaceStream.size();
      for (int j = pos; j < streamEnd; j++)
        indexData.push_back(mesh->polyFaceStream[j]);
    }
    
    anari::setParameterArray1D
      (anari.device, field, "cell.type",
       (const uint8_t *)cellTypeData.data(),
       cellTypeData.size());
    anari::setParameterArray1D
      (anari.device, field, "cell.index",
       (const uint32_t *)cellBeginData.data(),
       cellBeginData.size());
    anari::setParameterArray1D
      (anari.device, field, "index",
       (const uint32_t *)indexData.data(),
       indexData.size());

    anari::commitParameters(anari.device, field);
    
    auto volume = anari::newObject<anari::Volume>(anari.device, "transferFunction1D");
    anari::setAndReleaseParameter(anari.device, volume, "value", field);
    anari::commitParameters(anari.device, volume);

    return volume;
  }

  
}
