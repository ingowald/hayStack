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

#include "HayMaker.h"
#include "hayStack/TransferFunction.h"
#include <map>

namespace hs {

#if HANARI
  static void statusFunc(const void * /*userData*/,
                         ANARIDevice /*device*/,
                         ANARIObject source,
                         ANARIDataType /*sourceType*/,
                         ANARIStatusSeverity severity,
                         ANARIStatusCode /*code*/,
                         const char *message)
  {
    if (severity == ANARI_SEVERITY_FATAL_ERROR) {
      fprintf(stderr, "[FATAL][%p] %s\n", source, message);
      std::exit(1);
    } else if (severity == ANARI_SEVERITY_ERROR) {
      fprintf(stderr, "[ERROR][%p] %s\n", source, message);
    } else if (severity == ANARI_SEVERITY_WARNING) {
      fprintf(stderr, "[WARN ][%p] %s\n", source, message);
    } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
      fprintf(stderr, "[PERF ][%p] %s\n", source, message);
    }
    // Ignore INFO/DEBUG messages
  }
#endif
  
  HayMaker::HayMaker(Comm &world,
                     Comm &workers,
                     ThisRankData &thisRankData,
                     bool verbose)
    : world(world),
      workers(workers),
      rankData(std::move(thisRankData)),
      isActiveWorker(!thisRankData.empty()),
      verbose(verbose)
  {
    perDG.resize(rankData.size());
  }

  void HayMaker::createBarney()
  {
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
#if HANARI
     
      char *envlib = getenv("ANARI_LIBRARY");
      std::string libname = envlib ? "environment" : "barney";
      auto library = anari::loadLibrary(libname.c_str(), statusFunc);
      device = anari::newDevice(library, "default");
      anari::commitParameters(device, device);
#else
#if HS_FAKE_MPI
      barney = bnContextCreate
      (   /*data*/dataGroupIDs.data(), (int)dataGroupIDs.size(),
          /*gpus*/nullptr, -1);
#else
      barney = bnMPIContextCreate
        (world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
         /*gpus*/nullptr,-1);
#endif
#endif
    } else {
#if HANARI
      throw std::runtime_error("passive master not yet implemented");
#else
#if HS_FAKE_MPI
        barney = bnContextCreate
        (/*data*/nullptr, 0,/*gpus*/nullptr, 0);
#else
      barney = bnMPIContextCreate
        (world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);
#endif
#endif
    }

#if HANARI
    model = anari::newObject<anari::World>(device);
    anari::commitParameters(device, model);
    
    auto renderer = anari::newObject<anari::Renderer>(device, "default");
    anari::setParameter(device, renderer, "ambientRadiance", 1.f);
    anari::commitParameters(device, renderer);
    
    frame = anari::newObject<anari::Frame>(device);
    // anari::setParameter(device, frame, "size", imgSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(device, frame, "world",    model);
    anari::setParameter(device, frame, "renderer", renderer);
    
    this->camera = anari::newObject<anari::Camera>(device, "perspective");
    
    anari::setParameter(device, frame, "camera",   camera);
    anari::commitParameters(device, frame);
#else
    fb = bnFrameBufferCreate(barney,0);
    model = bnModelCreate(barney);
    camera = bnCameraCreate(barney,"perspective");
#endif
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
  
  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
#if HANARI
    this->hostRGBA = hostRGBA;
    anari::setParameter(device, frame, "size", (const anari::math::uint2&)fbSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_VEC4);
    anari::setParameter(device, frame, "channel.depth", ANARI_FLOAT32);
              
    anari::commitParameters(device, frame);
