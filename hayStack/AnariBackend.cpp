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

#if HANARI
#include "AnariBackend.h"

#if 0
# define TEST_IDCHANNEL "channel.primitiveId"    
#endif

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

  anari::Group AnariBackend::Slot::createGroup(const std::vector<anari::Surface> &geoms,
                                               const std::vector<anari::Volume> &volumes)
  {
    anari::Group meshGroup
      = anari::newObject<anari::Group>(global->device);
    anari::setParameterArray1D(global->device, meshGroup, "surface", geoms.data(),geoms.size());
    anari::setParameterArray1D(global->device, meshGroup, "volume", volumes.data(),volumes.size());
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
#if 1
    if (!miniMesh->normals.empty()) {
      anari::setParameterArray1D(device, mesh, "vertex.normal",
                                 (const anari::math::float3*)miniMesh->normals.data(),
                                 miniMesh->normals.size());
    }
#endif
    anari::commitParameters(device, mesh);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", mesh);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return surface;
  }

  AnariBackend::Global::Global(HayMaker *base)
    : base(base)
  {
    bool isActiveWorker = !base->localModel.empty();
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : base->localModel.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
      char *envlib = getenv("ANARI_LIBRARY");
      std::string libname = envlib ? "environment" :
#if HS_MPI
        "barney_mpi"
#else
        "barney"
#endif
        ;
      auto library = anari::loadLibrary(libname.c_str(), statusFunc);
      device = anari::newDevice(library, "default");
      anari::commitParameters(device, device);
    } else {
      throw std::runtime_error("passive master not yet implemented");
    }

    model = anari::newObject<anari::World>(device);
    anari::commitParameters(device, model);

    auto renderer = anari::newObject<anari::Renderer>(device, "default");
    // anari::setParameter(device, renderer, "ambientRadiance", 0.f);
    anari::setParameter(device, renderer, "ambientRadiance", .6f);
    anari::setParameter(device, renderer, "pixelSamples", base->pixelSamples);
#if 1
    std::vector<vec4f> bgGradient = {
      // vec4f(1,1,1,1),vec4f(.4,.4,1,1)
      // };
      // vec4f gradient[2] = {
      vec4f(.9f,.9f,.9f,1.f),
      vec4f(0.15f, 0.25f, .8f,1.f),
    };
    anari::setAndReleaseParameter
      (device,renderer,"background",
       anari::newArray2D(device,
                         (const anari::math::float4*)bgGradient.data(),
                         1,2));//bgGradient.size()));
#endif
    anari::commitParameters(device, renderer);

    frame = anari::newObject<anari::Frame>(device);
    // anari::setParameter(device, frame, "size", imgSize);
    // anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(device, frame, "world",    model);
    anari::setParameter(device, frame, "renderer", renderer);

    this->camera = anari::newObject<anari::Camera>(device, "perspective");

    anari::setParameter(device, frame, "camera",   camera);
    anari::commitParameters(device, frame);
  }

  void AnariBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    anari::setParameter(device, frame, "size", (const anari::math::uint2&)fbSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    // anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_VEC4);
    // anari::setParameter(device, frame, "channel.depth", ANARI_FLOAT32);
#ifdef TEST_IDCHANNEL
    anari::setParameter(device, frame, TEST_IDCHANNEL, ANARI_UINT32);
#endif

    anari::commitParameters(device, frame);
  }

