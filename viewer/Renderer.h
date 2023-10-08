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

#include "barney/common.h"
#include <vector>

/* parallel renderer abstraction */
namespace hs {
  using range1f = owl::common::interval<float> ;
  
  struct Camera {
    vec3f vp, vi, vu;
    float fovy;
  };

  struct DirLight {
    vec3f direction;
    vec3f radiance;
  };

  struct PointLight {
    vec3f position;
    vec3f power;
  };
  
  /*! base abstraction for any renderer - no matter whether its a
      single node or multiple workers on the back */
  struct Renderer {
    virtual void renderFrame() {}
    virtual void resize(const vec2i &fbSize, uint32_t *hostRgba) {}
    virtual void resetAccumulation() {}
    virtual void setCamera(const Camera &camera) {}
    virtual void setXF(const range1f &domain,
                       const std::vector<vec4f> &colors) {}
    virtual void screenShot() {}
    virtual void terminate() {}
    virtual void setLights(float ambient,
                           const std::vector<PointLight> &pointLights,
                           const std::vector<DirLight> &dirLights) {}
  };

}
