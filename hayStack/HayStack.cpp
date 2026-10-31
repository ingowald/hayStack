// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "HayStack.h"

namespace hs {

  vec3f Camera::reproject(vec2f relScreenPos, float depth, float aspect)
  {
    vec3f from   = (const vec3f&)vp;
    vec3f dir_00 = normalize(vi-vp);
    
    vec3f dir_du = normalize(cross(dir_00, vu));
    vec3f dir_dv = normalize(cross(dir_du, dir_00));

    float image_u = relScreenPos.x;
    float image_v = relScreenPos.y;
    if (type == ORTHO) {
      vec3f dir = dir_00;
      vec3f org = from
        + (ortho.height*aspect*(image_u - .5f)) * dir_du
        + (ortho.height*(image_v - .5f)) * dir_dv;
      return org + depth*dir;
    } else {
      vec3f org = from;
      dir_00 *= (float)(1.f / (2.0f * tanf((0.5f * perspective.fovy)
                                           * (float)M_PI / 180.0f)));
      vec3f dir
        = dir_00
        + (1.f*aspect*(image_u - .5f)) * dir_du
        + (1.f*(image_v - .5f)) * dir_dv;
      dir = normalize(dir);
      return org + depth*dir;
    }
  }
}