#else
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(world.rank==0)?hostRGBA:nullptr);
#endif
  }

  void HayMaker::setTransferFunction(const TransferFunction &xf) 
  {
    for (int dgID=0;dgID<rankData.size();dgID++) {
      auto &dg = perDG[dgID];
      if (dg.createdVolumes.empty())
        continue;

      for (auto vol : dg.createdVolumes) {
#if HANARI
        std::vector<anari::math::float3> colors;
        std::vector<float> opacities;

        for (int i=0;i<xf.colorMap.size();i++) {
          auto c = xf.colorMap[i];
          colors.emplace_back(c.x,c.y,c.z);
          opacities.emplace_back(c.w);
        }
        anari::setParameter(device, vol,
                            "unitDistance",
                            xf.baseDensity);

        anari::setAndReleaseParameter
          (device,
           vol,
           "color",
           anari::newArray1D(device, colors.data(), colors.size()));
        anari::setAndReleaseParameter
          (device,
           vol,
           "opacity",
           anari::newArray1D(device, opacities.data(), opacities.size()));
        range1f valueRange = xf.domain;
        anariSetParameter(device, vol, "valueRange", ANARI_FLOAT32_BOX1, &valueRange.lower);
        
        anari::commitParameters(device, vol);
        
        anari::setAndReleaseParameter
          (device, 
           model, "volume", anari::newArray1D(device, &vol));
        anari::release(device, vol);
#else
        bnVolumeSetXF(vol,
                      (float2&)xf.domain,
                      (const float4*)xf.colorMap.data(),
                      (int)xf.colorMap.size(),
                      xf.baseDensity);
#endif
      }
    }
    parallel_for
      ((int)rankData.size(),
       [&](int dgID)
       {
         auto &dg = perDG[dgID];
         if (dg.createdVolumes.empty())
           return;
#if HANARI
         anari::commitParameters(device, dg.volumeGroup);
         anari::commitParameters(device, model);
#else
         bnGroupBuild(dg.volumeGroup);
         BNDataGroup barney = bnGetDataGroup(model,dgID);
         bnBuild(barney);
#endif
       });
  }
  
  void HayMaker::renderFrame(int pathsPerPixel)
  {
#if HANARI
    // anari::commitParameters(device, frame);
    anari::render(device, frame);
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
    if (fb.width != fbSize.x || fb.height != fbSize.y)
      std::cout << "resized frame!?" << std::endl;
    else {
      if (hostRGBA)
        memcpy(hostRGBA,fb.data,fbSize.x*fbSize.y*sizeof(uint32_t));
    }
#else
    bnRender(model,camera,fb,pathsPerPixel);
#endif
  }

  void HayMaker::resetAccumulation()
  {
#if HANARI
    anari::commitParameters(device, frame);
#else
    bnAccumReset(fb);
#endif
  }

  void HayMaker::setCamera(const Camera &camera)
  {
#if HANARI
    anari::setParameter(device, this->camera, "aspect", fbSize.x / (float)fbSize.y);
    anari::setParameter(device, this->camera, "position", (const anari::math::float3&)camera.vp);
    vec3f camera_dir = normalize(camera.vi - camera.vp);
    anari::setParameter(device, this->camera, "direction", (const anari::math::float3&)camera_dir);
    anari::setParameter(device, this->camera, "up", (const anari::math::float3&)camera.vu);
    anari::commitParameters(device, this->camera); // commit each object to indicate modifications are done1
#else
    vec3f dir = camera.vi - camera.vp;
    bnSet3fc(this->camera,"direction",(const float3&)dir);
    bnSet3fc(this->camera,"position",(const float3&)camera.vp);
    bnSet3fc(this->camera,"up",(const float3&)camera.vu);
    bnSet1f (this->camera,"fovy",camera.fovy);
    bnSet1f (this->camera,"aspect",fbSize.x / float(fbSize.y));
    // bnPinholeCamera(this->camera,
    //                 (const float3&)camera.vp,
    //                 (const float3&)camera.vi,
    //                 (const float3&)camera.vu,
    //                 camera.fovy,
    //                 fbSize.x / float(fbSize.y));
    bnCommit(this->camera);
#endif
  }


