DEPRECATED

// ======================================================================== //
// Copyright 2022-2025 Ingo Wald                                            //
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

#if !NO_BARNEY
#include "BarneyBackend.h"

namespace hs {

  void BarneyBackend::Slot::createColorMapper(const range1f &inputRange,
                                              const std::vector<vec3f> &colorMap)
  {
    std::vector<vec4f> as4f;
    for (auto v : colorMap)
      as4f.push_back({v.x,v.y,v.z,1.f});
    int width = as4f.size();
    int height = 1;

    BNTextureData td
      = bnTextureData2DCreate(global->context,
                              this->localDataSlotWeWorkOn,
                              BN_FLOAT4, width, height,
                              as4f.data());
    
    BNSampler sampler = bnSamplerCreate(global->context, this->localDataSlotWeWorkOn,
                                        "texture2D");
    bnSetObject(sampler, "textureData", td);
    
    float scale = 1.f / (inputRange.upper-inputRange.lower);
    struct {
      vec4f v0,v1,v2,v3;
    } xfm;
    xfm.v0={scale,0.f,0.f,-inputRange.lower/scale};
    xfm.v1={0.f,1.f,0.f,0.f};
    xfm.v2={0.f,0.f,1.f,0.f};
    xfm.v3={0.f,0.f,0.f,1.f};
    bnSet4x4fv(sampler,"inTransform",(const bn_float4*)&xfm);
    bnSetString(sampler,"inAttribute","attribute0");
    bnSet1i(sampler,"filterMode",BN_TEXTURE_LINEAR);
    bnCommit(sampler);
    this->scalarMapper = sampler;
  }

  void BarneyBackend::Slot::setColorMapping(BNMaterial mat, const std::string &colorName)
  {
    bnSetString(mat,colorName.c_str(),"color");
  }
  
  void BarneyBackend::Slot::setScalarMapping(BNMaterial mat, const std::string &colorName)
  {
    bnSetObject(mat,colorName.c_str(),scalarMapper);
    bnCommit(mat);
  }
  
  BNGroup    BarneyBackend::Slot::createGroup(const std::vector<BNGeom> &geoms,
                                              const std::vector<BNVolume> &volumes)
  {
    BNGroup group
      = bnGroupCreate(global->context,this->localDataSlotWeWorkOn,
                      (BNGeom *)geoms.data(),(int)geoms.size(),
                      (BNVolume *)volumes.data(),volumes.size());
    bnGroupBuild(group);
    return group;
  }
  
  BarneyBackend::GeomHandle
  BarneyBackend::Slot::create(mini::Mesh::SP miniMesh,
                              MaterialLibrary<BarneyBackend> *materialLib)
  {
    auto model = global->model;
    auto context = global->context;
    BNMaterial mat = materialLib->getOrCreate(miniMesh->material);
    BNGeom geom = bnGeometryCreate(context,localDataSlotWeWorkOn,"triangles");

    int numVertices = miniMesh->vertices.size();
    int numIndices = miniMesh->indices.size();
    const bn_float2 *texcoords = (const bn_float2*)miniMesh->texcoords.data();
    const bn_float3 *vertices = (const bn_float3*)miniMesh->vertices.data();
    const bn_float3 *normals = (const bn_float3*)miniMesh->normals.data();
    const bn_int3 *indices = (const bn_int3*)miniMesh->indices.data();
    BNData _vertices = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT3,
                                    numVertices,vertices);
    bnSetAndRelease(geom,"vertices",_vertices);

    BNData _indices  = bnDataCreate(context,localDataSlotWeWorkOn,BN_INT3,
                                    numIndices,indices);
    bnSetAndRelease(geom,"indices",_indices);

