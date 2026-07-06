// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/MaterialLibrary.h"
#include "hayMaker/PerDevice.h"

namespace hm {

#if 0
  void AnariBackend::Slot::createColorMapper(const range1f &inputRange,
                                             const std::vector<vec3f> &colorMap)
  {
    std::vector<vec4f> as4f;
    for (auto v : colorMap)
      as4f.push_back({v.x,v.y,v.z,1.f});
    scalarMapper = anari::newObject<anari::Sampler>(device,"image1D");
    anari::setParameterArray1D(device, scalarMapper, "image",
                               (const anari::math::float4*)as4f.data(),as4f.size());
    float scale = 1.f / (inputRange.upper-inputRange.lower);
    struct {
      vec4f v0,v1,v2,v3;
    } xfm;
    xfm.v0={scale,0.f,0.f,-inputRange.lower/scale};
    xfm.v1={0.f,1.f,0.f,0.f};
    xfm.v2={0.f,0.f,1.f,0.f};
    xfm.v3={0.f,0.f,0.f,1.f};
    anariSetParameter(device,scalarMapper,
                      "inTransform",
                      ANARI_FLOAT32_MAT4,
                      &xfm);
    anari::setParameter(device,scalarMapper,"inAttribute","attribute0");
    anari::setParameter(device,scalarMapper,"filter","linear");
    anari::commitParameters(device,scalarMapper);
    std::cout << "color mapper created" << std::endl;
  }
#endif
  
  MaterialLibrary::MaterialLibrary(anari::Device device)
    : device(device)
  {}

  MaterialLibrary::~MaterialLibrary()
  {
    for (auto it : alreadyCreated)
      anariRelease(device,it.second);
  }

  anari::Material
  MaterialLibrary::getOrCreate(mini::Material::SP miniMat,
                               ColorMapper *colorMapper,
                               ScalarMapper *scalarMapper)
  {
    auto key = std::tuple<mini::Material::SP,
                          ColorMapper *,
                          ScalarMapper *>
      (miniMat,colorMapper,scalarMapper);
    if (alreadyCreated.find(key) != alreadyCreated.end())
      return alreadyCreated[key];

    auto matAndColorName = backend->create(miniMat);
    auto mat = matAndColorName.first;
    const std::string colorName = matAndColorName.second;
    if (colorMapped)
      anari::setParameter(device,mat,colorName.c_str(),"color");
      backend->setColorMapping(mat,colorName);
    if (scalarMapped)
    anari::setParameter(device,mat,colorName.c_str(),scalarMapper);
      backend->setScalarMapping(mat,colorName);
      
    alreadyCreated[key] = mat;
    return mat;
  }

}
