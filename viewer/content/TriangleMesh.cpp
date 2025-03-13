// ======================================================================== //
// Copyright 2025 Ingo Wald                                                 //
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

#include "viewer/content/TriangleMesh.h"

namespace hs {
  namespace content {
    
    /*! simple position/normal/color/index triangle meshes in binary format */
    VMDMesh::VMDMesh(const ResourceSpecifier &data,
                     int thisPartID)
    : data(data),
      fileSize(getFileSize(data.where)),
      thisPartID(thisPartID)
    {}
    
    void VMDMesh::create(DataLoader *loader,
                                const ResourceSpecifier &dataURL)
    {
      for (int i=0;i<dataURL.numParts;i++)
        loader->addContent(new VMDMesh(dataURL,i));
    }
    
    size_t VMDMesh::projectedSize()
    { return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts); }
    
    void   VMDMesh::executeLoad(DataRank &dataGroup, bool verbose)
    {
      std::ifstream in(data.where.c_str(),std::ios::binary);
      TriangleMesh::SP mesh = std::make_shared<TriangleMesh>();
      mesh->vertices = loadVectorOf<vec3f>(in);
      mesh->normals = loadVectorOf<vec3f>(in);
      mesh->colors = loadVectorOf<vec3f>(in);
      mesh->indices = loadVectorOf<vec3i>(in);
      mini::DisneyMaterial::SP mat = std::make_shared<mini::DisneyMaterial>();
      mat->metallic = 1.f;
      mat->roughness = 0.2f;
      mat->transmission = .7f;
      mesh->material = mat;//mini::Matte::create();
      if (data.numParts > 1)
        throw std::runtime_error("cannot split meshes yet");
      dataGroup.triangleMeshes.push_back(mesh);
    }
    
    std::string VMDMesh::toString() 
    {
      return "VMDMesh{fileName="+data.where+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    
  }
}
