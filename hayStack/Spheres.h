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

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "hayStack/HayStack.h"

namespace hs {
  
  struct SphereSet {
    typedef std::shared_ptr<SphereSet> SP;

    static SP create() { return std::make_shared<SphereSet>(); }
    
    box3f getBounds() const;
    
    std::vector<vec3f> origins;
    
    /*! array of radii - can be empty (in which case `radius` applies
        for all spheres equally), but if non-empty it has to be the
        same size as `origins` */
    std::vector<float> radii;
    
    float radius = .1f;
  };

} // ::hs