    if (normals) {
      BNData _normals  = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT3,
                                      normals?numVertices:0,normals);
      bnSetAndRelease(geom,"normals",_normals);
    }

    if (texcoords) {
      BNData _texcoords  = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT2,
                                        texcoords?numVertices:0,texcoords);
      bnSetData(geom,"vertex.attribute0",_texcoords);
      // bnSetAndRelease(geom,"texcoords",_texcoords);
      bnRelease(_texcoords);
    }
    bnSetObject(geom,"material",mat);
    bnCommit(geom);
    return geom;
  }

  BarneyBackend::Global::Global(HayMaker *base)
    : base(base)
  {
    bool isActiveWorker = !base->localModel.empty();
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : base->localModel.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
#if HS_FAKE_MPI
      context = bnContextCreate
        (   /*data*/dataGroupIDs.data(), (int)dataGroupIDs.size(),
            /*gpus*/base->gpuIDs.data(), /*-1 by default*/base->gpuIDs.size());
#else
      context = bnMPIContextCreate
        (base->world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
            /*gpus*/base->gpuIDs.data(), /*-1 by default*/base->gpuIDs.size());
#endif
    } else {
#if HS_FAKE_MPI
      context = bnContextCreate
        (/*data*/nullptr, 0,/*gpus*/nullptr, 0);
#else
      context = bnMPIContextCreate
        (base->world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);
#endif
    }

    renderer = bnRendererCreate(context,"default");
    bnSet1i(renderer,"pathsPerPixel",base->pixelSamples);
    bnSet1f(renderer,"ambientRadiance",0.5f);

    if (isnan(base->bgColor.x) || base->bgColor.x < 0.f) {
      vec4f gradient[2] = {
        vec4f(.9f,.9f,.9f,1.f),
        vec4f(0.15f, 0.25f, .8f,1.f),
      };
      BNTexture2D tex = bnTexture2DCreate(context,-1,
                                          BN_FLOAT4,1,2,
                                          gradient,
                                          BN_TEXTURE_LINEAR,
                                          BN_TEXTURE_CLAMP,BN_TEXTURE_CLAMP);
      bnSetObject(renderer,"bgTexture",tex);
    } else {
      bnSet4f(renderer,"bgColor",
              base->bgColor.x,
              base->bgColor.y,
              base->bgColor.z,
              base->bgColor.w);
    }
    bnCommit(renderer);
    
    fb     = bnFrameBufferCreate(context,0);
    model  = bnModelCreate(context);
    camera = bnCameraCreate(context,"perspective");
  }

  void BarneyBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    bnFrameBufferResize(fb,BN_UFIXED8_RGBA_SRGB,fbSize.x,fbSize.y,BN_FB_COLOR);
  }

  void BarneyBackend::Slot::applyTransferFunction(const TransferFunction &xf)
  {
    if (impl->rootVolumes.empty())
      return;

    for (auto vol : impl->rootVolumes) {
      bnVolumeSetXF(vol,
                    (bn_float2&)xf.domain,
                    (const bn_float4*)xf.colorMap.data(),
                    (int)xf.colorMap.size(),
                    xf.baseDensity);
      }
    if (impl->volumeGroup)  {
      bnGroupBuild(impl->volumeGroup);
      bnGroupBuild(impl->volumeGroup);
    }
  }
  
  void BarneyBackend::Global::renderFrame()
  {
    // resetAccumulation();
    bnRender(renderer,model,camera,fb);
    bnFrameBufferRead(fb,BN_FB_COLOR,hostRGBA,BN_UFIXED8_RGBA_SRGB);
  }

  void BarneyBackend::Global::resetAccumulation()
  {
    bnAccumReset(fb);
  }

  void BarneyBackend::Global::setCamera(const Camera &camera)
  {
    if (fbSize.x <= 0 || fbSize.y <= 0)
      throw std::runtime_error("trying to set camera, but window "
                               "size not yet set - can't compute aspect");
    vec3f dir = camera.vi - camera.vp;
    bnSet(this->camera,"direction",(const bn_float3&)dir);
    bnSet(this->camera,"position", (const bn_float3&)camera.vp);
    bnSet(this->camera,"up",       (const bn_float3&)camera.vu);
    bnSet1f (this->camera,"fovy",     camera.fovy);
    bnSet1f (this->camera,"aspect",   fbSize.x / float(fbSize.y));
    bnCommit(this->camera);
  }

  BNSampler BarneyBackend::Slot::create(mini::Texture::SP miniTex)
  {
    if (!miniTex) return 0;
    BNDataType texelFormat;
    switch (miniTex->format) {
    case mini::Texture::FLOAT4:
      texelFormat = BN_FLOAT4;
      break;
    case mini::Texture::FLOAT1:
      texelFormat = BN_FLOAT;
      break;
    case mini::Texture::RGBA_UINT8:
      texelFormat = BN_UFIXED8_RGBA;
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
    BNTextureData texData = bnTextureData2DCreate(global->context,this->localDataSlotWeWorkOn,
                                                  texelFormat,
                                                  miniTex->size.x,miniTex->size.y,
                                                  miniTex->data.data());
    BNSampler sampler = bnSamplerCreate(global->context,this->localDataSlotWeWorkOn,"texture2D");
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

  std::pair<BNMaterial,std::string> BarneyBackend::Slot::create(mini::Plastic::SP plastic)
  {
#if 1
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"physicallyBased");
    bnSet(mat,"baseColor",(const bn_float3&)plastic->pigmentColor);
    bnSet1f(mat,"specular",.1f*plastic->Ks.x);
    bnSet1f(mat,"roughness",plastic->roughness);
    bnSet1f(mat,"ior",plastic->eta);
#else
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"plastic");
    bnSet(mat,"pigmentColor",(const bn_float3&)plastic->pigmentColor);
    bnSet(mat,"Ks",(const bn_float3&)plastic->Ks);
    bnSet1f(mat,"roughness",plastic->roughness);
    bnSet1f(mat,"eta",plastic->eta);
#endif
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  
  std::pair<BNMaterial,std::string> BarneyBackend::Slot::create(mini::Velvet::SP velvet)
  {
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"velvet");
    bnSet(mat,"reflectance",(const bn_float3&)velvet->reflectance);
    bnSet(mat,"horizonScatteringColor",(const bn_float3&)velvet->horizonScatteringColor);
    bnSet1f(mat,"horizonScatteringFallOff",velvet->horizonScatteringFallOff);
    bnSet1f(mat,"backScattering",velvet->backScattering);
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  
  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::Matte::SP matte)
  {
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"AnariMatte");
    vec3f color = matte->reflectance / 3.14f;
    bnSet(mat,"color",(const bn_float3&)color);
    bnCommit(mat);
    
    return {mat,"color"};
  }
  
  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::Metal::SP metal)
  {
#if 0
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"AnariMatte");
    bnSet(mat,"color",1.f,0.f,0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"metal");
    bnSet(mat,"eta",(const bn_float3&)metal->eta);
    bnSet(mat,"k",(const bn_float3&)metal->k);
    bnSet1f (mat,"roughness",metal->roughness);
