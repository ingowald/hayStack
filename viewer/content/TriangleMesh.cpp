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
      mesh->vertices = withHeader::loadVectorOf<vec3f>(in);
      mesh->normals = withHeader::loadVectorOf<vec3f>(in);
      mesh->colors = withHeader::loadVectorOf<vec3f>(in);
      mesh->indices = withHeader::loadVectorOf<vec3i>(in);
      mini::DisneyMaterial::SP mat = std::make_shared<mini::DisneyMaterial>();
      mat->metallic = .8f;
      mat->roughness = 0.2f;
      mat->transmission = .6f;
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



    /*! simple position/normal/color/index triangle meshes in binary format */
    RGBTris::RGBTris(const ResourceSpecifier &data,
                     int thisPartID)
    : data(data),
      fileSize(getFileSize(data.where)),
      thisPartID(thisPartID)
    {}
    
    void RGBTris::create(DataLoader *loader,
                                const ResourceSpecifier &dataURL)
    {
      for (int i=0;i<dataURL.numParts;i++)
        loader->addContent(new RGBTris(dataURL,i));
    }
    
    size_t RGBTris::projectedSize()
    { return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts); }
    
    void   RGBTris::executeLoad(DataRank &dataGroup, bool verbose)
    {
      PING;
      std::ifstream in(data.where.c_str(),std::ios::binary);
      TriangleMesh::SP mesh = std::make_shared<TriangleMesh>();
      struct Vtx {
        vec3f pos;
        vec3f rgb;
      };
      std::vector<Vtx> vertices
        = noHeader::loadVectorOf<Vtx>(in,thisPartID,data.numParts);
      for (auto v : vertices) {
        mesh->vertices.push_back(v.pos);
        mesh->colors.push_back(v.rgb);
      }
      for (int i=0;i<mesh->vertices.size()/3;i++)
        mesh->indices.push_back(3*i+vec3i(0,1,2));
      
      mini::Matte::SP mat = std::make_shared<mini::Matte>();
      mesh->material = mat;//mini::Matte::create();
      dataGroup.triangleMeshes.push_back(mesh);
    }
    
    std::string RGBTris::toString() 
    {
      return "RGBTris{fileName="+data.where+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    



    /*! simple position/normal/color/index triangle meshes in binary format */
    HSMesh::HSMesh(const ResourceSpecifier &data,
                     int thisPartID)
    : data(data),
      fileSize(getFileSize(data.where)),
      thisPartID(thisPartID)
    {}
    
    void HSMesh::create(DataLoader *loader,
                                const ResourceSpecifier &dataURL)
    {
      for (int i=0;i<dataURL.numParts;i++)
        loader->addContent(new HSMesh(dataURL,i));
    }
    
    size_t HSMesh::projectedSize()
    { return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts); }
    
    void   HSMesh::executeLoad(DataRank &dataGroup, bool verbose)
    {
      TriangleMesh::SP mesh = std::make_shared<TriangleMesh>(data.where);
      mini::Matte::SP mat = std::make_shared<mini::Matte>();
      mesh->material = mat;
      if (data.numParts > 1)
        throw std::runtime_error("cannot split meshes yet");
      dataGroup.triangleMeshes.push_back(mesh);
    }
    
    std::string HSMesh::toString() 
    {
      return "HSMesh{fileName="+data.where+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    
  }
}
