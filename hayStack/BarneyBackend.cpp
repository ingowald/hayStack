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

#include "BarneyBackend.h"

namespace hs {

  BNGroup    BarneyBackend::Slot::createGroup(const std::vector<BNGeom> &geoms,
                                              const std::vector<BNVolume> &volumes)
  {
    BNGroup group
      = bnGroupCreate(global->model,this->slot,
                      (BNGeom *)geoms.data(),(int)geoms.size(),
                      (BNVolume *)volumes.data(),volumes.size());
    bnGroupBuild(group);
    return group;
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

  void BarneyBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(base->world.rank==0)?hostRGBA:nullptr);
  }

  void BarneyBackend::Slot::setTransferFunction(const std::vector<BNVolume> &rootVolumes,
                                                const TransferFunction &xf)
  {
    PING;
    if (rootVolumes.empty())
      return;

    for (auto vol : rootVolumes) {
      bnVolumeSetXF(vol,
                    (float2&)xf.domain,
                    (const float4*)xf.colorMap.data(),
                    (int)xf.colorMap.size(),
                    xf.baseDensity);
    }
    PING;
    bnGroupBuild(impl->volumeGroup);
    bnBuild(global->model,this->slot);
  }

  void BarneyBackend::Global::renderFrame(int pathsPerPixel)
  {
    bnRender(model,camera,fb,pathsPerPixel);
  }

  void BarneyBackend::Global::resetAccumulation()
  {
    bnAccumReset(fb);
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

  void BarneyBackend::Slot::setInstances(const std::vector<BNGroup> &groups,
                                         const std::vector<affine3f> &xfms)
  {
    bnSetInstances(global->model,this->slot,
                   (BNGroup*)groups.data(),(BNTransform *)xfms.data(),
                   (int)groups.size());
    bnBuild(global->model,slot);
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

  BNVolume BarneyBackend::Slot::create(const StructuredVolume::SP &vol)
  {
    BNTexture3D texture
      = bnTexture3DCreate(global->model,this->slot,
                          vol->texelFormat,vol->dims.x,vol->dims.y,vol->dims.z,
                          vol->rawData.data());
    BNScalarField sf
      = bnScalarFieldCreate(global->model,this->slot,"structured");
    assert(sf);
    bnSetObject(sf,"texture",texture);
    bnRelease(texture);
    bnSet3ic(sf,"dims",(const int3&)vol->dims);
    bnSet3fc(sf,"gridOrigin",(const float3&)vol->gridOrigin);
    bnSet3fc(sf,"gridSpacing",(const float3&)vol->gridSpacing);
    bnCommit(sf);
    BNVolume volume = bnVolumeCreate(global->model,this->slot,sf);
    bnRelease(sf);
    return volume;
  }

} // ::hs