#endif
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  
  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::BlenderMaterial::SP blender)
  {
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"AnariPBR");
    bnSet1f(mat,"metallic",blender->metallic);
    bnSet1f(mat,"ior",blender->ior);
    bnSet1f(mat,"roughness",blender->roughness);
    bnSet(mat,"baseColor",(const bn_float3&)blender->baseColor);
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  
  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::ThinGlass::SP thinGlass)
  {
#if 0
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"physicallyBased");
    bnSet1f (mat,"ior",1.45f);
    bnSet1f (mat,"transmission",1.f);
    bnSet1f (mat,"metallic",0.f);
    bnSet1f (mat,"specular",0.f);
    bnSet1f (mat,"roughness",0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"matte");
    vec3f gray(.5f);
    bnSet(mat,"reflectance",(const bn_float3&)gray);
#endif
    bnCommit(mat);
    return {mat,"reflectance"};
  }
  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::Dielectric::SP dielectric)
  {
#if 1
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"physicallyBased");
    bnSet1f (mat,"ior",dielectric->etaInside);
    bnSet1f (mat,"transmission",1.f);
    bnSet1f (mat,"metallic",0.f);
    bnSet1f (mat,"specular",0.f);
    bnSet1f (mat,"roughness",0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"glass");
    bnSet(mat,"attenuationColorInside",(const bn_float3&)dielectric->transmission);
    bnSet1f (mat,"etaInside",dielectric->etaInside);
    bnSet1f (mat,"etaOutside",dielectric->etaOutside);
    // BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"dielectric");
    // bnSet(mat,"transmission",(const bn_float3&)dielectric->transmission);
    // bnSet1f (mat,"etaInside",dielectric->etaInside);
    // bnSet1f (mat,"etaOutside",dielectric->etaOutside);