#if HANARI
#else
  struct TextureLibrary
  {
    TextureLibrary(BNDataGroup barney)
      : barney(barney)
    {}

    BNTexture getOrCreate(mini::Texture::SP miniTex)
    {
      auto it = alreadyCreated.find(miniTex);
      if (it != alreadyCreated.end()) return it->second;

      BNTexture bnTex = create(miniTex);
      alreadyCreated[miniTex] = bnTex;
      return bnTex;
    }
    
  private:
    BNTexture create(mini::Texture::SP miniTex)
    {
      if (!miniTex) return 0;
      BNTexelFormat texelFormat;
      switch (miniTex->format) {
      case mini::Texture::FLOAT4:
        texelFormat = BN_TEXEL_FORMAT_RGBA32F;
        break;
      case mini::Texture::FLOAT1:
        texelFormat = BN_TEXEL_FORMAT_R32F;
        break;
      case mini::Texture::RGBA_UINT8:
        texelFormat = BN_TEXEL_FORMAT_RGBA8;
        break;
      default:
        std::cout << "warning: unsupported mini::Texture format #" << (int)miniTex->format << std::endl;
        return 0;
      }
      
      BNTextureFilterMode filterMode;
      switch (miniTex->filterMode) {
      case mini::Texture::FILTER_BILINEAR:
        /*! default filter mode - bilinear */
        filterMode = BN_TEXTURE_LINEAR;
        break;
      case mini::Texture::FILTER_NEAREST:
        /*! explicitly request nearest-filtering */
        filterMode = BN_TEXTURE_NEAREST;
        break;
      default:
        std::cout << "warning: unsupported mini::Texture filter mode #" << (int)miniTex->filterMode << std::endl;
        return 0;
      }

      BNTextureAddressMode addressMode = BN_TEXTURE_WRAP;
      BNTextureColorSpace  colorSpace  = BN_COLOR_SPACE_LINEAR;
      BNTexture newTex
        = bnTexture2DCreate(barney,texelFormat,
                            miniTex->size.x,miniTex->size.y,
                            miniTex->data.data(),
                            filterMode,addressMode,colorSpace);
      return newTex;
    }
    
    std::map<mini::Texture::SP,BNTexture> alreadyCreated;
    BNDataGroup barney;
  };
