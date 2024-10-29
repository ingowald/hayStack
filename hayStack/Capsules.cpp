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

#include "hayStack/Capsules.h"

namespace hs {
  
  box3f Capsules::getBounds() const
  {
    auto getSphere = [&](int vertexID) -> box3f {
      const vec4f v = vertices[vertexID];
      vec3f c = (const vec3f &)v;
      float r = v.w;
      return box3f(c-r,c+r);
    };
    box3f bounds;
    for (auto index : indices) {
      bounds.extend(getSphere(index.x));
      bounds.extend(getSphere(index.y));
    }
    PING; PRINT(bounds);
    return bounds;
  }
  
} // ::hs
