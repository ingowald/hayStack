// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include "hayStack/HayStack.h"

namespace hs {
  
  struct TriangleMesh {
    typedef std::shared_ptr<TriangleMesh> SP;

    static SP create() { return std::make_shared<TriangleMesh>(); }

    TriangleMesh() {};
    // TriangleMesh(const std::string &fileName);
    // void write(const std::string &fileName);
    
    BoundsData getBounds() const;
    
    std::vector<vec3f> vertices;
    std::vector<vec3f> normals;
    std::vector<vec3f> colors;
    std::vector<vec3i> indices;
    struct {
      std::vector<float> perVertex;
    } scalars;
    
    mini::Material::SP material;
  };

} // ::hs
