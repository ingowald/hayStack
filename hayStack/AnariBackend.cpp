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

  void AnariBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    anari::setParameter(device, frame, "size", (const anari::math::uint2&)fbSize);
    anari::setParameter(device, frame, "channel.color", ANARI_UFIXED8_VEC4);
    anari::setParameter(device, frame, "channel.depth", ANARI_FLOAT32);

    anari::commitParameters(device, frame);
  }

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

      anari::setAndReleaseParameter
        (device, model, "volume", anari::newArray1D(device, &vol));
      // anari::release(device, vol);
    }

    anari::commitParameters(device, impl->volumeGroup);
    anari::commitParameters(device, global->model);
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
    anari::setParameter(device,material,"alphaMode","mask");

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

  void AnariBackend::Global::finalizeRender()
  {
    anari::setParameter(device, frame, "world",    model);
    std::cout << "### COMMITTING FRAME" << std::endl;
    anari::commitParameters(device, frame);
  }

  anari::Light AnariBackend::Slot::create(const mini::DirLight &ml)
  {
    auto device = global->device;
    anari::Light light = anari::newObject<anari::Light>(device,"directional");
    assert(light);
    anari::setParameter(device,light,"direction",(const anari::math::float3&)ml.direction);
    anari::setParameter(device,light,"irradiance",(const anari::math::float3&)ml.radiance);
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
    anari::setAndReleaseParameter(device, volume, "value", field);
    anari::commitParameters(device, volume);

    return volume;
  }

  anari::Volume AnariBackend::Slot::create(const std::pair<umesh::UMesh::SP,box3f> &v)
  {
    return {};
  }
  
  std::vector<anari::Surface>
  AnariBackend::Slot::createSpheres(SphereSet::SP content)
  { return {}; }
    
  std::vector<anari::Surface>
  AnariBackend::Slot::createCylinders(Cylinders::SP content)
  { return {}; }
  

} // ::hs
#endif

