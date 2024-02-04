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

#include "hayStack/Cylinders.h"

namespace hs {
  
  box3f Cylinders::getBounds() const
  {
    box3f bounds;
    int   numCylinders = indices.empty() ? (vertices.size()/2) : indices.size();
    for (int i=0;i<numCylinders;i++) {
      vec2i idx = indices.empty() ? (vec2i(2*i)+vec2i(0,1)) : indices[i];
      float r = radii.empty()?radius:radii[i];
      vec3f a = vertices[idx.x];
      vec3f b = vertices[idx.y];
      bounds.extend(min(a,b)-r);
      bounds.extend(max(a,b)+r);
    }
    return bounds;
  }
  
} // ::hs
