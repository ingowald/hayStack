// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayMaker/common.h"

namespace hm {
  struct SingleDeviceRenderer;
  
  struct ScalarMapper {
    static ScalarMapper create(SingleDeviceRenderer *renderer,
                               const range1f &inputRange,
                               const std::vector<vec3f> &colorMap);
    // void setOn(anari::Material material,
    //            const std::string &colorName)
    // { anari::setParameter(device,mat,colorName.c_str(),sampler); }
    
    anari::Sampler sampler;
  };

  struct ColorMapper {
    static ColorMapper create(SingleDeviceRenderer *renderer,
                              const range1f &inputRange,
                              const std::vector<vec3f> &colorMap);
    // void setOn(anari::Material material,
    //            const std::string &colorName)
    // {
    //   anari::setParameter(device,mat,colorName.c_str(),"color");
    //   anari::setParameter(device,mat,colorName.c_str(),sampler);
    // }
    
    anari::Sampler sampler;
  };
  
  /*! keeps track of which frontend materials have already been
    created on the backend, and returns handle to already created
    backend variant if it exists -- or creates one if this is not
    yet the case */
  struct MaterialLibrary {
    
    MaterialLibrary(SingleDeviceRenderer *renderer);
    ~MaterialLibrary();
    
    anari::Material getOrCreate(mini::Material::SP miniMat,
                               bool hasColorAttribute=false,
                               anari::Sampler colorSampler=0);

  private:
    /*! creates an anari::material for the given
        mini::Material. returns both teh anari handle ti created, as
        well as a string saying what the anari material's name for the
        color paramter is (ie, 'color' vs 'baseColor') */
    static std::pair<anari::Material,std::string>
    create(mini::Material::SP miniMat);
    
    std::map<
      /*key*/
      std::tuple<mini::Material::SP,bool,bool>,
      /* value */
      anari::Material> alreadyCreated;
    
    SingleDeviceRenderer *const renderer;
  };
  
}

