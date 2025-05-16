// ======================================================================== //
// Copyright 2025++ Ingo Wald                                               //
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

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "hayStack/HayStack.h"
#include <tinyAMR/Model.h>

namespace hs {

  /*! wraps around a tiny-amr model, and adds getBoudn() and
      getValueRange() */
  struct TAMRVolume {
    typedef std::shared_ptr<TAMRVolume> SP;
    
    TAMRVolume(tamr::Model::SP model,
               const vec3f &gridOrigin=vec3f(0.f),
               const vec3f &gridSpacing=vec3f(1.f))
      : model(model),
        gridOrigin(gridOrigin),
        gridSpacing(gridSpacing)
    {}

    box3f getBounds() const;
    range1f getValueRange() const;

    tamr::Model::SP model;
    const vec3f gridOrigin;
    const vec3f gridSpacing;
  };

}
