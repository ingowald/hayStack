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

#include "viewer/DataLoader.h"
#include "viewer/content/Capsules.h"
#include <fstream>

namespace hs {
  namespace content {
    
    struct FatCapsule {
      struct {
        vec3f position;
        float radius;
        vec3f color;
      } vertex[2];
    };
    inline bool operator<(const FatCapsule &a, const FatCapsule &b)
    {
      return memcmp(&a,&b,sizeof(a)) < 0;
    }
  
    Capsules::Capsules(const std::string &fileName,
                       size_t fileSize,
                       int thisPartID,
                       int numPartsToSplitInto)
      : fileName(fileName),
        fileSize(fileSize),
        thisPartID(thisPartID),
        numPartsToSplitInto(numPartsToSplitInto)
    {}
  
    void Capsules::create(DataLoader *loader,
                          const ResourceSpecifier &data)
    {
      const std::string fileName = data.where;
      const size_t fileSize = getFileSize(data.where);
      for (int i=0;i<data.numParts;i++)
        loader->addContent(new Capsules(data.where,
                                        fileSize,i,
                                        data.numParts));
    }
  
    size_t Capsules::projectedSize() 
    { return fileSize; }
  
    void   Capsules::executeLoad(DataRank &dataGroup, bool verbose) 
    {
      hs::Capsules::SP cs = hs::Capsules::create();
      cs->material = mini::DisneyMaterial::create();
      
      size_t numCapsulesTotal = fileSize / sizeof(FatCapsule);
      size_t begin = (thisPartID * numCapsulesTotal) / numPartsToSplitInto;
      size_t end = ((thisPartID+1) * numCapsulesTotal) / numPartsToSplitInto;
      size_t count = end - begin;

      std::vector<FatCapsule> fatCapsules(count);

      std::ifstream in(fileName.c_str(),std::ios::binary);
      in.seekg(begin*sizeof(FatCapsule),in.beg);
      in.read((char *)fatCapsules.data(),count*sizeof(FatCapsule));

      std::map<std::pair<vec4f,vec3f>,int> knownVertices;
      bool hadNanColors = false;
      for (auto fc : fatCapsules) {
        int segmentIndices[2];
        for (int i=0;i<2;i++) {
          const auto &fcv = fc.vertex[i];
          vec4f vertex;
          (vec3f&)vertex = fcv.position;
          vertex.w       = fcv.radius;
          vec3f color    = fcv.color;
          if (isnan(color.x)) {
            color = vec3f(-1.f);
            hadNanColors = true;
          }
          std::pair<vec4f,vec3f> key = { vertex,color };
          if (knownVertices.find(key) == knownVertices.end()) {
            knownVertices[key] = cs->vertices.size();
            cs->vertices.push_back(vertex);
            cs->vertexColors.push_back(color);
          }
          segmentIndices[i] = knownVertices[key];
        }
        cs->indices.push_back(vec2i(segmentIndices[0],segmentIndices[1]));
      }
      if (hadNanColors)
        cs->vertexColors.clear();
      
      dataGroup.capsuleSets.push_back(cs);
    }
  
  }
}
