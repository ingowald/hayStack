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

#include "hayStack/TriangleMesh.h"
#include "viewer/DataLoader.h"
#include <fstream>

namespace hs {

  TriangleMesh::TriangleMesh(const std::string &fileName)
  {
    std::ifstream in(fileName.c_str(),std::ios::binary);
    
    vertices = withHeader::loadVectorOf<vec3f>(in);
    normals = withHeader::loadVectorOf<vec3f>(in);
    colors = withHeader::loadVectorOf<vec3f>(in);
    indices = withHeader::loadVectorOf<vec3i>(in);
    scalars.perVertex = withHeader::loadVectorOf<float>(in);
  }
  
  void TriangleMesh::write(const std::string &fileName)
  {
    std::ofstream out(fileName.c_str(),std::ios::binary);
    
    withHeader::writeVector(out,vertices);
    withHeader::writeVector(out,normals);
    withHeader::writeVector(out,colors);
    withHeader::writeVector(out,indices);
    withHeader::writeVector(out,scalars.perVertex);
  }

  BoundsData TriangleMesh::getBounds() const
  {
    BoundsData bb;
    for (auto v : vertices) bb.spatial.extend(v);
    for (auto v : scalars.perVertex) bb.mapped.extend(v);
    return bb;
  }
  
}

