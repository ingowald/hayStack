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

#if !NO_BARNEY
#include "BarneyBackend.h"

#if BARNEY_MPI
# pragma message("Barney has MPI enabled")
#else
# define HS_FAKE_MPI 1
#endif

namespace hs {

  BNGroup    BarneyBackend::Slot::createGroup(const std::vector<BNGeom> &geoms,
                                              const std::vector<BNVolume> &volumes)
  {
    BNGroup group
      = bnGroupCreate(global->context,this->slot,
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
    BNGeom geom = bnGeometryCreate(context,slot,"triangles");

    int numVertices = miniMesh->vertices.size();
    int numIndices = miniMesh->indices.size();
    const float2 *texcoords = (const float2*)miniMesh->texcoords.data();
    const float3 *vertices = (const float3*)miniMesh->vertices.data();
    const float3 *normals = (const float3*)miniMesh->normals.data();
    const int3 *indices = (const int3*)miniMesh->indices.data();
    BNData _vertices = bnDataCreate(context,slot,BN_FLOAT3,
                                    numVertices,vertices);
    bnSetAndRelease(geom,"vertices",_vertices);

    BNData _indices  = bnDataCreate(context,slot,BN_INT3,
                                    numIndices,indices);
    bnSetAndRelease(geom,"indices",_indices);

    if (normals) {
      BNData _normals  = bnDataCreate(context,slot,BN_FLOAT3,
                                      normals?numVertices:0,normals);
      bnSetAndRelease(geom,"normals",_normals);
    }

    if (texcoords) {
      BNData _texcoords  = bnDataCreate(context,slot,BN_FLOAT2,
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
            /*gpus*/nullptr, -1);
#else
      context = bnMPIContextCreate
        (base->world.comm,
         /*data*/dataGroupIDs.data(),dataGroupIDs.size(),
         /*gpus*/nullptr,-1);
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


#if 1
    vec4f gradient[2] = {
      vec4f(.9f,.9f,.9f,1.f),
      vec4f(0.15f, 0.25f, .8f,1.f),
    };
    // BNData data = bnDataCreate(context,-1,
    //                            BN_FLOAT4,2,
    //                            gradient);
    // BNTextureData tex = bnTextureData2DCreate(context,-1,
    //                                           BN_FLOAT4,1,2,
    //                                           gradient);
    BNTexture2D tex = bnTexture2DCreate(context,-1,
                                        BN_FLOAT4,1,2,
                                        gradient);
    bnSetObject(renderer,"bgTexture",tex);
#endif
    bnCommit(renderer);
    
    fb     = bnFrameBufferCreate(context,0);
    model  = bnModelCreate(context);
    camera = bnCameraCreate(context,"perspective");
  }

  void BarneyBackend::Global::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    this->fbSize = fbSize;
    this->hostRGBA = hostRGBA;
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,BN_FB_COLOR);
    // (base->world.rank==0)?hostRGBA:nullptr);
  }

//   void BarneyBackend::Slot::setTransferFunction(const std::vector<BNVolume> &rootVolumes,
//                                                 const TransferFunction &xf)
//   {
// #if 1
//     currentXF = xf;
//     needRebuild = true;
// #else
//     if (rootVolumes.empty())
//       return;

//     for (auto vol : rootVolumes) {
//       bnVolumeSetXF(vol,
//                     (float2&)xf.domain,
//                     (const float4*)xf.colorMap.data(),
//                     (int)xf.colorMap.size(),
//                     xf.baseDensity);
//       }
//     if (impl->volumeGroup)
//       bnGroupBuild(impl->volumeGroup);
//     needRebuild = true;
//     // bnBuild(global->context,this->slot);
// #endif
//   }

  void BarneyBackend::Slot::applyTransferFunction(const TransferFunction &xf)
  {
    if (impl->rootVolumes.empty())
      return;

    for (auto vol : impl->rootVolumes) {
      bnVolumeSetXF(vol,
                    (float2&)xf.domain,
                    (const float4*)xf.colorMap.data(),
                    (int)xf.colorMap.size(),
                    xf.baseDensity);
      }
    if (impl->volumeGroup)  {
      bnGroupBuild(impl->volumeGroup);
      bnGroupBuild(impl->volumeGroup);
    }
    // needRebuild = true;
    // bnBuild(global->context,this->slot);
  }
  