#endif
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  std::pair<BNMaterial,std::string> BarneyBackend::Slot::create(mini::MetallicPaint::SP metallicPaint)
  {
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"blender");
    bnSet(mat,"baseColor",(const bn_float3&)metallicPaint->shadeColor);
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
    //     BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"metallic_paint");
    //     bnSet(mat,"shadeColor",(const bn_float3&)metallicPaint->shadeColor);
    //     bnSet(mat,"glitterColor",(const bn_float3&)metallicPaint->glitterColor);
    //     bnSet1f(mat,"glitterSpread",metallicPaint->glitterSpread);
    //     bnSet1f(mat,"eta",metallicPaint->eta);
    // #endif
    bnCommit(mat);
    return {mat,"baseColor"};
  }
  std::pair<BNMaterial,std::string> BarneyBackend::Slot::create(mini::DisneyMaterial::SP disney)
  {
#if 0
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"AnariMatte");
    bnSet(mat,"color",(const bn_float3&)disney->baseColor);
    bnCommit(mat);
    
    return {mat,"color"};
#else
    BNMaterial mat = bnMaterialCreate(global->context,localDataSlotWeWorkOn,"AnariPBR");
    bnSet(mat,"baseColor",(const bn_float3&)disney->baseColor);
    bnSet1f(mat,"roughness",   disney->roughness);
    bnSet1f(mat,"metallic",    disney->metallic);
    bnSet1f(mat,"transmission",
#if 0
            disney->ior < 1.1f
            ? 0.f
            :
#endif
            disney->transmission);
    bnSet1f(mat,"ior",         disney->ior);
    if (disney->ior == 1.f) {
      bnSet1f(mat,"opacity",1.f-disney->transmission);
    }

    if (disney->colorTexture) {
      BNSampler tex = impl->textureLibrary.getOrCreate(disney->colorTexture);
      bnSetObject(mat,"baseColor",tex);
    }
    bnCommit(mat);
    return {mat,"baseColor"};
