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

#include "viewer/content/DGEFContent.h"
#include "DGEF.h"

namespace mini {
  
  Scene::SP loadDGEF(const std::string &objFile)
  {
    dgef::Model::SP model
      = dgef::Model::read(objFile);
    std::vector<mini::Instance::SP> instances;
    // std::vector<mini::Mesh::SP> meshes;
    for (auto in : model->meshes) {
      mini::Mesh::SP out = mini::Mesh::create();
      for (auto v : in->vertices)
        out->vertices.push_back(vec3f(v.x,v.y,v.z));
      for (auto v : in->indices)
        out->indices.push_back(vec3i(v.x,v.y,v.z));
      mini::Object::SP obj = mini::Object::create({out});
      instances.push_back(mini::Instance::create(obj));
      // meshes.push_back(out);
    }
    // mini::Object::SP obj = mini::Object::create(meshes);
    Scene::SP scene = mini::Scene::create(instances);
    return scene;    
  }
  
} // ::mini

namespace hs {
  
  void DGEFContent::executeLoad(DataRank &dataGroup, bool verbose) 
  {
    dataGroup.minis.push_back(mini::loadDGEF(fileName));
  }
  
}
