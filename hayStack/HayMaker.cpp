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

  BNGroup    BarneyBackend::Slot::createGroup(const std::vector<BNGeom> &geoms)
  {
    BNGroup group = bnGroupCreate(global->model,this->slot,
                         (BNGeom *)geoms.data(),(int)geoms.size(),
                         nullptr,0);
    bnGroupBuild(group);
    return group;
  }
  anari::Group AnariBackend::Slot::createGroup(const std::vector<anari::Surface> &geoms)
  {
    anari::Group meshGroup
      = anari::newObject<anari::Group>(global->device);
    anari::setParameterArray1D(global->device, meshGroup, "surface", geoms.data(),geoms.size());
    anari::commitParameters(global->device, meshGroup);
    return meshGroup;
  }

  AnariBackend::GeomHandle AnariBackend::Slot::create(mini::Mesh::SP miniMesh,
                                                      MaterialLibrary<AnariBackend> *materialLib)
  {
    auto device = global->device;
    anari::Material material = materialLib->getOrCreate(miniMesh->material);
    anari::Geometry mesh
      = anari::newObject<anari::Geometry>(device, "triangle");
    anari::setParameterArray1D(device, mesh, "vertex.position",
                               (const anari::math::float3*)miniMesh->vertices.data(),
                               miniMesh->vertices.size());
    // anari::setParameterArray1D(device, mesh, "vertex.color", color, 4);
    anari::setParameterArray1D(device, mesh, "primitive.index",
                               (const anari::math::uint3*)miniMesh->indices.data(),
                               miniMesh->indices.size());
    if (!miniMesh->texcoords.empty())
      anari::setParameterArray1D(device, mesh, "vertex.attribute0",
                                 (const anari::math::float2*)miniMesh->texcoords.data(),
                               miniMesh->texcoords.size());
    anari::commitParameters(device, mesh);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", mesh);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return surface;
  }

  BarneyBackend::GeomHandle BarneyBackend::Slot::create(mini::Mesh::SP miniMesh,
                                                        MaterialLibrary<BarneyBackend> *materialLib)
  {
    auto model = global->model;
    BNMaterial mat = materialLib->getOrCreate(miniMesh->material);
    BNGeom geom = bnGeometryCreate(model,slot,"triangles");

    int numVertices = miniMesh->vertices.size();
    int numIndices = miniMesh->indices.size();
    const float2 *texcoords = (const float2*)miniMesh->texcoords.data();
    const float3 *vertices = (const float3*)miniMesh->vertices.data();
    const float3 *normals = (const float3*)miniMesh->normals.data();
    const int3 *indices = (const int3*)miniMesh->indices.data();
    BNData _vertices = bnDataCreate(model,slot,BN_FLOAT3,numVertices,vertices);
    bnSetAndRelease(geom,"vertices",_vertices);

    BNData _indices  = bnDataCreate(model,slot,BN_INT3,numIndices,indices);
    bnSetAndRelease(geom,"indices",_indices);

    if (normals) {
      BNData _normals  = bnDataCreate(model,slot,BN_FLOAT3,normals?numVertices:0,normals);
      bnSetAndRelease(geom,"normals",_normals);
    }

    if (texcoords) {
      BNData _texcoords  = bnDataCreate(model,slot,BN_FLOAT2,texcoords?numVertices:0,texcoords);
      bnSetData(geom,"vertex.attribute0",_texcoords);
      // bnSetAndRelease(geom,"texcoords",_texcoords);
      bnRelease(_texcoords);
    }
    bnSetObject(geom,"material",mat);
    bnCommit(geom);
    return geom;
  }


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

  BarneyBackend::Global::Global(HayMaker *base)
    : base(base)
  {
    bool isActiveWorker = !base->rankData.empty();
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : base->rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
#if HS_FAKE_MPI
      barney = bnContextCreate
        (   /*data*/dataGroupIDs.data(), (int)dataGroupIDs.size(),
            /*gpus*/nullptr, -1);
#else
      barney = bnMPIContextCreate
        (base->world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
         /*gpus*/nullptr,-1);
#endif
    } else {
#if HS_FAKE_MPI
      barney = bnContextCreate
        (/*data*/nullptr, 0,/*gpus*/nullptr, 0);
#else
      barney = bnMPIContextCreate
        (base->world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);
#endif
    }

    fb     = bnFrameBufferCreate(barney,0);
    model  = bnModelCreate(barney);
    camera = bnCameraCreate(barney,"perspective");
  }


  AnariBackend::Global::Global(HayMaker *base)
    : base(base)
  {
    bool isActiveWorker = !base->rankData.empty();
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : base->rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
      char *envlib = getenv("ANARI_LIBRARY");
      std::string libname = envlib ? "environment" : "barney";
      auto library = anari::loadLibrary(libname.c_str(), statusFunc);
      device = anari::newDevice(library, "default");
      anari::commitParameters(device, device);
    } else {
      throw std::runtime_error("passive master not yet implemented");
    }

    model = anari::newObject<anari::World>(device);
    anari::commitParameters(device, model);

    auto renderer = anari::newObject<anari::Renderer>(device, "default");
    anari::setParameter(device, renderer, "ambientRadiance", 10.f);
    anari::commitParameters(device, renderer);

    frame = anari::newObject<anari::Frame>(device);
    // anari::setParameter(device, frame, "size", imgSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(device, frame, "world",    model);
    anari::setParameter(device, frame, "renderer", renderer);

    this->camera = anari::newObject<anari::Camera>(device, "perspective");

    anari::setParameter(device, frame, "camera",   camera);
    anari::commitParameters(device, frame);
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

  void BarneyBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(base->world.rank==0)?hostRGBA:nullptr);
  }

  void AnariBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    anari::setParameter(device, frame, "size", (const anari::math::uint2&)fbSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_VEC4);
    anari::setParameter(device, frame, "channel.depth", ANARI_FLOAT32);

    anari::commitParameters(device, frame);
  }

  void BarneyBackend::Slot::setTransferFunction(const std::vector<BNVolume> &createdVolumes,
                                                const TransferFunction &xf)
  {
    if (createdVolumes.empty())
      return;

    for (auto vol : createdVolumes) {
      bnVolumeSetXF(vol,
                    (float2&)xf.domain,
                    (const float4*)xf.colorMap.data(),
                    (int)xf.colorMap.size(),
                    xf.baseDensity);
    }
    // bnGroupBuild(this->volumeGroup);
    // bnBuild(global->model,this->slot);
  }

  void AnariBackend::Slot::setTransferFunction(const std::vector<anari::Volume> &createdVolumes,
                                               const TransferFunction &xf)
  {
    if (createdVolumes.empty())
      return;

    auto device = global->device;
    auto model = global->model;

    for (auto vol : createdVolumes) {
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
        (device,vol,"color",
         anari::newArray1D(device, colors.data(), colors.size()));
      anari::setAndReleaseParameter
        (device,vol,"opacity",
         anari::newArray1D(device, opacities.data(), opacities.size()));
      range1f valueRange = xf.domain;
      anariSetParameter(device, vol, "valueRange", ANARI_FLOAT32_BOX1, &valueRange.lower);

      anari::commitParameters(device, vol);

      anari::setAndReleaseParameter
        (device, model, "volume", anari::newArray1D(device, &vol));
      anari::release(device, vol);
    }

    anari::commitParameters(device, this->volumeGroup);
    anari::commitParameters(device, global->model);
  }

  void BarneyBackend::Global::renderFrame(int pathsPerPixel)
  {
    bnRender(model,camera,fb,pathsPerPixel);
  }

  void AnariBackend::Global::renderFrame(int pathsPerPixel)
  {
    // anari::commitParameters(device, frame);
    anari::render(device, frame);
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
    if (fb.width != fbSize.x || fb.height != fbSize.y)
      std::cout << "resized frame!?" << std::endl;
    else {
      if (hostRGBA)
        memcpy(hostRGBA,fb.data,fbSize.x*fbSize.y*sizeof(uint32_t));
    }
  }

  void BarneyBackend::Global::resetAccumulation()
  {
    bnAccumReset(fb);
  }

  void AnariBackend::Global::resetAccumulation()
  {
    anari::commitParameters(device, frame);
  }

  void BarneyBackend::Global::setCamera(const Camera &camera)
  {
    vec3f dir = camera.vi - camera.vp;
    bnSet3fc(this->camera,"direction",(const float3&)dir);
    bnSet3fc(this->camera,"position", (const float3&)camera.vp);
    bnSet3fc(this->camera,"up",       (const float3&)camera.vu);
    bnSet1f (this->camera,"fovy",     camera.fovy);
    bnSet1f (this->camera,"aspect",   fbSize.x / float(fbSize.y));
    bnCommit(this->camera);
  }

  void AnariBackend::Global::setCamera(const Camera &camera)
  {
    anari::setParameter(device, this->camera,
                        "aspect",    fbSize.x / (float)fbSize.y);
    anari::setParameter(device, this->camera,
                        "position",  (const anari::math::float3&)camera.vp);
    vec3f camera_dir = normalize(camera.vi - camera.vp);
    anari::setParameter(device, this->camera,
                        "direction", (const anari::math::float3&)camera_dir);
    anari::setParameter(device, this->camera,
                        "up",        (const anari::math::float3&)camera.vu);
    anari::commitParameters(device, this->camera);
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

  BNSampler BarneyBackend::Slot::create(mini::Texture::SP miniTex)
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
      std::cout << "warning: unsupported mini::Texture format #"
                << (int)miniTex->format << std::endl;
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
      std::cout << "warning: unsupported mini::Texture filter mode #"
                << (int)miniTex->filterMode << std::endl;
      return 0;
    }

    BNTextureAddressMode wrapMode   = BN_TEXTURE_MIRROR;//WRAP;
    BNTextureColorSpace  colorSpace = BN_COLOR_SPACE_LINEAR;
    BNTextureData texData = bnTextureData2DCreate(global->model,this->slot,
                                                  texelFormat,
                                                  miniTex->size.x,miniTex->size.y,
                                                  miniTex->data.data());
    BNSampler sampler = bnSamplerCreate(global->model,this->slot,"texture2D");
    assert(sampler);
    bnSetString(sampler,"inAttribute","attribute0");
    bnSet1i(sampler,"wrapMode0",(int)wrapMode);
    bnSet1i(sampler,"wrapMode1",(int)wrapMode);
    bnSet1i(sampler,"wrapMode2",(int)wrapMode);
    bnSet1i(sampler,"filterMode",(int)filterMode);
    bnSetObject(sampler,"textureData",texData);
    bnRelease(texData);
    bnCommit(sampler);
    return sampler;
  }


  anari::Sampler AnariBackend::Slot::create(mini::Texture::SP miniTex)
  {
    if (!miniTex) return 0;

    auto device = global->device;
    std::string filterMode;
    switch (miniTex->filterMode) {
    case mini::Texture::FILTER_BILINEAR:
      /*! default filter mode - bilinear */
      filterMode = "linear";
      break;
    case mini::Texture::FILTER_NEAREST:
      /*! explicitly request nearest-filtering */
      filterMode = "nearest";
      break;
    default:
      std::cout << "warning: unsupported mini::Texture filter mode #"
                << (int)miniTex->filterMode << std::endl;
      return 0;
    }

    std::string wrapMode   = "mirrorRepeat";
    // BNTextureData texData = bnTextureData2DCreate(global->model,this->slot,
    //                                               texelFormat,
    //                                               miniTex->size.x,miniTex->size.y,
    //                                               miniTex->data.data());

    anari::Array2D image;
    switch (miniTex->format) {
    case mini::Texture::FLOAT4:
      image = anariNewArray2D(device, (const void *)miniTex->data.data(),
                              nullptr,nullptr,ANARI_FLOAT32_VEC4,
                              (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    case mini::Texture::FLOAT1:
      image = anariNewArray2D(device, (const void **)miniTex->data.data(),
                                0,0,ANARI_FLOAT32,
                                (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    case mini::Texture::RGBA_UINT8:
      image = anariNewArray2D(device, (const void *)miniTex->data.data(),
                                0,0,ANARI_UFIXED8_VEC4,
                                (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    default:
      std::cout << "warning: unsupported mini::Texture format #"
                << (int)miniTex->format << std::endl;
      return 0;
    }
    anari::commitParameters(device,image);

    anari::Sampler sampler
      = anari::newObject<anari::Sampler>(device,"image2D");
    assert(sampler);
    // anari::setParameter(device,sampler,"inAttribute","attribute0");
    anari::setParameter(device,sampler,"wrapMode1",wrapMode);
    anari::setParameter(device,sampler,"wrapMode2",wrapMode);
    anari::setParameter(device,sampler,"filterMode",filterMode);
    anari::setParameter(device,sampler,"image",image);
    anari::commitParameters(device,sampler);
    return sampler;
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

  BNMaterial BarneyBackend::Slot::create(mini::Plastic::SP plastic)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"plastic");
    bnSet3fc(mat,"pigmentColor",(const float3&)plastic->pigmentColor);
    bnSet3fc(mat,"Ks",(const float3&)plastic->Ks);
    bnSet1f(mat,"roughness",plastic->roughness);
    bnSet1f(mat,"eta",plastic->eta);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Velvet::SP velvet)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"velvet");
    bnSet3fc(mat,"reflectance",(const float3&)velvet->reflectance);
    bnSet3fc(mat,"horizonScatteringColor",(const float3&)velvet->horizonScatteringColor);
    bnSet1f(mat,"horizonScatteringFallOff",velvet->horizonScatteringFallOff);
    bnSet1f(mat,"backScattering",velvet->backScattering);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Matte::SP matte)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"matte");
    bnSet3fc(mat,"reflectance",(const float3&)matte->reflectance);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Metal::SP metal)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"metal");
    bnSet3fc(mat,"eta",(const float3&)metal->eta);
    bnSet3fc(mat,"k",(const float3&)metal->k);
    bnSet1f (mat,"roughness",metal->roughness);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::ThinGlass::SP thinGlass)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"matte");
    vec3f gray(.5f);
    bnSet3fc(mat,"reflectance",(const float3&)gray);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Dielectric::SP dielectric)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"glass");
    bnSet3fc(mat,"attenuationColorInside",(const float3&)dielectric->transmission);
    bnSet1f (mat,"etaInside",dielectric->etaInside);
    bnSet1f (mat,"etaOutside",dielectric->etaOutside);
    // BNMaterial mat = bnMaterialCreate(global->model,slot,"dielectric");
    // bnSet3fc(mat,"transmission",(const float3&)dielectric->transmission);
    // bnSet1f (mat,"etaInside",dielectric->etaInside);
    // bnSet1f (mat,"etaOutside",dielectric->etaOutside);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::MetallicPaint::SP metallicPaint)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"blender");
    bnSet3fc(mat,"baseColor",(const float3&)metallicPaint->shadeColor);
    bnSet1f (mat,"roughness",.15f);
    bnSet1f (mat,"metallic",.8f);
    bnSet1f (mat,"clearcoat",.15f);
    bnSet1f (mat,"clearcoat_roughness",.15f);