#if 1
  void AnariBackend::Slot::applyTransferFunction(const TransferFunction &xf)
  {
    if (impl->rootVolumes.empty())
      return;

    auto device = global->device;
    auto model = global->model;
    
    for (auto vol : impl->rootVolumes) {
#if 1
      int N = xf.colorMap.size();
      auto colorArray = anari::newArray1D(device,ANARI_FLOAT32_VEC3,N);
      auto alphaArray = anari::newArray1D(device,ANARI_FLOAT32,N);
      vec3f *colors = (vec3f*)anariMapArray(device,colorArray);
      float *alphas = (float*)anariMapArray(device,alphaArray);
      for (int i=0;i<N;i++) {
        auto c = xf.colorMap[i];
        colors[i] = vec3f(c.x,c.y,c.z);
        alphas[i] = c.w;
      }
      anariUnmapArray(device,colorArray);
      anariUnmapArray(device,alphaArray);
      anari::setAndReleaseParameter
        (device,vol,"color",colorArray);
      anari::setAndReleaseParameter
        (device,vol,"opacity",alphaArray);
#else
      std::vector<anari::math::float3> colors;
      std::vector<float> opacities;

      for (int i=0;i<xf.colorMap.size();i++) {
        auto c = xf.colorMap[i];
        colors.emplace_back(c.x,c.y,c.z);
        opacities.emplace_back(c.w);
      }
      anari::setAndReleaseParameter
        (device,vol,"color",
         anari::newArray1D(device, colors.data(), colors.size()));
      anari::setAndReleaseParameter
        (device,vol,"opacity",
         anari::newArray1D(device, opacities.data(), opacities.size()));
#endif
      anari::setParameter(device, vol,
                          "unitDistance",
                          xf.baseDensity);
      range1f valueRange = xf.domain;
      anariSetParameter(device, vol, "valueRange",
                        ANARI_FLOAT32_BOX1,
                        &valueRange.lower);

      anari::commitParameters(device, vol);
    }

    anari::commitParameters(device, impl->volumeGroup);
    anari::commitParameters(device, global->model);
  }
#else
  void AnariBackend::Slot::setTransferFunction(const std::vector<anari::Volume> &rootVolumes,
                                               const TransferFunction &xf)
  {
    if (rootVolumes.empty())
      return;

    auto device = global->device;
    auto model = global->model;

    for (auto vol : rootVolumes) {
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
    }

    anari::commitParameters(device, impl->volumeGroup);
    anari::commitParameters(device, global->model);
  }
