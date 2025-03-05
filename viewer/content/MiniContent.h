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
                       const std::string &dataURL)
    {
      loader->addContent(new MiniContent(/* this is a plain filename for umesh:*/dataURL));
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
#if 0
      // mini has evnmap lights upside down (apparently). fix this.
      if (ms->envMapLight) {
        Texture::SP tex = ms->envMapLight->texture;
        if (tex && tex->format == mini::Texture::FLOAT4) {
          std::cout << "#hs: flipping env-map texture" << std::endl;
          vec4f *texels = (vec4f*)tex->data.data();
          for (int iy=0;iy<tex->size.y/2;iy++)
            for (int ix=0;ix<tex->size.x;ix++) {
              vec4f *lo = texels+tex->size.x*iy;
              vec4f *hi = texels+tex->size.x*(tex->size.y-1-iy);
              std::swap(lo[ix],hi[ix]);
            }
        }
      }
#endif
      
      if (ms->envMapLight) {
        // this is a hack for when splitting a model into multiple
        // aprts, and each model having a copy of the light
        // sources. in theory this SHOULD be fixed in the splitter,
        // but for now do it here.
        for (auto other : dataGroup.minis)
          if (other->envMapLight) {
            ms->envMapLight = {};
            ms->quadLights.clear();
            ms->dirLights.clear();
          }
      }
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
