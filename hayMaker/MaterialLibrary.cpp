// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/MaterialLibrary.h"
#include "hayMaker/SingleDeviceRenderer.h"
// #include "hayMaker/PerDevice.h"

namespace hm {

  ColorMapper ColorMapper::create(SingleDeviceRenderer *renderer,
                                  const range1f &inputRange,
                                  const std::vector<vec3f> &colorMap)
  {
    std::vector<vec4f> as4f;
    for (auto v : colorMap)
      as4f.push_back({v.x,v.y,v.z,1.f});
    anari::Sampler scalarMapper
      = anari::newObject<anari::Sampler>(renderer->anari.device,"image1D");
    anari::setParameterArray1D(renderer->anari.device, scalarMapper, "image",
                               (const anari::math::float4*)as4f.data(),as4f.size());
    float scale = 1.f / (inputRange.upper-inputRange.lower);
    struct {
      vec4f v0,v1,v2,v3;
    } xfm;
    xfm.v0={scale,0.f,0.f,-inputRange.lower/scale};
    xfm.v1={0.f,1.f,0.f,0.f};
    xfm.v2={0.f,0.f,1.f,0.f};
    xfm.v3={0.f,0.f,0.f,1.f};
    anariSetParameter(renderer->anari.device,scalarMapper,
                      "inTransform",
                      ANARI_FLOAT32_MAT4,
                      &xfm);
    anari::setParameter(renderer->anari.device,scalarMapper,"inAttribute","attribute0");
    anari::setParameter(renderer->anari.device,
                        scalarMapper,"filter","linear");
    anari::commitParameters(renderer->anari.device,scalarMapper);
    std::cout << "color mapper created" << std::endl;
    return { scalarMapper };
  }
  
  MaterialLibrary::MaterialLibrary(SingleDeviceRenderer *renderer)
    : renderer(renderer)
  {}

  MaterialLibrary::~MaterialLibrary()
  {
    for (auto it : alreadyCreated)
      anariRelease(renderer->anari.device,it.second);
  }

  anari::Material
  MaterialLibrary::getOrCreate(mini::Material::SP miniMat,
                               bool hasColorAttribute,
                               anari::Sampler colorSampler)
  {
    auto key = std::tuple<mini::Material::SP,bool,anari::Sampler>
      (miniMat,hasColorAttribute,colorSampler);
    if (alreadyCreated.find(key) != alreadyCreated.end())
      return alreadyCreated[key];

    auto matAndColorName = create(miniMat);
    auto mat = matAndColorName.first;
    const std::string colorName = matAndColorName.second;
    if (hasColorAttribute)
      anari::setParameter(renderer->anari.device,mat,
                          colorName.c_str(),"color");
    if (colorSampler)
      anari::setParameter(renderer->anari.device,mat,
                          colorName.c_str(),colorSampler);
    alreadyCreated[key] = mat;
    return mat;
  }

}
