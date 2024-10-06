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
  
  struct Capsules {
    typedef std::shared_ptr<Capsules> SP;

    static SP create() { return std::make_shared<Capsules>(); }
    
    box3f getBounds() const;

    /*! vertices - position and radius */
    std::vector<vec4f> vertices;
      
      /*! per-elelemnt of per-vertex colors */
    std::vector<vec3f> vertexColors;

    /*! array of index pair; each such pair refers to two points in
        the points[] array; those are the begin/end of that
        capsule. If this array is empty, it is to be treated as if it
        contained {(0,1)(2,3),...,(numPoints-2,numPoints-1)} */
    std::vector<vec2i>  indices;
    mini::Material::SP material;
  };

} // ::hs