#endif
  
  void HayMaker::buildDataGroup(int dgID)
  {
#if HANARI
    std::vector<anari::Light> lights;
#else
    BNDataGroup barney = bnGetDataGroup(model,dgID);
    std::vector<BNLight> lights;
#endif

    std::vector<vec4f> xfValues;
    for (int i=0;i<100;i++)
      xfValues.push_back(vec4f(.5f,.5f,0.5f,
                               clamp(5.f-fabsf((float)i-40),0.f,1.f)));

    auto &dg = perDG[dgID];

#if HANARI
    std::vector<anari::Surface>   rootGroupGeoms;
    std::vector<anari::Group>     groups;
#else
    std::vector<BNGeom>   rootGroupGeoms;
    std::vector<BNGroup>  groups;
#endif
    std::vector<affine3f> xfms;
    auto &myData = rankData.dataGroups[dgID];

    // ------------------------------------------------------------------
    // render all SphereGeoms
    // ------------------------------------------------------------------
    if (!myData.sphereSets.empty()) {
#if HANARI
#else
      // std::vector<BNGeom>  &geoms = rootGroupGeoms;
      for (auto &sphereSet : myData.sphereSets) {
        // BNMaterial material = BN_DEFAULT_MATERIAL;
        BNGeom geom
          = bnGeometryCreate(barney,"spheres");
          // = bnSpheresCreate(barney,
          //                   &material,
          //                   (float3*)sphereSet->origins.data(),
          //                   (int)sphereSet->origins.size(),
          //                   (float3*)sphereSet->colors.data(),
          //                   sphereSet->radii.data(),
          //                   sphereSet->radius);
        if (!geom) continue;
        // .......................................................
        BNData origins = bnDataCreate(barney,BN_FLOAT3,
                                      sphereSet->origins.size(),
                                      sphereSet->origins.data());
        bnSetData(geom,"origins",origins);
        // .......................................................
        if (!sphereSet->colors.empty()) {
          BNData colors = bnDataCreate(barney,BN_FLOAT3,
                                       sphereSet->colors.size(),
                                       sphereSet->colors.data());
          bnSetData(geom,"colors",colors);
        }
        // .......................................................
        if (!sphereSet->radii.empty()) {
          BNData radii = bnDataCreate(barney,BN_FLOAT,
                                      sphereSet->radii.size(),
                                      sphereSet->radii.data());
          bnSetData(geom,"radii",radii);
        }
        // .......................................................
        bnSet1f (geom,"radius",sphereSet->radius);
        // .......................................................
        // bnSet3fc(geom,"material.baseColor",   material.baseColor);
        // bnSet1f (geom,"material.transmission",material.transmission);
        // bnSet1f (geom,"material.ior",         material.ior);
        // if (material.colorTexture)
        //   bnSetObject(geom,"material.colorTexture",material.colorTexture);
        // if (material.alphaTexture)
        //   bnSetObject(geom,"material.alphaTexture",material.alphaTexture);
        // .......................................................
        bnCommit(geom);
        // .......................................................
        BNGroup group = bnGroupCreate(barney,&geom,1,0,0);
        bnGroupBuild(group);
        groups.push_back(group);
        xfms.push_back({});
        // geoms.push_back(geom);
      }
#endif
    }

    // ------------------------------------------------------------------
    // render all CylinderGeoms
    // ------------------------------------------------------------------
    if (!myData.cylinderSets.empty()) {
#if HANARI
#else
      std::vector<BNGeom>  &geoms = rootGroupGeoms;
      for (auto &cylinderSet : myData.cylinderSets) {
        // BNMaterialHelper material = BN_DEFAULT_MATERIAL;
        BNGeom geom
          = bnGeometryCreate(barney,"cylinders");

        if (!geom) continue;
        // .......................................................
        BNData vertices = bnDataCreate(barney,BN_FLOAT3,
                                      cylinderSet->vertices.size(),
                                      cylinderSet->vertices.data());
        PRINT(cylinderSet->vertices.size());
        bnSetData(geom,"vertices",vertices);
        // .......................................................
        if (cylinderSet->indices.empty()) {
          for (int i=0;i<cylinderSet->vertices.size();i+=2) {
            cylinderSet->indices.push_back({i,i+1});
          }
        }
        BNData indices = bnDataCreate(barney,BN_INT2,
                                      cylinderSet->indices.size(),
                                      cylinderSet->indices.data());
        bnSetData(geom,"indices",indices);
        PRINT(cylinderSet->indices.size());
        // .......................................................
        if (!cylinderSet->colors.empty()) {
          BNData colors = bnDataCreate(barney,BN_FLOAT3,
                                       cylinderSet->colors.size(),
                                       cylinderSet->colors.data());
          bnSetData(geom,"colors",colors);
        }
        // .......................................................
        if (!cylinderSet->radii.empty()) {
          BNData radii = bnDataCreate(barney,BN_FLOAT,
                                      cylinderSet->radii.size(),
                                      cylinderSet->radii.data());
          bnSetData(geom,"radii",radii);
          PRINT(cylinderSet->radii.size());
          bnSet1i(geom,"radiusPerVertex",(int)cylinderSet->radiusPerVertex);
        }
        // .......................................................
        // bnSet3fc(geom,"material.baseColor",material.baseColor);
        // bnSet1f(geom,"material.transmission",material.transmission);
        // bnSet1f(geom,"material.ior",material.ior);
        // if (material.colorTexture)
        //   bnSetObject(geom,"material.colorTexture",material.colorTexture);
        // if (material.alphaTexture)
        //   bnSetObject(geom,"material.alphaTexture",material.alphaTexture);
        // .......................................................
        bnCommit(geom);
        // .......................................................
        geoms.push_back(geom);
      }
#endif
    }

    for (auto mini : myData.minis) {
      // ------------------------------------------------------------------
      // set light(s) for given mini scene
      // ------------------------------------------------------------------
#if HANARI
#else
      for (auto ml : mini->quadLights) {
        BNLight light = bnLightCreate(barney,"quad");
        if (!light) continue;
        bnSet3fc(light,"corner",(const float3&)ml.corner);
        bnSet3fc(light,"edge0",(const float3&)ml.edge0);
        bnSet3fc(light,"edge1",(const float3&)ml.edge1);
        bnSet3fc(light,"emission",(const float3&)ml.emission);
        bnCommit(light);
        lights.push_back(light);
      }
      for (auto ml : mini->dirLights) {
        BNLight light = bnLightCreate(barney,"directional");
        if (!light) continue;
        bnSet3fc(light,"direction",(const float3&)ml.direction);
        bnSet3fc(light,"radiance",(const float3&)ml.radiance);
        bnCommit(light);
        lights.push_back(light);
      }
      if (mini->envMapLight) {
        BNLight light = bnLightCreate(barney,"environment");
        if (light) {
          bnSet4x3fv(light,"envMap.transform",(const float *)&mini->envMapLight->transform);
          PRINT(mini->envMapLight->transform);
          mini::Texture::SP envMap = mini->envMapLight->texture;
          if (envMap) {
#if 1
            BNTextureFilterMode filterMode = BN_TEXTURE_LINEAR;
            BNTextureAddressMode addressMode = BN_TEXTURE_WRAP;
            BNTextureColorSpace  colorSpace  = BN_COLOR_SPACE_LINEAR;
            BNTexelFormat texelFormat = BN_TEXEL_FORMAT_RGBA32F;
            BNTexture texture
              = bnTexture2DCreate(barney,texelFormat,
                                  envMap->size.x,envMap->size.y,
                                  envMap->data.data(),
                                  filterMode,addressMode,colorSpace);
            bnSetObject(light,"envMap.texture",texture);
#else
            BNData texData = 0;
            switch(envMap->format) {
            case mini::Texture::FLOAT4:
              texData = bnDataCreate(barney,BN_FLOAT4,
                                     envMap->size.x*envMap->size.y,
                                     envMap->data.data());
              break;
            default:
              std::cout << "un-supported env-map format - skipping" << std::endl;
            };
            bnSetData(light,"envMap.texels",texData);
            bnSet2i(light,"envMap.dims",envMap->size.x,envMap->size.y);
#endif
          }
          bnCommit(light);
          lights.push_back(light);
        }
      }
#endif
      // ------------------------------------------------------------------
      // render all (possibly instanced) triangle meshes from mini format
      // ------------------------------------------------------------------
#if HANARI
      std::map<mini::Object::SP, anari::Group> miniGroups;
#else
      std::map<mini::Object::SP, BNGroup> miniGroups;
      TextureLibrary textureLibrary(barney);
#endif

      
      for (auto inst : mini->instances) {
        if (!miniGroups[inst->object]) {
#if HANARI
          std::vector<anari::Surface> geoms;
#else
          std::vector<BNGeom> geoms;
#endif
          for (auto miniMesh : inst->object->meshes) {
#if HANARI
            anari::Material material
              = anari::newObject<anari::Material>(device, "matte");
            anari::math::float3 color(miniMesh->material->baseColor.x,
                                      miniMesh->material->baseColor.y,
                                      miniMesh->material->baseColor.z// ,
                                      // 1.f
                                      );
            PRINT((const vec3f&)color);
            anari::setParameter(device,material,"color",color);
            anari::commitParameters(device, material);
            
            anari::Geometry mesh
              = anari::newObject<anari::Geometry>(device, "triangle");
            anari::setParameterArray1D(device, mesh, "vertex.position",
                                       (const anari::math::float3*)miniMesh->vertices.data(),
                                       miniMesh->vertices.size());
            // anari::setParameterArray1D(device, mesh, "vertex.color", color, 4);
            anari::setParameterArray1D(device, mesh, "primitive.index",
                                       (const anari::math::uint3*)miniMesh->indices.data(),
                                       miniMesh->indices.size());
            anari::commitParameters(device, mesh);
            
            anari::Surface  surface = anari::newObject<anari::Surface>(device);
            anari::setAndReleaseParameter(device, surface, "geometry", mesh);
            anari::setAndReleaseParameter(device, surface, "material", material);
            anari::commitParameters(device, surface);
            
            auto geom = surface;
#else
            BNMaterialHelper material;// = BN_DEFAULT_MATERIAL;
            mini::Material::SP miniMat = miniMesh->material;
            material.baseColor = (const float3&)miniMat->baseColor;
            material.transmission = miniMat->transmission;
            material.metallic     = miniMat->metallic;
            material.roughness    = miniMat->roughness;
            material.ior          = miniMat->ior;
            material.colorTexture = textureLibrary.getOrCreate(miniMat->colorTexture);
            material.alphaTexture = textureLibrary.getOrCreate(miniMat->alphaTexture);
            // some of our test mini files have a texture, but basecolor isn't set - fix this here.
            if (material.colorTexture && (vec3f&)material.baseColor == vec3f(0.f))
              (vec3f&)material.baseColor = vec3f(1.f);
              
            BNGeom geom 
              = bnTriangleMeshCreate
              (barney,&material,
               (int3*)miniMesh->indices.data(),(int)miniMesh->indices.size(),
               (float3*)miniMesh->vertices.data(),(int)miniMesh->vertices.size(),
               miniMesh->normals.empty()
               ? nullptr
               : (float3*)miniMesh->normals.data(),
               miniMesh->texcoords.empty()
               ? nullptr
               : (float2*)miniMesh->texcoords.data()
               );
#endif
            geoms.push_back(geom);
          }
#if HANARI
          anari::Group meshGroup
            = anari::newObject<anari::Group>(device);
          anari::setParameterArray1D(device, meshGroup, "surface", geoms.data(),geoms.size());
          anari::commitParameters(device, meshGroup);
#else
          BNGroup meshGroup
            = bnGroupCreate(barney,geoms.data(),(int)geoms.size(),
                            nullptr,0);
          bnGroupBuild(meshGroup);
#endif
          miniGroups[inst->object] = meshGroup;
        }

        groups.push_back(miniGroups[inst->object]);
        xfms.push_back((const affine3f&)inst->xfm);

      }

    }
    
    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto _unst : myData.unsts) {
      auto unst = _unst.first;
      const box3f domain = _unst.second;
#if HANARI
#else
      //      BNMaterialHelper material = BN_DEFAULT_MATERIAL;
      std::vector<vec4f> verts4f;
      std::vector<int> gridOffsets;
      std::vector<vec3i> gridDims;
      std::vector<box4f> gridDomains;
      const std::vector<float> &gridScalars = unst->gridScalars;
      const float *scalars = unst->perVertex->values.data();

      for (int i=0;i<unst->grids.size();i++) {
        const umesh::Grid &grid = unst->grids[i];
        gridDims.push_back((vec3i &)grid.numCells);
        gridDomains.push_back((box4f &)grid.domain);
        gridOffsets.push_back(grid.scalarsOffset);
      }

      for (int i=0;i<unst->vertices.size();i++) {
        verts4f.push_back(vec4f(unst->vertices[i].x,
                                unst->vertices[i].y,
                                unst->vertices[i].z,
                                scalars[i]));
      }

      BNScalarField mesh = bnUMeshCreate
        (barney,
         // vertices: 4 floats each
         (const float *)verts4f.data(),(int)verts4f.size(),
         // tets: 4 ints each
         (const int *)unst->tets.data(),(int)unst->tets.size(),
         (const int *)unst->pyrs.data(),(int)unst->pyrs.size(),
         (const int *)unst->wedges.data(),(int)unst->wedges.size(),
         (const int *)unst->hexes.data(),(int)unst->hexes.size(),
         (int)unst->grids.size(),
         gridOffsets.data(),
         (const int *)gridDims.data(),
         (const float *)gridDomains.data(),
         gridScalars.data(), 
         (int)gridScalars.size(),
         (const float3*)&domain.lower);
      // rootGroupGeoms.push_back(geom);
      BNVolume volume = bnVolumeCreate(barney,mesh);
      dg.createdVolumes.push_back(volume);
#endif
    }
    

    // ------------------------------------------------------------------
    // render all UMeshes
    // ------------------------------------------------------------------
    for (auto vol : myData.structuredVolumes) {
#if HANARI
      anari::math::int3 volumeDims = (const anari::math::int3&)vol->dims;
      
      auto field = anari::newObject<anari::SpatialField>(device, "structuredRegular");
      anari::setParameter(device, field, "origin",
                          (const anari::math::float3&)vol->gridOrigin);
      anari::setParameter(device, field, "spacing",
                          (const anari::math::float3&)vol->gridSpacing);
      switch(vol->texelFormat) {
      case BN_TEXEL_FORMAT_R32F:
        anari::setParameterArray3D
          (device, field, "data", (const float *)vol->rawData.data(),
           volumeDims.x, volumeDims.y, volumeDims.z);
        break;
      case BN_TEXEL_FORMAT_R8:
        anari::setParameterArray3D
          (device, field, "data", (const uint8_t *)vol->rawData.data(),
           volumeDims.x, volumeDims.y, volumeDims.z);
        break;
      case BN_TEXEL_FORMAT_R16: {
        std::cout << "volume with uint16s, converting to float" << std::endl;
        static std::vector<float> volumeAsFloats(vol->rawData.size()/2);
        for (size_t i=0;i<volumeAsFloats.size();i++)
          volumeAsFloats[i] = ((uint16_t*)vol->rawData.data())[i]
            * (1.f/((1<<16)-1));
        anari::setParameterArray3D
          (device, field, "data", (const float *)volumeAsFloats.data(),
           volumeDims.x, volumeDims.y, volumeDims.z);
        } break;
      default:
        throw std::runtime_error("un-supported scalar type in hanari structured volume");
      }
        
      anari::commitParameters(device, field);

      auto volume = anari::newObject<anari::Volume>(device, "transferFunction1D");
      anari::setAndReleaseParameter(device, volume, "field", field);
      dg.createdVolumes.push_back(volume);
#else
      BNScalarField sf
        = bnScalarFieldCreate(barney,"structured");
      BNTexture3D texture
        = bnTexture3DCreate(barney,
                            vol->texelFormat,vol->dims.x,vol->dims.y,vol->dims.z,
                            vol->rawData.data());
      bnSetObject(sf,"texture",texture);
      bnRelease(texture);
      if (vol->rawDataRGB.size() != 0) {
        BNTexture3D texture
          = bnTexture3DCreate(barney,
                              BN_TEXEL_FORMAT_RGBA8,vol->dims.x,vol->dims.y,vol->dims.z,
                              vol->rawDataRGB.data());
        bnSetObject(sf,"textureColorMap",texture);
        bnRelease(texture);
      }
      bnSet3ic(sf,"dims",(const int3&)vol->dims);
      bnSet3fc(sf,"gridOrigin",(const float3&)vol->gridOrigin);
      bnSet3fc(sf,"gridSpacing",(const float3&)vol->gridSpacing);
      bnCommit(sf);
      dg.createdVolumes.push_back(bnVolumeCreate(barney,sf));
      bnRelease(sf);
#endif
    }
    

    // ------------------------------------------------------------------
    // create a single instance for all 'root' geometry that isn't
    // explicitly instantitated itself
    // ------------------------------------------------------------------
    if (!rootGroupGeoms.empty() || !lights.empty()) {
#if HANARI
      anari::Group rootGroup
        = anariNewGroup(device);
#else
      BNGroup rootGroup
        = bnGroupCreate(barney,rootGroupGeoms.data(),(int)rootGroupGeoms.size(),
                        nullptr,0);
      bnGroupBuild(rootGroup);

      if (!lights.empty()) {
        BNData lightsData = bnDataCreate(barney,BN_OBJECT,lights.size(),lights.data());
        bnSetData(rootGroup,"lights",lightsData);
      }
      bnCommit(rootGroup);
      
      groups.push_back(rootGroup);
      xfms.push_back(affine3f());
#endif

    }
    // ------------------------------------------------------------------
    // create a single instance for all 'root' volume(s)
    // ------------------------------------------------------------------
    if (!dg.createdVolumes.empty()) {
#if HANARI
      anari::Group rootGroup
        = anariNewGroup(device);
      anari::setParameterArray1D(device, rootGroup, "volume",
                                 dg.createdVolumes.data(),
                                 dg.createdVolumes.size());
#else
      BNGroup rootGroup
        = bnGroupCreate(barney,nullptr,0,
                        dg.createdVolumes.data(),
                        (int)dg.createdVolumes.size());
      bnGroupBuild(rootGroup);
      groups.push_back(rootGroup);
#endif
      xfms.push_back(affine3f());
      dg.volumeGroup = rootGroup;
    }

    // ------------------------------------------------------------------
    // finally - specify top-level instances for all the stuff we
    // generated
    // -----------------------------------------------------------------
