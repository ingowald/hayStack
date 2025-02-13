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

#include "viewer/DataLoader.h"

namespace hs {

  /*! a 'umesh' file of unstructured mesh data */
  struct MiniContent : public LoadableContent {
    MiniContent(const std::string &fileName)
      : fileName(fileName),
        fileSize(getFileSize(fileName))
    {}

    static void create(DataLoader *loader,
                       /*const std::string &dataURL*/const ResourceSpecifier &dataURL)
    {
      printf("MiniContent: dataURL.numParts: %d\n", dataURL.numParts);

      for (int i=0;i<dataURL.numParts;i++) {
        loader->addContent(new MiniContent(/* this is a plain filename for umesh:*/dataURL.where));
      }
    }
    
    std::string toString() override
    {
      return "Mini{fileName="+fileName+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    size_t projectedSize() override
    { return 2 * fileSize; }
    
    void   executeLoad(DataRank &dataGroup, bool verbose) override
    {
      mini::Scene::SP ms = mini::Scene::load(fileName);
      dataGroup.minis.push_back(ms);
      
#if 1
      if (getenv("HS_COLOR_MESHID")) {
        static int uniqueID = 0;
        std::map<Object::SP,int> objIDs;
        std::map<Mesh::SP,int> meshIDs;
        for (auto inst : ms->instances) {
          if (objIDs.find(inst->object) == objIDs.end()) {
            int ID = (uniqueID += inst->object->meshes.size());
            objIDs[inst->object] = ID;
            for (int i=0;i<inst->object->meshes.size();i++)
              meshIDs[inst->object->meshes[i]] = (ID + i);
          }
        }
        for (auto pair : meshIDs) {
          auto mesh = pair.first;
          // mini::DisneyMaterial::SP asDisney = mesh->material->as<mini::DisneyMaterial>();
          mini::DisneyMaterial::SP asDisney = mini::DisneyMaterial::create();
          if (asDisney)
            asDisney->baseColor = 0.7f*randomColor(meshIDs[mesh]);
          mesh->material = asDisney;
        }
      }
#endif
#if 1
      if (getenv("HS_COLOR_GRAY")) {
        static int uniqueID = 0;
        std::map<Object::SP,int> objIDs; 
        std::map<Mesh::SP,int> meshIDs;
        for (auto inst : ms->instances) {
          if (objIDs.find(inst->object) == objIDs.end()) {
            int ID = (uniqueID += inst->object->meshes.size());
            objIDs[inst->object] = ID;
            for (int i=0;i<inst->object->meshes.size();i++)
              meshIDs[inst->object->meshes[i]] = (ID + i);
          }
        }
        for (auto pair : meshIDs) {
          auto mesh = pair.first;
          mini::Matte::SP asMini = mini::Matte::create();
          if (asMini)
            asMini->reflectance = .7f;
          mesh->material = asMini;
        }
      }
#endif
    }

    const std::string fileName;
    const size_t      fileSize;
  };

}
