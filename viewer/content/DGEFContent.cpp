// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "viewer/content/DGEFContent.h"
#include "DGEF.h"

namespace mini {
  
  Scene::SP loadDGEF(const std::string &objFile)
  {
    dgef::Model::SP model
      = dgef::Model::read(objFile);
    std::vector<mini::Instance::SP> instances;
    for (auto in : model->meshes) {
      mini::Mesh::SP out = mini::Mesh::create();
      for (auto v : in->vertices)
        out->vertices.push_back(vec3f(v.x,v.y,v.z));
      for (auto v : in->indices)
        out->indices.push_back(vec3i(v.x,v.y,v.z));
      mini::Object::SP obj = mini::Object::create({out});
      instances.push_back(mini::Instance::create(obj));
    }
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
