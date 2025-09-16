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

#pragma once

#include "hayStack/HayStack.h"

namespace hs {
  
  struct Cylinders {
    typedef std::shared_ptr<Cylinders> SP;

    static SP create() { return std::make_shared<Cylinders>(); }
    
    box3f getBounds() const;
    
    std::vector<vec3f> vertices;

    /*! per-elelemnt of per-vertex colors */
    std::vector<vec3f> colors;

    /*! array of index pair; each such pair refers to two points in
        the points[] array; those are the begin/end of that
        cylinder. If this array is empty, it is to be treated as if it
        contained {(0,1)(2,3),...,(numPoints-2,numPoints-1)} */
    std::vector<vec2i>  indices;
    
    /*! array of radii - can be empty (in which case `radius` applies
        for all spheres equally), but if non-empty it has to be the
        same size as `indices`, or same size of 'points' dependong on
        whether those are per-vertex or per-element radii */
    std::vector<float> radii;

    bool colorPerVertex  = false;
    bool radiusPerVertex = false;
    bool roundedCap      = false;
    
    /*! fall-back radius for all cylinders if radii array is empty */
    float radius = .1f;

    Material::SP material;
  };

} // ::hs