#if HANARI

    std::vector<anari::Instance> instances;
    for (int i=0;i<groups.size();i++) {
      anari::Instance inst = anari::newObject<anari::Instance>(device,"transform");
      anari::setParameter(device, inst, "group", groups[i]);
      // anari::setAndReleaseParameter(device, inst, "group", groups[i]);

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
      anari::setParameter(device, inst, "transform", axf);
      // // getParam("transform", ANARI_FLOAT32_MAT4, &xfm);
      // m_xfm.xfm.l.vx = make_float3(xfm[0].x, xfm[0].y, xfm[0].z);
      // m_xfm.xfm.l.vy = make_float3(xfm[1].x, xfm[1].y, xfm[1].z);
      // m_xfm.xfm.l.vz = make_float3(xfm[2].x, xfm[2].y, xfm[2].z);
      // m_xfm.xfm.p = make_float3(xfm[3].x, xfm[3].y, xfm[3].z);
      
      // anari::setAndReleaseParameter(device, surface, "material", material);
      // todo: transform
      // anariSetParameter(self.device, instance, 'transform', ANARI_FLOAT32_MAT4, transform)
      anari::commitParameters(device, inst);
      instances.push_back(inst);
    } 
    anari::setAndReleaseParameter
      (device,
       model,
       "instance",
       anari::newArray1D(device, instances.data(),instances.size()));
    anari::commitParameters(device, model);
#else
    
    bnSetInstances(barney,groups.data(),(BNTransform *)xfms.data(),
                   (int)groups.size());
    bnBuild(barney);
#endif
  }
  
}
