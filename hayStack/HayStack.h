// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include <miniScene/Scene.h>

#define HAYSTACK_NYI() throw std::runtime_error(std::string(__PRETTY_FUNCTION__)+" not yet implemented")

namespace hs {
  using namespace mini;
  using range1f = mini::common::interval<float>;
  
  struct Camera {
    typedef enum { PERSPECTIVE, ORTHO } Type;
    
    Camera() { perspective.fovy = 60.f; }
    vec3f reproject(vec2f relScreenPos, float depth, float aspect);
    
    Type type = PERSPECTIVE;
    vec3f vp   = vec3f(0.f);
    vec3f vi   = vec3f(0.f);
    vec3f vu   = vec3f(0.f,1.f,0.f);
    float lensRadius = 0.f;
    float focalLength = 0.f;

    union {
      struct { float fovy; } perspective;
      struct { float height; } ortho;
    };
    float nearClip = 0.f;
  };

  struct DirLight {
    vec3f direction;
    vec3f radiance;
  };

  struct PointLight {
    vec3f position;
    vec3f power;
  };

  struct BoundsData {
    void extend(const BoundsData &other);

    /*! the usual spatial-only bounds, in world space */
    box3f   spatial;
    
    /*! range of all scalar fields */
    range1f scalars;
    
    /*! range of all (color-)mapped scalar fields, if present */
    range1f mapped;
  };


  inline std::ostream &operator<<(std::ostream &o, const BoundsData &bd)
  {
    o << "{spatial=" << bd.spatial
      << ":scalarField(s)=" << bd.scalars
      << ":mappedScalars=" << bd.mapped << "}";
    return o;
  }
  
  inline void BoundsData::extend(const BoundsData &other)
  {
    spatial.extend(other.spatial);
    scalars.extend(other.scalars);
    mapped.extend(other.mapped);
  }

  
} // ::hs