#endif
  
  void AnariBackend::Global::renderFrame()
  {
    // anari::commitParameters(device, frame);
    anari::render(device, frame);
#ifdef TEST_IDCHANNEL
    auto fb = anari::map<uint32_t>(device, frame, "TEST_IDCHANNEL");
#else
    auto fb = anari::map<uint32_t>(device, frame, "channel.color");
#endif
    
    if (fb.width != fbSize.x || fb.height != fbSize.y)
      std::cout << "resized frame!?" << std::endl;
    else {
      if (hostRGBA) {
#ifdef TEST_IDCHANNEL
        const uint64_t FNV_basis = 0xcbf29ce484222325ULL;
        const uint64_t FNV_prime = 0x100000001b3ULL;
        for (int i=0;i<fb.width*fb.height;i++) {
          uint32_t ID = fb.data[i];
          uint64_t s = FNV_basis + FNV_prime * ID;
          
          s = s * FNV_prime ^ ID;
          int r = s & 0xff;
          s = s * FNV_prime ^ ID;
          int g = s & 0xff;
          s = s * FNV_prime ^ ID;
          int b = s & 0xff;
          uint32_t rgba = b<<24 | g<<16 | r<<8 | 0xff;
          hostRGBA[i] = rgba;
        }
#else
        memcpy(hostRGBA,fb.data,fbSize.x*fbSize.y*sizeof(uint32_t));
#endif
      }
    }
#ifdef TEST_IDCHANNEL
    anari::unmap(device,frame,TEST_IDCHANNEL);
#else
    anari::unmap(device,frame,"channel.color");
#endif
  }

  void AnariBackend::Global::resetAccumulation()
  {
    anari::commitParameters(device, frame);
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
    // PRINT(camera_dir);
    anari::setParameter(device, this->camera,
                        "up",        (const anari::math::float3&)camera.vu);
    anari::commitParameters(device, this->camera);
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

  anari::Material AnariBackend::Slot::create(mini::Metal::SP metal, bool colorMapped)
  {
    auto device = global->device;

    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");
    anari::setParameter(device,material,"alphaMode","blend");
    
    vec3f baseColor = metal->k * float (1.f/ M_PI);
    if (colorMapped)  {
      anari::setParameter(device,material,"baseColor","color");
    } else {
      anari::setParameter(device,material,"baseColor",
                          (const anari::math::float3&)baseColor);
    }
    anari::setParameter(device,material,"alphaMode","blend");
    anari::setParameter(device,material,"metallic",1.f);
    anari::setParameter(device,material,"opacity",1.f);
    anari::setParameter(device,material,"roughness",metal->roughness);
    anari::setParameter(device,material,"ior",metal->eta.x);
    anari::commitParameters(device,material);
    return material;
  }
  
  anari::Material AnariBackend::Slot::create(mini::MetallicPaint::SP metal, bool colorMapped)
  {
    auto device = global->device;

    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");
    anari::setParameter(device,material,"alphaMode","blend");

    vec3f baseColor = (const vec3f&)metal->shadeColor;
    anari::setParameter(device,material,"baseColor",
                        (const anari::math::float3&)baseColor);
    anari::setParameter(device,material,"alphaMode","blend");
    anari::setParameter(device,material,"metallic",1.f);
    anari::setParameter(device,material,"opacity",1.f);
    anari::setParameter(device,material,"roughness",metal->glitterSpread);
    anari::setParameter(device,material,"ior",1.f/metal->eta);
    anari::setParameter(device,material,"specular",.0f);
    anari::setParameter(device,material,"clearcoat",.0f);
    anari::setParameter(device,material,"transmission",.0f);
    anari::commitParameters(device, material);
    return material;
  }

  
  anari::Material AnariBackend::Slot::create(mini::Matte::SP matte,
                                             bool colorMapped)
  {
    auto device = global->device;
    anari::Material material
      = anari::newObject<anari::Material>(device, "matte");
    anari::setParameter(device,material,"alphaMode","blend");

    vec3f color = matte->reflectance / 3.14f;
    if (colorMapped) {
      anari::setParameter(device,material,"color",
                          "color");
    } else {
      anari::setParameter(device,material,"color",
                          (const anari::math::float3&)color);
    }
    anari::commitParameters(device, material);
    return material;
  }
  
  anari::Material AnariBackend::Slot::create(mini::DisneyMaterial::SP disney,
                                             bool colorMapped)
  {
    auto device = global->device;

#if 0
    {
      anari::Material material
        = anari::newObject<anari::Material>(device, "matte");
      anari::setParameter(device,material,"alphaMode","blend");
      anari::setParameter(device,material,"color",
                          (const anari::math::float3&)disney->baseColor);
      anari::commitParameters(device, material);
      return material;
    }
#endif
    
    
    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");
    anari::setParameter(device,material,"alphaMode","blend");

    if (colorMapped)
      anari::setParameter(device,material,"baseColor",
                          "color");
    else {
      anari::setParameter(device,material,"baseColor",
                          (const anari::math::float3&)disney->baseColor);
    }
#if 1
    // anari::setParameter(device,material,"metallic",1.f);
    anari::setParameter(device,material,"metallic",disney->metallic);
    anari::setParameter(device,material,"opacity",1.f-disney->transmission);
    // anari::setParameter(device,material,"roughness",.02f);
    anari::setParameter(device,material,"roughness",disney->roughness);
    // anari::setParameter(device,material,"specular",.0f);
    anari::setParameter(device,material,"specular",.0f);
    anari::setParameter(device,material,"clearcoat",.0f);
    // anari::setParameter(device,material,"transmission",.0f);
    // anari::setParameter(device,material,"transmission",disney->transmission);
    anari::setParameter(device,material,"ior",disney->ior);
#else
    anari::setParameter(device,material,"roughness",disney->roughness);
    anari::setParameter(device,material,"metallic", disney->metallic);
    anari::setParameter(device,material,"ior",      disney->ior);
    anari::setParameter(device,material,"alphaMode","mask");
#endif
    if (disney->colorTexture) {
      anari::Sampler tex = impl->textureLibrary.getOrCreate(disney->colorTexture);
      if (tex) anari::setParameter(device,material,"baseColor",tex);
    }

    anari::commitParameters(device, material);
    return material;
  }

  anari::Material AnariBackend::Slot::create(mini::Dielectric::SP dielectric, bool colorMapped)
  {
    auto device = global->device;

    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");

    anari::setParameter(device,material,"alphaMode","blend");
    anari::setParameter(device,material,"ior",dielectric->etaInside);
    anari::setParameter(device,material,"transmission",1.f);
    anari::setParameter(device,material,"metallic",0.f);
    anari::setParameter(device,material,"specular",0.f);
    anari::setParameter(device,material,"roughness",0.f);

    anari::commitParameters(device, material);
    return material;
  }

  anari::Material AnariBackend::Slot::create(mini::Plastic::SP plastic, bool colorMapped)
  {
    auto device = global->device;

#if 0
    {
      vec3f base(1,0,0);
      anari::Material material
        = anari::newObject<anari::Material>(device, "matte");
      anari::setParameter(device,material,"alphaMode","blend");
      anari::setParameter(device,material,"color",(const anari::math::float3&)base);
      anari::commitParameters(device, material);
      return material;
    }
#endif
    anari::Material material
      = anari::newObject<anari::Material>(device, "physicallyBased");

    anari::setParameter(device,material,"alphaMode","blend");
    anari::setParameter(device,material,"ior",plastic->eta);
    /* this is almost certainly wrong, but in the embree imported
       xml files all the plastic's have Ks==1.f and only non-one
       value is pigmentcolor... */
    vec3f base = min(plastic->Ks,plastic->pigmentColor);;
    anari::setParameter(device,material,"baseColor",(const anari::math::float3&)base);
    anari::setParameter(device,material,"transmission",0.f);
    anari::setParameter(device,material,"metallic",0.f);
    anari::setParameter(device,material,"specular",0.f);
    anari::setParameter(device,material,"roughness",plastic->roughness);

    anari::commitParameters(device, material);
    return material;
  }
  
  
  anari::Material AnariBackend::Slot::create(mini::Material::SP miniMat, bool colorMapped)
  {
    static std::set<std::string> typesCreated;
    if (typesCreated.find(miniMat->toString()) == typesCreated.end()) {
      std::cout
        << MINI_TERMINAL_YELLOW
        << "#hs: creating at least one instance of material *** "
        << miniMat->toString() << " ***"
        << MINI_TERMINAL_DEFAULT << std::endl;
      typesCreated.insert(miniMat->toString());
    }

    auto device = global->device;

    if (mini::Plastic::SP plastic = miniMat->as<mini::Plastic>())
      return create(plastic, colorMapped);
    if (mini::DisneyMaterial::SP disney = miniMat->as<mini::DisneyMaterial>())
      return create(disney, colorMapped);
    if (mini::Dielectric::SP dielectric = miniMat->as<mini::Dielectric>())
      return create(dielectric, colorMapped);
    // if (mini::Velvet::SP velvet = miniMat->as<mini::Velvet>())
    //   return create(velvet);
    if (mini::MetallicPaint::SP metallicPaint = miniMat->as<mini::MetallicPaint>())
      return create(metallicPaint, colorMapped);
    if (mini::Matte::SP matte = miniMat->as<mini::Matte>())
      return create(matte, colorMapped);
    if (mini::Metal::SP metal = miniMat->as<mini::Metal>())
      return create(metal, colorMapped);
    if (mini::Dielectric::SP dielectric = miniMat->as<mini::Dielectric>())
      return create(dielectric, colorMapped);
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
    anari::setParameter(device,material,"alphaMode","blend");
    anari::math::float3 color(.7f,.7f,.7f);
    anari::setParameter(device,material,"color",color);
    anari::commitParameters(device, material);
    return material;
  }

  void AnariBackend::Slot::setInstances(const std::vector<anari::Group> &groups,
                                        const std::vector<affine3f> &xfms)
  {
    auto device = global->device;
    auto model  = global->model;
    std::vector<anari::Instance> instances;
    for (int i=0;i<groups.size();i++) {
      anari::Instance inst
        = anari::newObject<anari::Instance>(device,"transform");
      anari::setParameter(device, inst, "group", groups[i]);

      // vec3f rc = randomColor(i);
      // anari::math::float4 instColor(rc.x,rc.y,rc.z,1.f);
      // anari::setParameter(device, inst, "color", instColor);

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
    anari::commitParameters(device, model);
    
  }

  void AnariBackend::Global::finalizeRender()
  {
    if (dirty) {
      anari::setParameter(device, frame, "world",    model);
      anari::commitParameters(device, frame);
      dirty = false;
    }
  }

  float average(vec3f v) { return (v.x+v.y+v.z)/3.f; }


  //#define CHECK_HDRI 1
  
  anari::Light AnariBackend::Slot::create(const mini::EnvMapLight &ml)
  {
    std::cout << MINI_TERMINAL_YELLOW
              << "#hs: creating env-map light ..."
              << MINI_TERMINAL_DEFAULT << std::endl;
    auto device = global->device;
    vec2i size = ml.texture->size;
    anari::Array2D radiance
      = anariNewArray2D(device, nullptr,nullptr,nullptr,
                        ANARI_FLOAT32_VEC3,
                        (size_t)size.x,(size_t)size.y);
    vec3f *as3f = (vec3f*)anariMapArray(device,radiance);
    PING; PRINT(size);
#if CHECK_HDRI
    // ==================================================================
    // base pattern is thin black grid on white background
    // ==================================================================
    for (int iy=0;iy<size.y;iy++) {
      for (int ix=0;ix<size.x;ix++) {
        vec3f color 
          = ((ix%16==0)||(iy%16==0))
          ? vec3f(0.f)
          : vec3f(1.f);
        as3f[ix+iy*size.x] = color;
      }
    }
    // ==================================================================
    // red square in lower left corner
    // ==================================================================
    for (int iy=0;iy<size.y/32;iy++) {
      for (int ix=0;ix<size.x/32;ix++) {
        vec3f color = vec3f(1,0,0);
        as3f[ix+iy*size.x] = color;
      }
    }
    // ==================================================================
    // blue crosshair through center image, 
    // ==================================================================
    for (int iy=0;iy<size.y;iy++) {
      int ix = size.x/2;
      as3f[ix+iy*size.x] = vec3f(0,0,1);
    }
    for (int ix=0;ix<size.x;ix++) {
      int iy = size.y/2;
      as3f[ix+iy*size.x] = vec3f(0,0,1);
    }
    // ==================================================================
    // gradient
    // ==================================================================
    int iy0 = size.y/2-16;
    int ix0 = size.x/2-16;
    int iy1 = size.y/2+16;
    int ix1 = size.x/2+16;
    for (int iy=iy0;iy<=iy1;iy++) {
      for (int ix=ix0;ix<=ix1;ix++) {
        float r = float(ix-ix0)/float(ix1-ix0);
        float g = float(iy-iy0)/float(iy1-iy0);
        // as3f[ix+iy*size.x] = vec3f(0,1,0);
        as3f[ix+iy*size.x] = vec3f(r,g,(r+g)/2.f);
      }
    }
#else
    for (int i=0;i<size.x*size.y;i++) {
      as3f[i]
        = (const vec3f&)((vec4f*)ml.texture->data.data())[i];
    }
#endif
    anariUnmapArray(device,radiance);
    anari::commitParameters(device,radiance);
    
    anari::Light light = anari::newObject<anari::Light>(device,"hdri");
    anari::setAndReleaseParameter(device,light,"radiance",radiance);
    vec3f up = ml.transform.l.vz;
    vec3f dir = - ml.transform.l.vx;

    std::cout << "setting HDRI orientation dir = " << dir << ", up = " << up << std::endl;
    anari::setParameter(device,light,"up",
                        (const anari::math::float3&)up);
    anari::setParameter(device,light,"direction",
                        (const anari::math::float3&)dir);
    anari::setParameter(device,light,"scale",1.f);
    anari::commitParameters(device,light);
    return light;
  }

  
  anari::Light AnariBackend::Slot::create(const mini::DirLight &ml)
  {
    auto device = global->device;
    anari::Light light = anari::newObject<anari::Light>(device,"directional");
    assert(light);
    std::cout << "dir light direction "
              << (const vec3f&)ml.direction << std::endl;
    anari::setParameter(device,light,"direction",(const anari::math::float3&)ml.direction);
    anari::setParameter(device,light,"irradiance",average(ml.radiance));
    anari::commitParameters(device,light);
    return light;
  }

  void AnariBackend::Slot::setLights(anari::Group rootGroup,
                                     const std::vector<anari::Light> &lights)
  {
    auto device = global->device;
    if (!lights.empty()) {
      anari::setParameterArray1D
        (device, global->model, "light", lights.data(),lights.size());
    }
    anari::commitParameters(device,global->model);
  }

  anari::Volume AnariBackend::Slot::create(const StructuredVolume::SP &vol)
  {
    auto device = global->device;

    anari::math::int3 volumeDims = (const anari::math::int3&)vol->dims;
      
    auto field = anari::newObject<anari::SpatialField>(device, "structuredRegular");
    anari::setParameter(device, field, "origin",
                        (const anari::math::float3&)vol->gridOrigin);
    anari::setParameter(device, field, "spacing",
                        (const anari::math::float3&)vol->gridSpacing);
    if (vol->texelFormat == "float") {
      anari::setParameterArray3D
        (device, field, "data", (const float *)vol->rawData.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else if (vol->texelFormat == "uint8_t") {
      anari::setParameterArray3D
        (device, field, "data", (const uint8_t *)vol->rawData.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else if (vol->texelFormat == "uint16_t") {
      std::cout << "volume with uint16s, converting to float" << std::endl;
      static std::vector<float> volumeAsFloats(vol->rawData.size()/2);
      for (size_t i=0;i<volumeAsFloats.size();i++)
        volumeAsFloats[i] = ((uint16_t*)vol->rawData.data())[i]
          * (1.f/((1<<16)-1));
      anari::setParameterArray3D
        (device, field, "data", (const float *)volumeAsFloats.data(),
         volumeDims.x, volumeDims.y, volumeDims.z);
    } else {
      throw std::runtime_error("un-supported scalar type in hanari structured volume");
    }
        
    anari::commitParameters(device, field);

    auto volume = anari::newObject<anari::Volume>(device, "transferFunction1D");
    anari::setAndReleaseParameter(device, volume, "value", field);
    anari::commitParameters(device, volume);

    return volume;
  }

  anari::Volume AnariBackend::Slot::create(const std::pair<umesh::UMesh::SP,box3f> &meshAndDomain)
  {
    auto mesh = meshAndDomain.first;
    assert(mesh);
    auto device = global->device;

    auto field = anari::newObject<anari::SpatialField>(device, "unstructured");

    anari::setParameterArray1D
      (device, field, "vertex.position",
       (const anari::math::float3 *)mesh->vertices.data(),
       mesh->vertices.size());
    anari::setParameterArray1D
      (device, field, "vertex.data",
       (const float *)mesh->perVertex->values.data(),
       mesh->perVertex->values.size());
    std::vector<uint8_t>  cellTypeData;
    std::vector<uint32_t> cellBeginData;
    std::vector<uint32_t> indexData;

    // this isn't fully spec'ed yet
    enum { _VTK_TET = 10, _VTK_HEX=12, _VTK_WEDGE=13, _VTK_PYR=14 };
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
    
    anari::setParameterArray1D
      (device, field, "cell.type",
       (const uint8_t *)cellTypeData.data(),
       cellTypeData.size());
    anari::setParameterArray1D
      (device, field, "cell.begin",
       (const uint32_t *)cellBeginData.data(),
       cellBeginData.size());
    anari::setParameterArray1D
      (device, field, "index",
       (const uint32_t *)indexData.data(),
       indexData.size());

    anari::commitParameters(device, field);
    
    auto volume = anari::newObject<anari::Volume>(device, "transferFunction1D");
    anari::setAndReleaseParameter(device, volume, "value", field);
    anari::commitParameters(device, volume);

    return volume;
  }

  std::vector<anari::Surface>
  AnariBackend::Slot::createCapsules(hs::Capsules::SP caps,
                                     MaterialLibrary<AnariBackend> *materialLib)
  {
    auto device = global->device;
    anari::Material material
      = materialLib->getOrCreate(caps->material,caps->colors.size()>0);
    std::vector<vec3f> position;
    std::vector<float> radius;
    std::vector<vec4f> color;
    std::vector<uint32_t> index;
    for (auto idx : caps->indices) {
      index.push_back(position.size());
      position.push_back((const vec3f&)caps->vertices[idx.x]);
      position.push_back((const vec3f&)caps->vertices[idx.y]);
      radius.push_back(caps->vertices[idx.x].w);
      radius.push_back(caps->vertices[idx.y].w);
      if (!caps->colors.empty()) {
        color.push_back(caps->colors[idx.x]);
        color.push_back(caps->colors[idx.y]);
      }
    }
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(device, "curve");
    anari::setParameterArray1D
      (device, geom, "vertex.position",
       (const anari::math::float3*)position.data(),
       position.size());
    anari::setParameterArray1D
      (device, geom, "vertex.radius",
       (const float*)radius.data(),
       radius.size());
    anari::setParameterArray1D
      (device, geom, "primitive.index",
       (const uint32_t*)index.data(),
       index.size());
    if (!caps->colors.empty()) {
      anari::setParameterArray1D
        (device, geom, "vertex.color",
         (const anari::math::float4*)color.data(),
         color.size());
    }
    anari::commitParameters(device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", geom);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return { surface };
  }
  
  std::vector<anari::Surface>
  AnariBackend::Slot::createSpheres(SphereSet::SP content,
                                    MaterialLibrary<AnariBackend> *materialLib)
  { 
    // this is what minicontent looks like:
    // std::vector<vec3f> origins;
    // std::vector<vec3f> colors;
    // std::vector<float> radii;
    // mini::Material::SP material;
    // float radius = .1f;

    bool hasColor = !content->colors.empty();
    auto device = global->device;
    anari::Material material
      = materialLib->getOrCreate(content->material,hasColor);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(device, "sphere");
    anari::setParameterArray1D
      (device, geom, "vertex.position",
       (const anari::math::float3*)content->origins.data(),
       content->origins.size());
    if (!content->colors.empty()) {
      anari::setParameterArray1D
        (device, geom, "vertex.color",
         (const anari::math::float3*)content->colors.data(),
         content->origins.size());
    }
    if (content->radii.empty()) {
      anari::setParameter(device,geom,"radius",(float)content->radius);
    } else {
      anari::setParameterArray1D
        (device, geom, "vertex.radius",
         (const float*)content->radii.data(),
         content->radii.size());
    }

    anari::commitParameters(device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", geom);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return { surface };
  }

  std::vector<anari::Surface>
  AnariBackend::Slot::createTriangleMesh(TriangleMesh::SP content,
                                         MaterialLibrary<AnariBackend> *materialLib)
  {
    auto device = global->device;

    bool colorMapped = content->colors.size();
      
    anari::Material material = materialLib->getOrCreate(content->material,colorMapped);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(device, "triangle");
    anari::setParameterArray1D
      (device, geom, "vertex.position",
       (const anari::math::float3*)content->vertices.data(),
       content->vertices.size());
    if (!content->normals.empty()) {
      anari::setParameterArray1D
        (device, geom, "vertex.normal",
         (const anari::math::float3*)content->normals.data(),
         content->normals.size());
    }
    anari::setParameterArray1D
      (device, geom, "primitive.index",
       (const anari::math::uint3*)content->indices.data(),
       content->indices.size());
    if (!content->colors.empty()) {
      anari::setParameterArray1D
        (device, geom, "vertex.color",
         (const anari::math::float3*)content->colors.data(),
         content->colors.size());
    }
      
    anari::commitParameters(device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", geom);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return {surface};
  }
  
  std::vector<anari::Surface>
  AnariBackend::Slot::createCylinders(Cylinders::SP content,
                                      MaterialLibrary<AnariBackend> *materialLib)
  {
    auto device = global->device;

    bool colorMapped = content->colors.size();
      
    anari::Material material = materialLib->getOrCreate(content->material,colorMapped);
    anari::Geometry geom
      = anari::newObject<anari::Geometry>(device, "cylinder");
    anari::setParameterArray1D
      (device, geom, "vertex.position",
       (const anari::math::float3*)content->vertices.data(),
       content->vertices.size());
    if (content->radii.empty()) {
      std::vector<float> radii;
      for (int i=0;i<content->vertices.size();i++)
        radii.push_back((float)content->radius);
      anari::setParameterArray1D
        (device, geom, "primitive.radius",
         (const float*)radii.data(),
         radii.size());
    } else {
      anari::setParameterArray1D
        (device, geom, "primitive.radius",
         (const float*)content->radii.data(),
         content->radii.size());
    }

    if (colorMapped) {
      if (!content->colors.empty()) {
        std::vector<vec4f> color;
        for (auto col : content->colors)
          color.push_back(vec4f(col.x,col.y,col.z,1.f));
        if (color.size() == content->vertices.size()) {
          anari::setParameterArray1D
            (device, geom, "vertex.color",
             (const anari::math::float4*)color.data(),
             color.size());
        } else {
          anari::setParameterArray1D
            (device, geom, "primitive.color",
             // (device, geom, "vertex.color",
             (const anari::math::float4*)color.data(),
             color.size());
        }
      }
    }
    
    anari::commitParameters(device, geom);

    anari::Surface  surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", geom);
    anari::setParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    return { surface };
  }
  

} // ::hs
#endif