//     // bnSet1f (mat,"ior",metallicPaint->eta);
// #else
//     // float eta = 1.45f;
//     // vec3f glitterColor { 0.055f, 0.16f, 0.25f };
//     // float glitterSpread = 0.025f;
//     // vec3f shadeColor { 0.f, 0.03f, 0.07f };
//     BNMaterial mat = bnMaterialCreate(global->model,slot,"metallic_paint");
//     bnSet3fc(mat,"shadeColor",(const float3&)metallicPaint->shadeColor);
//     bnSet3fc(mat,"glitterColor",(const float3&)metallicPaint->glitterColor);
//     bnSet1f(mat,"glitterSpread",metallicPaint->glitterSpread);
//     bnSet1f(mat,"eta",metallicPaint->eta);
// #endif
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::DisneyMaterial::SP disney)
  {
    BNMaterial mat = bnMaterialCreate(global->model,slot,"AnariPBR");
    bnSet3fc(mat,"emission",(const float3&)disney->emission);
    bnSet3fc(mat,"baseColor",(const float3&)disney->baseColor);
    bnSet1f(mat,"roughness",   disney->roughness);
    bnSet1f(mat,"metallic",    disney->metallic);
    bnSet1f(mat,"transmission",disney->transmission);
    bnSet1f(mat,"ior",         disney->ior);

    if (disney->colorTexture) {
      BNSampler tex = impl->textureLibrary.getOrCreate(disney->colorTexture);
      if (tex) bnSetObject(mat,"baseColor",tex);
    }
    // if (disney->alphaTexture)
    //   bnSetObject(mat,"alphaTexture",backend->getOrCreate(slot, disney->alphaTexture));

    // if (disney->colorTexture)
    //   PRINT(disney->colorTexture->toString());
    // if (disney->alphaTexture)
    //   PRINT(disney->alphaTexture->toString());

    bnCommit(mat);
    return mat;
  }

  BNMaterial BarneyBackend::Slot::create(mini::Material::SP miniMat)
  {
    // PRINT(miniMat->toString());
    static std::set<std::string> typesCreated;
    if (typesCreated.find(miniMat->toString()) == typesCreated.end()) {
      std::cout
        << OWL_TERMINAL_YELLOW
        << "#hs: creating at least one instance of material *** "
        << miniMat->toString() << " ***"
        << OWL_TERMINAL_DEFAULT << std::endl;
      typesCreated.insert(miniMat->toString());
    }
    if (mini::Plastic::SP plastic = miniMat->as<mini::Plastic>())
      return create(plastic);
    if (mini::DisneyMaterial::SP disney = miniMat->as<mini::DisneyMaterial>())
      return create(disney);
    if (mini::Velvet::SP velvet = miniMat->as<mini::Velvet>())
      return create(velvet);
    if (mini::MetallicPaint::SP metallicPaint = miniMat->as<mini::MetallicPaint>())
      return create(metallicPaint);
    if (mini::Matte::SP matte = miniMat->as<mini::Matte>())
      return create(matte);
    if (mini::Metal::SP metal = miniMat->as<mini::Metal>())
      return create(metal);
    if (mini::Dielectric::SP dielectric = miniMat->as<mini::Dielectric>())
      return create(dielectric);
    if (mini::ThinGlass::SP thinGlass = miniMat->as<mini::ThinGlass>())
      return create(thinGlass);
    throw std::runtime_error("could not create barney material for mini mat "
                             +miniMat->toString());
  }

  anari::Material AnariBackend::Slot::create(mini::DisneyMaterial::SP disney)
  {
    auto device = global->device;

    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");
    anari::setParameter(device,material,"baseColor",
                        (const anari::math::float3&)disney->baseColor);
    anari::setParameter(device,material,"roughness",disney->roughness);
    anari::setParameter(device,material,"metallic", disney->metallic);
    anari::setParameter(device,material,"ior",      disney->ior);
    anari::setParameter(device,material,"opacityMode","mask");

    if (disney->colorTexture) {
      anari::Sampler tex = impl->textureLibrary.getOrCreate(disney->colorTexture);
      if (tex) anari::setParameter(device,material,"baseColor",tex);
    }

    anari::commitParameters(device, material);
    return material;
  }
  anari::Material AnariBackend::Slot::create(mini::Material::SP miniMat)
  {
    static std::set<std::string> typesCreated;
    if (typesCreated.find(miniMat->toString()) == typesCreated.end()) {
      std::cout
        << OWL_TERMINAL_YELLOW
        << "#hs: creating at least one instance of material *** "
        << miniMat->toString() << " ***"
        << OWL_TERMINAL_DEFAULT << std::endl;
      typesCreated.insert(miniMat->toString());
    }

    auto device = global->device;

    // if (mini::Plastic::SP plastic = miniMat->as<mini::Plastic>())
    //   return create(plastic);
    if (mini::DisneyMaterial::SP disney = miniMat->as<mini::DisneyMaterial>())
      return create(disney);
    // if (mini::Velvet::SP velvet = miniMat->as<mini::Velvet>())
    //   return create(velvet);
    // if (mini::MetallicPaint::SP metallicPaint = miniMat->as<mini::MetallicPaint>())
    //   return create(metallicPaint);
    // if (mini::Matte::SP matte = miniMat->as<mini::Matte>())
    //   return create(matte);
    // if (mini::Metal::SP metal = miniMat->as<mini::Metal>())
    //   return create(metal);
    // if (mini::Dielectric::SP dielectric = miniMat->as<mini::Dielectric>())
    //   return create(dielectric);
    // if (mini::ThinGlass::SP thinGlass = miniMat->as<mini::ThinGlass>())
    //   return create(thinGlass);
    // throw std::runtime_error("could not create barney material for mini mat "
    //                          +miniMat->toString());

    std::cout << MINI_TERMINAL_RED
              << "#warning: do not know how to realize mini material '"
              << miniMat->toString() << "'; replacing with matte gray"
              << MINI_TERMINAL_DEFAULT << std::endl;

    anari::Material material
      = anari::newObject<anari::Material>(device, "matte");
    anari::math::float3 color(.7f,.7f,.7f);
    anari::setParameter(device,material,"color",color);
    anari::commitParameters(device, material);
    return material;
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
    // create a group that can hold all kind of 'global' stuff that's
    // not an instance
    // ------------------------------------------------------------------
    rootGroup = impl->createGroup({});
    rootInstances.groups.push_back(rootGroup);
    rootInstances.xfms.push_back(affine3f{});

    // ------------------------------------------------------------------
    // render all mini::Scene formatted geometry - however many there
    // may be
    // -----------------------------------------------------------------
    auto &myData = this->global->base->rankData.dataGroups[this->slot];
    for (auto miniScene : myData.minis)
      renderMiniScene(miniScene);

    // ==================================================================
    // ==================================================================

    // ------------------------------------------------------------------
    // render all lights, mini::Scene formatted geometry - however many there
    // may be
    // -----------------------------------------------------------------

    // ==================================================================
    // now that all light and instances have been _created_ and
    // appended to the respective arrays, add these to the model
    // ==================================================================

    // 'attach' the lights to the root group
    impl->setLights(rootGroup,lights);

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
    return this->createGroup(meshes);
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


  void AnariBackend::Slot::setInstances(const std::vector<anari::Group> &groups,
                                        const std::vector<affine3f> &xfms)
  {
    auto device = global->device;
    auto model  = global->model;
    std::vector<anari::Instance> instances;
    PRINT(groups.size());
    PRINT((int*)groups[0]);
    for (int i=0;i<groups.size();i++) {
      anari::Instance inst
        = anari::newObject<anari::Instance>(device,"transform");
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
      anari::commitParameters(device, inst);
      instances.push_back(inst);
    }
    anari::setAndReleaseParameter
      (device,
       model,
       "instance",
       anari::newArray1D(device, instances.data(),instances.size()));
    std::cout << "### COMMITTING MODEL" << std::endl;
    anari::commitParameters(device, model);
  }


  void BarneyBackend::Slot::setInstances(const std::vector<BNGroup> &groups,
                                         const std::vector<affine3f> &xfms)
  {
    bnSetInstances(global->model,this->slot,
                   (BNGroup*)groups.data(),(BNTransform *)xfms.data(),
                   (int)groups.size());
    bnBuild(global->model,slot);
  }


  template<typename Backend>
  void HayMakerT<Backend>::buildSlots()
  {
    for (auto slot : perSlot)
      slot->renderAll();
    global.finalizeRender();
  }

  void AnariBackend::Global::finalizeRender()
  {
    anari::setParameter(device, frame, "world",    model);
    std::cout << "### COMMITTING FRAME" << std::endl;
    anari::commitParameters(device, frame);
  }

  void BarneyBackend::Global::finalizeRender()
  {
  }

      BNLight BarneyBackend::Slot::create(const mini::QuadLight &ml)
      {
        BNLight light = bnLightCreate(global->model,this->slot,"quad");
        assert(light);
        bnSet3fc(light,"corner",(const float3&)ml.corner);
        bnSet3fc(light,"edge0",(const float3&)ml.edge0);
        bnSet3fc(light,"edge1",(const float3&)ml.edge1);
        bnSet3fc(light,"emission",(const float3&)ml.emission);
        bnCommit(light);
        return light;
      }

  BNLight BarneyBackend::Slot::create(const mini::DirLight &ml)
  {
    BNLight light = bnLightCreate(global->model,this->slot,"directional");
    assert(light);
    bnSet3fc(light,"direction",(const float3&)ml.direction);
    bnSet3fc(light,"radiance",(const float3&)ml.radiance);
    bnCommit(light);
    return light;
  }

  anari::Light AnariBackend::Slot::create(const mini::DirLight &ml)
  {
    auto device = global->device;
    anari::Light light = anari::newObject<anari::Light>(device,"directional");
    assert(light);
    anari::setParameter(device,light,"direction",(const anari::math::float3&)ml.direction);
    anari::setParameter(device,light,"irradiance",(const anari::math::float3&)ml.radiance);
    anari::commitParameters(device,light);
    PING; PRINT(ml.radiance);
    return light;
  }

      BNLight BarneyBackend::Slot::create(const mini::EnvMapLight &ml)
      {
        return {};
      }

  void BarneyBackend::Slot::setLights(BNGroup rootGroup,
                                      const std::vector<BNLight> &lights)
  {
    PING; PRINT(lights.size());
    if (!lights.empty()) {
      BNData lightsData = bnDataCreate(global->model,this->slot,
                                       BN_OBJECT,lights.size(),lights.data());
      bnSetData(rootGroup,"lights",lightsData);
      bnRelease(lightsData);
    }
    bnCommit(rootGroup);
    bnGroupBuild(rootGroup);
  }


  void AnariBackend::Slot::setLights(anari::Group rootGroup,
                                     const std::vector<anari::Light> &lights)
  {
    auto device = global->device;
    PRINT(lights.size());
    if (!lights.empty()) {
      anari::setParameterArray1D
        (device, global->model, "light", lights.data(),lights.size());
      // BNData lightsData
      //   = bnDataCreate(global->model,this->slot,
      //                                  BN_OBJECT,lights.size(),lights.data());
      // bnSetData(rootGroup,"lights",lightsData);
      // bnRelease(lightsData);
    }
    PING;
    anari::commitParameters(device,global->model);
  }


  template struct HayMakerT<BarneyBackend>;
  template struct HayMakerT<AnariBackend>;
}