#endif
  }

  std::pair<BNMaterial,std::string>
  BarneyBackend::Slot::create(mini::Material::SP miniMat)
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
    if (mini::BlenderMaterial::SP blender = miniMat->as<mini::BlenderMaterial>())
      return create(blender);
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

  void BarneyBackend::Slot::setInstances(const std::vector<BNGroup> &groups,
                                         const std::vector<affine3f> &xfms)
  {
    bnSetInstances(global->model,this->localDataSlotWeWorkOn,
                   (BNGroup*)groups.data(),(BNTransform *)xfms.data(),
                   (int)groups.size());
    bnBuild(global->model,localDataSlotWeWorkOn);
  }


  void BarneyBackend::Global::finalizeRender()
  {
  }
  
  void BarneyBackend::Slot::finalizeSlot()
  {
    if (needRebuild) {
      bnBuild(global->model,localDataSlotWeWorkOn);
      needRebuild = false;
    }
  }

  BNLight BarneyBackend::Slot::create(const mini::QuadLight &ml)
  {
    BNLight light = bnLightCreate(global->context,this->localDataSlotWeWorkOn,"quad");
    assert(light);
    bnSet(light,"corner",(const bn_float3&)ml.corner);
    bnSet(light,"edge0",(const bn_float3&)ml.edge0);
    bnSet(light,"edge1",(const bn_float3&)ml.edge1);
    bnSet(light,"emission",(const bn_float3&)ml.emission);
    bnCommit(light);
    return light;
  }

  BNLight BarneyBackend::Slot::create(const mini::DirLight &ml)
  {
    BNLight light = bnLightCreate(global->context,this->localDataSlotWeWorkOn,"directional");
    assert(light);
    bnSet(light,"direction",(const bn_float3&)ml.direction);
    bnSet(light,"radiance",(const bn_float3&)ml.radiance);
    bnCommit(light);
    return light;
  }

  BNLight BarneyBackend::Slot::create(const mini::EnvMapLight &ml)
  {
    std::cout << MINI_TERMINAL_YELLOW
              << "#hs: creating env-map light ..."
              << MINI_TERMINAL_DEFAULT << std::endl;
    vec2i size = ml.texture->size;
    assert(ml.texture);

    const vec4f *texels
      = (const vec4f *)ml.texture->data.data();
    BNTexture2D texture
      = bnTexture2DCreate(global->context,this->localDataSlotWeWorkOn,
                          BN_FLOAT4,
                          size.x,size.y,
                          texels);
    
    BNLight light = bnLightCreate(global->context,this->localDataSlotWeWorkOn,"envmap");

    bnSetObject(light,"texture", texture);
    bnRelease(texture);

    vec3f up = ml.transform.l.vz;
    vec3f dir = - ml.transform.l.vx;
    bnSet(light,"direction",(const bn_float3&)dir);
    bnSet(light,"up",(const bn_float3&)up);
    
    bnCommit(light);
    return light;
  }

  void BarneyBackend::Slot::setLights(BNGroup rootGroup,
                                      const std::vector<BNLight> &lights)
  {
    if (!lights.empty()) {
      std::cout << "setting lights slot " << this->localDataSlotWeWorkOn << std::endl;
      BNData lightsData = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                                       BN_OBJECT,lights.size(),lights.data());
      bnSetData(rootGroup,"lights",lightsData);
      bnRelease(lightsData);
    }
    bnCommit(rootGroup);
    bnGroupBuild(rootGroup);
  }

  BNVolume BarneyBackend::Slot::create(const StructuredVolume::SP &vol)
  {
    BNDataType texelFormat;
    if (vol->texelFormat == "float")
      texelFormat = BN_FLOAT;
    else if (vol->texelFormat == "uint8_t")
      texelFormat = BN_UFIXED8;
    else if (vol->texelFormat == "uint16_t")
      texelFormat = BN_UFIXED16;
    else
      throw std::runtime_error("unsupported voluem format '"+vol->texelFormat+"'");
    
    BNTextureData td
      = bnTextureData3DCreate(global->context,this->localDataSlotWeWorkOn,
                              texelFormat,vol->dims.x,vol->dims.y,vol->dims.z,
                              vol->rawData.data());
    BNScalarField sf
      = bnScalarFieldCreate(global->context,this->localDataSlotWeWorkOn,"structured");
    assert(sf);
    bnSetObject(sf,"textureData",td);
    bnRelease(td);
    bnSet(sf,"dims",(const bn_int3&)vol->dims);
    bnSet(sf,"gridOrigin",(const bn_float3&)vol->gridOrigin);
    bnSet(sf,"gridSpacing",(const bn_float3&)vol->gridSpacing);
    bnCommit(sf);
    BNVolume volume = bnVolumeCreate(global->context,this->localDataSlotWeWorkOn,sf);
    bnRelease(sf);
    return volume;
  }

  BNVolume BarneyBackend::Slot::create(const TAMRVolume::SP &model)
  {
    auto input = model->model;
    
    std::vector<vec3i>    origins;
    std::vector<vec3i>    dims;
    std::vector<int>      levels;
    std::vector<uint64_t> offsets;
    const std::vector<int> &refinements = input->refinementOfLevel;

    for (auto &grid : input->grids) {
      origins.push_back((const vec3i&)grid.origin);
      dims.push_back((const vec3i&)grid.dims);
      offsets.push_back(grid.offset);
      levels.push_back(grid.level);
    }
    PRINT(dims.size());
    BNScalarField sf
      = bnScalarFieldCreate(global->context,this->localDataSlotWeWorkOn,"BlockStructuredAMR");
    BNData originsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT3,origins.size(),origins.data());
    bnSetData(sf,"grid.origins",originsData);
    BNData dimsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT3,dims.size(),dims.data());
    bnSetData(sf,"grid.dims",dimsData);
    BNData levelsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT,levels.size(),levels.data());
    bnSetData(sf,"grid.levels",levelsData);
    BNData offsetsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_LONG,offsets.size(),offsets.data());
    bnSetData(sf,"grid.offsets",offsetsData);

    BNData scalarsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_FLOAT,input->numCellsAcrossAllGrids,
                     input->scalars.data());
    bnSetData(sf,"scalars",scalarsData);
    BNData refinementsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT,refinements.size(),refinements.data());
    bnSetData(sf,"level.refinements",refinementsData);
    bnCommit(sf);
    
    BNVolume volume = bnVolumeCreate(global->context,this->localDataSlotWeWorkOn,sf);
    bnRelease(sf);
    return volume;
  }
  
  BNVolume BarneyBackend::Slot::create(const std::pair<umesh::UMesh::SP,box3f> &up)
  {
    umesh::UMesh::SP mesh = up.first;
    int tetBegin = 0;
    int pyrBegin = tetBegin + 4 * mesh->tets.size();
    int wedBegin = pyrBegin + 5 * mesh->pyrs.size();
    int hexBegin = wedBegin + 6 * mesh->wedges.size();
    int numIndices = hexBegin + 8 * mesh->hexes.size();
    std::vector<int> indices(numIndices);
    memcpy(indices.data()+tetBegin,mesh->tets.data(),
           4*sizeof(int)*mesh->tets.size());
    memcpy(indices.data()+pyrBegin,mesh->pyrs.data(),
           5*sizeof(int)*mesh->pyrs.size());
    memcpy(indices.data()+wedBegin,mesh->wedges.data(),
           6*sizeof(int)*mesh->wedges.size());
    memcpy(indices.data()+hexBegin,mesh->hexes.data(),
           8*sizeof(int)*mesh->hexes.size());

    std::vector<int>     elementOffsets;
    std::vector<uint8_t> cellTypes;
    for (int i=0;i<mesh->tets.size();i++) {
      elementOffsets.push_back(tetBegin+4*i);
      cellTypes.push_back(BN_UNSTRUCTURED_TET);
    }
    for (int i=0;i<mesh->pyrs.size();i++) {
      elementOffsets.push_back(pyrBegin+5*i);
      cellTypes.push_back(BN_UNSTRUCTURED_PYRAMID);
    }
    for (int i=0;i<mesh->wedges.size();i++) {
      elementOffsets.push_back(wedBegin+6*i);
      cellTypes.push_back(BN_UNSTRUCTURED_PRISM);
    }
    for (int i=0;i<mesh->hexes.size();i++) {
      elementOffsets.push_back(hexBegin+8*i);
      cellTypes.push_back(BN_UNSTRUCTURED_HEX);
    }
    
    assert(mesh->perVertex);
    assert(mesh->perVertex->values.size() == mesh->vertices.size());
    
    BNScalarField sf
      = bnScalarFieldCreate(global->context,this->localDataSlotWeWorkOn,
                            "unstructured");
    BNData vertexData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_FLOAT3,
                     mesh->vertices.size(),
                     mesh->vertices.data());
    BNData cellTypeData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_UINT8,
                     cellTypes.size(),
                     cellTypes.data());
    BNData scalarData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_FLOAT,
                     mesh->perVertex->values.size(),
                     mesh->perVertex->values.data());
    BNData indicesData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT,indices.size(),indices.data());
    BNData offsetsData
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,
                     BN_INT,elementOffsets.size(),elementOffsets.data());
    bnSetAndRelease(sf,"vertex.position",vertexData);
    bnSetAndRelease(sf,"vertex.data",scalarData);
    bnSetAndRelease(sf,"index",indicesData);
    bnSetAndRelease(sf,"cell.index",offsetsData);
    bnSetAndRelease(sf,"cell.type",cellTypeData);
    bnCommit(sf);

    BNVolume volume = bnVolumeCreate(global->context,this->localDataSlotWeWorkOn,sf);
    bnRelease(sf);
    return volume;
  }

  std::vector<BNGeom>
  BarneyBackend::Slot
  ::createCapsules(hs::Capsules::SP content,
                   MaterialLibrary<BarneyBackend> *materialLib)
  {
    BNGeom geom
      = bnGeometryCreate(global->context,this->localDataSlotWeWorkOn,"capsules");
    BNData vertices
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT4,
                     content->vertices.size(),
                     content->vertices.data());
    bnSetAndRelease(geom,"vertices",vertices);
    BNData colors
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT4,
                     content->colors.size(),
                     content->colors.data());
    bnSetAndRelease(geom,"vertex.color",colors);
    BNData indices
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_INT2,
                     content->indices.size(),
                     content->indices.data());
    bnSetAndRelease(geom,"indices",indices);
    BNMaterial mat
      = materialLib->getOrCreate(content->material);
    bnSetString(mat,"baseColor","color");
    bnSetString(mat,"color","color");
    bnCommit(mat);
    
    bnSetObject(geom,"material",mat);
    bnCommit(geom);
    
    return { geom };
  }
  
  
  std::vector<BNGeom>
  BarneyBackend::Slot
  ::createSpheres(SphereSet::SP content,
                  MaterialLibrary<BarneyBackend> *materialLib)
  { 

    // this is what minicontent looks like:
    // std::vector<vec3f> origins;
    // std::vector<vec3f> colors;
    // std::vector<float> radii;
    // mini::Material::SP material;
    // float radius = .1f;
    
    BNGeom geom
      = bnGeometryCreate(global->context,this->localDataSlotWeWorkOn,"spheres");
    BNData origins
      = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT3,
                     content->origins.size(),
                     content->origins.data());
    bnSetAndRelease(geom,"origins",origins);

    if (content->radii.empty()) {
      bnSet1f(geom,"radius",content->radius);
    } else {
      BNData radii
        = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT,
                       content->radii.size(),
                       content->radii.data());
      bnSetAndRelease(geom,"radii",radii);
    }

    if (content->colors.empty()) {
    } else {
      BNData colors
        = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT3,
                       content->colors.size(),
                       content->colors.data());
      // bnSetAndRelease(geom,"colors",colors);
      bnSetAndRelease(geom,"primitive.color",colors);
    }
    
    BNMaterial mat
      = materialLib->getOrCreate(content->material);
    bnSetObject(geom,"material",mat);
    
    bnCommit(geom);
    return { geom };
  }

  std::vector<BNGeom>
  BarneyBackend::Slot::createTriangleMesh(TriangleMesh::SP content,
                                          MaterialLibrary<BarneyBackend> *materialLib)
  {
    auto model = global->model;
    auto context = global->context;
    bool colorMapped = content->colors.size()>0;

    BNMaterial mat = materialLib->getOrCreate(content->material,colorMapped,
                                              content->scalars.perVertex.size()>0);
    BNGeom geom = bnGeometryCreate(context,localDataSlotWeWorkOn,"triangles");

    int numVertices = content->vertices.size();
    int numIndices = content->indices.size();
    const bn_float3 *vertices = (const bn_float3*)content->vertices.data();
    const bn_float3 *normals = (const bn_float3*)content->normals.data();
    const bn_float3 *colors = (const bn_float3*)content->colors.data();
    const bn_int3 *indices = (const bn_int3*)content->indices.data();
    const float *scalars = (const float*)content->scalars.perVertex.data();
    BNData _vertices = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT3,
                                    numVertices,vertices);
    bnSetAndRelease(geom,"vertices",_vertices);

    if (!content->colors.empty()) {
      BNData _colors = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT3,
                                    numVertices,colors);
      bnSetAndRelease(geom,"vertex.color",_colors);
    }

    if (!content->scalars.perVertex.empty()) {
      BNData _scalars = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT,
                                     numVertices,scalars);
      bnSetAndRelease(geom,"vertex.attribute0",_scalars);
    }

    BNData _indices  = bnDataCreate(context,localDataSlotWeWorkOn,BN_INT3,
                                    numIndices,indices);
    bnSetAndRelease(geom,"indices",_indices);

    if (normals) {
      BNData _normals  = bnDataCreate(context,localDataSlotWeWorkOn,BN_FLOAT3,
                                      normals?numVertices:0,normals);
      bnSetAndRelease(geom,"normals",_normals);
    }

    bnSetObject(geom,"material",mat);
    bnCommit(geom);
    return {geom};
  }
  
  std::vector<BNGeom>
  BarneyBackend::Slot::createCylinders(Cylinders::SP content,
                                       MaterialLibrary<BarneyBackend> *materialLib)
  {
    BNGeom geom = bnGeometryCreate(global->context,this->localDataSlotWeWorkOn,"cylinders");
    if (!geom) return {};
    BNData vertices = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT3,
                                   content->vertices.size(),
                                   content->vertices.data());
    bnSetAndRelease(geom,"vertices",vertices);

    if (content->indices.empty()) {
      std::cout << MINI_TERMINAL_RED
                << "#hs.bn.cyl: warning - empty indices array, creating default one"
                << MINI_TERMINAL_DEFAULT << std::endl;
      for (int i=0;i<content->vertices.size();i+=2)
        content->indices.push_back(vec2i(i,i+1));
    }
    BNData indices = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_INT2,
                                  content->indices.size(),
                                  content->indices.data());
    bnSetAndRelease(geom,"indices",indices);

    BNData radii = bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT,
                                content->radii.size(),
                                content->radii.data());
    bnSetAndRelease(geom,"radii",radii);
    
    BNData colors
      = content->colors.empty()
      ? nullptr
      : bnDataCreate(global->context,this->localDataSlotWeWorkOn,BN_FLOAT3,
                     content->colors.size(),
                     content->colors.data());
    if (colors) {
      if (content->colorPerVertex) {
        bnSetAndRelease(geom,"vertex.color",colors);
      } else {
        bnSetAndRelease(geom,"primitive.color",colors);
      }
    }

    bnCommit(geom);
    return { geom };
  }
  
  /*! clean up and shut down */
  void BarneyBackend::Global::terminate()
  {
    bnContextDestroy(context);
    context = 0;
  }
  
} // ::hs

#endif
