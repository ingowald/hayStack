// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
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

#include "UMeshContent.h"
#include "umesh/extractSurfaceMesh.h"

namespace hs {


  UMeshContent::UMeshContent(const std::string &fileName)
    : fileName(fileName),
      fileSize(getFileSize(fileName))
  {}

  void UMeshContent::create(DataLoader *loader,
                            const std::string &dataURL)
  {
    loader->addContent(new UMeshContent(/* this is a plain filename for umesh:*/dataURL));
  }
    
  std::string UMeshContent::toString() 
  {
    return "UMesh{fileName="+fileName+", proj size "
      +prettyNumber(projectedSize())+"B}";
  }
  
  size_t UMeshContent::projectedSize() 
  { return 2 * fileSize; }
    
  void   UMeshContent::executeLoad(DataRank &dataRank, bool verbose) 
  {
    umesh::UMesh::SP mesh = umesh::UMesh::loadFrom(fileName);
#if 0
    std::cout << "KILLING ALL NON-TETS!" << std::endl;
    mesh->pyrs.clear();
    mesh->hexes.clear();
    mesh->wedges.clear();
#endif
    dataRank.unsts.push_back({mesh,box3f()});

    if (1 && (mesh->triangles.size() || mesh->quads.size())) {
      std::cout << "#hs: umesh seems to have surface triangles - extracting those." << std::endl;
      umesh::UMesh::SP extracted = umesh::extractSurfaceMesh(mesh);
      std::cout << "#hs: got " << extracted->toString() << std::endl;
      
      mini::Mesh::SP miniMesh = mini::Mesh::create();
      mini::DisneyMaterial::SP mat = mini::DisneyMaterial::create();
      mat->transmission = .7f;
      mat->ior = 1.f;
      miniMesh->material = mat;
      miniMesh->vertices.resize(extracted->vertices.size());
      std::copy(extracted->vertices.begin(),extracted->vertices.end(),
                (umesh::vec3f*)miniMesh->vertices.data());
      for (auto tri : extracted->triangles)
        miniMesh->indices.push_back(vec3i(tri.x,tri.y,tri.z));
      for (auto quad : extracted->quads) {
        miniMesh->indices.push_back(vec3i(quad.x,quad.y,quad.z));
        miniMesh->indices.push_back(vec3i(quad.x,quad.z,quad.w));
      }
      mini::Object::SP miniObject = mini::Object::create({miniMesh});
      mini::Scene::SP miniScene = mini::Scene::create({mini::Instance::create(miniObject)});
      dataRank.minis.push_back(miniScene);
    }
  }

  

  
  SpatiallyPartitionedUMeshContent
  ::SpatiallyPartitionedUMeshContent(const std::string umeshFileName,
                                     const box3f &domain)
    : fileName(umeshFileName),
      fileSize(getFileSize(umeshFileName)),
      domain(domain)
  {}
    
  void SpatiallyPartitionedUMeshContent::create(DataLoader *loader,
                                                const ResourceSpecifier &dataURL)
  {
    const std::string domainsFileName = dataURL.where+".domains";
    size_t domainsFileSize = getFileSize(domainsFileName.c_str());
    int numParts = (domainsFileSize - 2*sizeof(size_t)) / (sizeof(box3f)+sizeof(range1f));
    std::vector<box3f> domains(numParts);
    std::vector<range1f> valueRanges(numParts);
    std::ifstream in(domainsFileName.c_str(),std::ios::binary);
    size_t numDomains;
    in.read((char*)&numDomains,sizeof(numDomains));
    if (numParts != numDomains)
      throw std::runtime_error("fishy results from reading domains");
    std::cout << "#hs.spumesh: reading " << numParts << " domains" << std::endl;
    in.read((char*)domains.data(),domains.size()*sizeof(domains[0]));

    size_t numRanges;
    in.read((char*)&numRanges,sizeof(numRanges));
    if (numParts != numRanges)
      throw std::runtime_error("fishy results from reading value ranges");
    std::cout << "#hs.spumesh: reading " << numParts << " value ranges" << std::endl;
    in.read((char*)valueRanges.data(),valueRanges.size()*sizeof(valueRanges[0]));
    for (int i=0;i<numParts;i++) {
      char suffix[100];
      snprintf(suffix,sizeof(suffix),"_%05i",i);
      std::string partFileName = dataURL.where+suffix+".umesh";
      loader->addContent(new SpatiallyPartitionedUMeshContent(partFileName,domains[i]));

      // umesh::UMesh::SP mesh = umesh::UMesh::loadFrom(partFileName);
      // loader->dataRanks.unsts.push_back({mesh,domains[i]});
    }
  }
  // {
  //   loader->addContent(new SpatiallyParitionedUMeshContent(dataURL));
  // }
    
  std::string SpatiallyPartitionedUMeshContent::toString() 
  {
    return "SpatiallyParitionedUMesh{}";
  }
  size_t SpatiallyPartitionedUMeshContent::projectedSize() 
  { return 2 * fileSize; }
    
  void SpatiallyPartitionedUMeshContent::executeLoad(DataRank &dataRank, bool verbose) 
  {
    dataRank.unsts.push_back({umesh::UMesh::loadFrom(fileName),domain});
  }

  
}