  void BarneyBackend::Global::renderFrame()
  {
    bnRender(renderer,model,camera,fb);
    bnFrameBufferRead(fb,BN_FB_COLOR,hostRGBA,BN_UFIXED8_RGBA);
  }

  void BarneyBackend::Global::resetAccumulation()
  {
    bnAccumReset(fb);
  }

  void BarneyBackend::Global::setCamera(const Camera &camera)
  {
    if (fbSize.x <= 0 || fbSize.y <= 0)
      throw std::runtime_error("trying to set camera, but window size not yet set - can't compute aspect");
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
    PING; PRINT(miniTex);
    if (!miniTex) return 0;
    BNDataType texelFormat;
    switch (miniTex->format) {
    case mini::Texture::FLOAT4:
      texelFormat = BN_FLOAT4_RGBA;
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
    PRINT(miniTex->size);
    BNTextureData texData
      = bnTextureData2DCreate(global->context,this->slot,
                              texelFormat,
                              miniTex->size.x,miniTex->size.y,
                              miniTex->data.data());
    BNSampler sampler
      = bnSamplerCreate(global->context,this->slot,"texture2D");
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

  BNMaterial BarneyBackend::Slot::create(mini::Plastic::SP plastic, bool colorMapped)
  {
#if 1
    BNMaterial mat = bnMaterialCreate(global->context,slot,"physicallyBased");
    bnSet3fc(mat,"baseColor",(const float3&)plastic->pigmentColor);
    bnSet1f(mat,"specular",.1f*plastic->Ks.x);
    bnSet1f(mat,"roughness",plastic->roughness);
    bnSet1f(mat,"ior",plastic->eta);
#else
    BNMaterial mat = bnMaterialCreate(global->context,slot,"plastic");
    bnSet3fc(mat,"pigmentColor",(const float3&)plastic->pigmentColor);
    bnSet3fc(mat,"Ks",(const float3&)plastic->Ks);
    bnSet1f(mat,"roughness",plastic->roughness);
    bnSet1f(mat,"eta",plastic->eta);
#endif
    bnCommit(mat);
    return mat;
  }
  
  BNMaterial BarneyBackend::Slot::create(mini::Velvet::SP velvet, bool colorMapped)
  {
    BNMaterial mat = bnMaterialCreate(global->context,slot,"velvet");
    bnSet3fc(mat,"reflectance",(const float3&)velvet->reflectance);
    bnSet3fc(mat,"horizonScatteringColor",(const float3&)velvet->horizonScatteringColor);
    bnSet1f(mat,"horizonScatteringFallOff",velvet->horizonScatteringFallOff);
    bnSet1f(mat,"backScattering",velvet->backScattering);
    bnCommit(mat);
    return mat;
  }
  
  BNMaterial BarneyBackend::Slot::create(mini::Matte::SP matte, bool colorMapped)
  {
#if 1
    BNMaterial mat = bnMaterialCreate(global->context,slot,"AnariMatte");
    vec3f color = matte->reflectance / 3.14f;
    bnSet3fc(mat,"color",(const float3&)color);
#else
    BNMaterial mat = bnMaterialCreate(global->context,slot,"matte");
    bnSet3fc(mat,"reflectance",(const float3&)matte->reflectance);
#endif
    std::cout << "committing " << (int*)mat << std::endl;
    bnCommit(mat);
    
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Metal::SP metal, bool colorMapped)
  {
#if 0
    BNMaterial mat = bnMaterialCreate(global->context,slot,"AnariMatte");
    bnSet3f(mat,"color",1.f,0.f,0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,slot,"metal");
    bnSet3fc(mat,"eta",(const float3&)metal->eta);
    bnSet3fc(mat,"k",(const float3&)metal->k);
    bnSet1f (mat,"roughness",metal->roughness);
#endif
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::BlenderMaterial::SP blender, bool colorMapped)
  {
    BNMaterial mat = bnMaterialCreate(global->context,slot,"AnariPBR");
    bnSet1f(mat,"metallic",blender->metallic);
    bnSet1f(mat,"ior",blender->ior);
    bnSet1f(mat,"roughness",blender->roughness);
    bnSet3fc(mat,"baseColor",(const float3&)blender->baseColor);
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::ThinGlass::SP thinGlass, bool colorMapped)
  {
#if 0
    BNMaterial mat = bnMaterialCreate(global->context,slot,"physicallyBased");
    bnSet1f (mat,"ior",1.45f);
    bnSet1f (mat,"transmission",1.f);
    bnSet1f (mat,"metallic",0.f);
    bnSet1f (mat,"specular",0.f);
    bnSet1f (mat,"roughness",0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,slot,"matte");
    vec3f gray(.5f);
    bnSet3fc(mat,"reflectance",(const float3&)gray);
#endif
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::Dielectric::SP dielectric, bool colorMapped)
  {
#if 1
    BNMaterial mat = bnMaterialCreate(global->context,slot,"physicallyBased");
    bnSet1f (mat,"ior",dielectric->etaInside);
    bnSet1f (mat,"transmission",1.f);
    bnSet1f (mat,"metallic",0.f);
    bnSet1f (mat,"specular",0.f);
    bnSet1f (mat,"roughness",0.f);
#else
    BNMaterial mat = bnMaterialCreate(global->context,slot,"glass");
    bnSet3fc(mat,"attenuationColorInside",(const float3&)dielectric->transmission);
    bnSet1f (mat,"etaInside",dielectric->etaInside);
    bnSet1f (mat,"etaOutside",dielectric->etaOutside);
    // BNMaterial mat = bnMaterialCreate(global->context,slot,"dielectric");
    // bnSet3fc(mat,"transmission",(const float3&)dielectric->transmission);
    // bnSet1f (mat,"etaInside",dielectric->etaInside);
    // bnSet1f (mat,"etaOutside",dielectric->etaOutside);
#endif
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::MetallicPaint::SP metallicPaint, bool colorMapped)
  {
    BNMaterial mat = bnMaterialCreate(global->context,slot,"blender");
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
    //     BNMaterial mat = bnMaterialCreate(global->context,slot,"metallic_paint");
    //     bnSet3fc(mat,"shadeColor",(const float3&)metallicPaint->shadeColor);
    //     bnSet3fc(mat,"glitterColor",(const float3&)metallicPaint->glitterColor);
    //     bnSet1f(mat,"glitterSpread",metallicPaint->glitterSpread);
    //     bnSet1f(mat,"eta",metallicPaint->eta);
    // #endif
    bnCommit(mat);
    return mat;
  }
  BNMaterial BarneyBackend::Slot::create(mini::DisneyMaterial::SP disney, bool colorMapped)
  {
    PRINT(disney->baseColor);
    BNMaterial mat = bnMaterialCreate(global->context,slot,"AnariPBR");
    // bnSet3fc(mat,"emission",
    //          (const float3&)disney->emission);
    PING;
    bnSet3fc(mat,"baseColor",(const float3&)disney->baseColor);
    PING;
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

    PING;
    if (disney->colorTexture) {
      PING;
      BNSampler tex = impl->textureLibrary.getOrCreate(disney->colorTexture);
      PING; PRINT(tex);
      bnSetObject(mat,"baseColor",tex);
    }
    PING;
    bnCommit(mat);
    PING;
    return mat;
  }

  BNMaterial BarneyBackend::Slot::create(mini::Material::SP miniMat, bool colorMapped)
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
      return create(blender,colorMapped);
    if (mini::Plastic::SP plastic = miniMat->as<mini::Plastic>())
      return create(plastic,colorMapped);
    if (mini::DisneyMaterial::SP disney = miniMat->as<mini::DisneyMaterial>())
      return create(disney,colorMapped);
    if (mini::Velvet::SP velvet = miniMat->as<mini::Velvet>())
      return create(velvet,colorMapped);
    if (mini::MetallicPaint::SP metallicPaint = miniMat->as<mini::MetallicPaint>())
      return create(metallicPaint,colorMapped);
    if (mini::Matte::SP matte = miniMat->as<mini::Matte>())
      return create(matte,colorMapped);
    if (mini::Metal::SP metal = miniMat->as<mini::Metal>())
      return create(metal,colorMapped);
    if (mini::Dielectric::SP dielectric = miniMat->as<mini::Dielectric>())
      return create(dielectric,colorMapped);
    if (mini::ThinGlass::SP thinGlass = miniMat->as<mini::ThinGlass>())
      return create(thinGlass,colorMapped);
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
  
  void BarneyBackend::Slot::finalizeSlot()
  {
   if (needRebuild) {
      bnBuild(global->model,slot);
      needRebuild = false;
    }
  }

  BNLight BarneyBackend::Slot::create(const mini::QuadLight &ml)
  {
    BNLight light = bnLightCreate(global->context,this->slot,"quad");
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
    BNLight light = bnLightCreate(global->context,this->slot,"directional");
    assert(light);
    bnSet3fc(light,"direction",(const float3&)ml.direction);
    bnSet3fc(light,"radiance",(const float3&)ml.radiance);
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
      = bnTexture2DCreate(global->context,this->slot,
                          BN_FLOAT4_RGBA,
                          size.x,size.y,
                          texels);
    
    BNLight light = bnLightCreate(global->context,this->slot,"envmap");

    bnSetObject(light,"texture", texture);
    bnRelease(texture);

    bnSet3fc(light,"direction",(const float3&)ml.transform.l.vx);
    bnSet3fc(light,"up",(const float3&)ml.transform.l.vz);
    
    bnCommit(light);
    return light;
  }

  void BarneyBackend::Slot::setLights(BNGroup rootGroup,
                                      const std::vector<BNLight> &lights)
  {
    if (!lights.empty()) {
      BNData lightsData = bnDataCreate(global->context,this->slot,
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
    
    BNTexture3D texture
      = bnTexture3DCreate(global->context,this->slot,
                          texelFormat,vol->dims.x,vol->dims.y,vol->dims.z,
                          vol->rawData.data());
    BNScalarField sf
      = bnScalarFieldCreate(global->context,this->slot,"structured");
    assert(sf);
    bnSetObject(sf,"texture",texture);
    bnRelease(texture);
    bnSet3ic(sf,"dims",(const int3&)vol->dims);
    bnSet3fc(sf,"gridOrigin",(const float3&)vol->gridOrigin);
    bnSet3fc(sf,"gridSpacing",(const float3&)vol->gridSpacing);
    bnCommit(sf);
    BNVolume volume = bnVolumeCreate(global->context,this->slot,sf);
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
    memcpy(indices.data()+tetBegin,mesh->tets.data(),  4*sizeof(int)*mesh->tets.size());
    memcpy(indices.data()+pyrBegin,mesh->pyrs.data(),  5*sizeof(int)*mesh->pyrs.size());
    memcpy(indices.data()+wedBegin,mesh->wedges.data(),6*sizeof(int)*mesh->wedges.size());
    memcpy(indices.data()+hexBegin,mesh->hexes.data(), 8*sizeof(int)*mesh->hexes.size());

    std::vector<int>     elementOffsets;
    // std::vector<uint8_t> elementTypes;
    for (int i=0;i<mesh->tets.size();i++) {
      elementOffsets.push_back(tetBegin+4*i);
      // elementTypes.push_back(VTK_TETRA);
    }
    for (int i=0;i<mesh->pyrs.size();i++) {
      elementOffsets.push_back(pyrBegin+5*i);
      // elementTypes.push_back(VTK_PYRAMID);
    }
    for (int i=0;i<mesh->wedges.size();i++) {
      elementOffsets.push_back(wedBegin+6*i);
      // elementTypes.push_back(VTK_WEDGE);
    }
    for (int i=0;i<mesh->hexes.size();i++) {
      elementOffsets.push_back(hexBegin+8*i);
      // elementTypes.push_back(VTK_HEXAHEDRON);
    }

    assert(mesh->perVertex);
    assert(mesh->perVertex->values.size() == mesh->vertices.size());
    std::vector<vec4f> vertices;
    for (int i=0;i<mesh->vertices.size();i++) {
      vec4f v;
      (umesh::vec3f&)v = mesh->vertices[i];
      v.w = mesh->perVertex->values[i];
      vertices.push_back(v);
    }
    // for (int i=0;i<mesh->hexes.size();i++)
    //   makeVTKOrder(vertices.data(),
                   
    BNScalarField sf = bnUMeshCreate(global->context,this->slot,
                                     (float4*)vertices.data(),vertices.size(),
                                     indices.data(),indices.size(),
                                     elementOffsets.data(),elementOffsets.size(),
                                     nullptr);
    BNVolume volume = bnVolumeCreate(global->context,this->slot,sf);
    bnRelease(sf);
    return volume;
  }

  std::vector<BNGeom>
  BarneyBackend::Slot
  ::createCapsules(hs::Capsules::SP content,
                   MaterialLibrary<BarneyBackend> *materialLib)
  {
    BNGeom geom
      = bnGeometryCreate(global->context,this->slot,"capsules");
    if (!geom) {
      std::cout << "barney backend doesn't support 'capsules' geometry"
                << std::endl;
      return {};
    }
    BNData vertices
      = bnDataCreate(global->context,this->slot,BN_FLOAT4,
                     content->vertices.size(),
                     content->vertices.data());
    bnSetAndRelease(geom,"vertices",vertices);
    BNData colors
      = bnDataCreate(global->context,this->slot,BN_FLOAT4,
                     content->colors.size(),
                     content->colors.data());
    bnSetAndRelease(geom,"vertex.color",colors);
    BNData indices
      = bnDataCreate(global->context,this->slot,BN_INT2,
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
      = bnGeometryCreate(global->context,this->slot,"spheres");
    BNData origins
      = bnDataCreate(global->context,this->slot,BN_FLOAT3,
                     content->origins.size(),
                     content->origins.data());
    bnSetAndRelease(geom,"origins",origins);

    if (content->radii.empty()) {
      bnSet1f(geom,"radius",content->radius);
    } else {
      BNData radii
        = bnDataCreate(global->context,this->slot,BN_FLOAT,
                       content->radii.size(),
                       content->radii.data());
      bnSetAndRelease(geom,"radii",radii);
    }

    if (content->colors.empty()) {
    } else {
      BNData colors
        = bnDataCreate(global->context,this->slot,BN_FLOAT3,
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
  BarneyBackend::Slot::createCylinders(Cylinders::SP content)
  {
    BNGeom geom = bnGeometryCreate(global->context,this->slot,"cylinders");
    if (!geom) return {};
    // std::vector<vec3f> vertices;
    // std::vector<vec3f> colors;
    // std::vector<vec2i>  indices;
    // std::vector<float> radii;

    // bool colorPerVertex  = false;
    // bool radiusPerVertex = false;
    // bool roundedCap      = false;
    BNData vertices = bnDataCreate(global->context,this->slot,BN_FLOAT3,
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
    BNData indices = bnDataCreate(global->context,this->slot,BN_INT2,
                                  content->indices.size(),
                                  content->indices.data());
    bnSetAndRelease(geom,"indices",indices);

    BNData radii = bnDataCreate(global->context,this->slot,BN_FLOAT,
                                content->radii.size(),
                                content->radii.data());
    bnSetAndRelease(geom,"radii",radii);
    
    bnSet1i(geom,"radiusPerVertex",content->radiusPerVertex);
    // bnSet1i(geom,"colorPerVertex",content->colorPerVertex);
    // bnSet1i(geom,"roundedCap",0);
    
    BNData colors
      = content->colors.empty()
      ? nullptr
      : bnDataCreate(global->context,this->slot,BN_FLOAT3,
                     content->colors.size(),
                     content->colors.data());
    if (colors)
      bnSetAndRelease(geom,"primitive.color",colors);

    bnCommit(geom);
    return { geom };
  }

} // ::hs

#endif
