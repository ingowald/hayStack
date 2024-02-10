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

#include "miniScene/Scene.h"
#include <queue>

using namespace mini;

double computeCost(const Mesh::SP mesh)
{
  double sum = 1000;
  sum += mesh->vertices.size()*12;
  sum += mesh->normals.size()*12;
  sum += mesh->texcoords.size()*8;
  sum += mesh->indices.size()*100;
  return sum;
}

double computeCost(Object::SP object)
{
  double sum = 0.f;
  for (auto mesh : object->meshes)
    sum += computeCost(mesh);
  return sum;
}

double computeCost(const std::vector<Instance::SP> &instances)
{
  return
    100. * instances.size()
    +
    computeCost(instances[0]->object);
}

int main(int ac, char **av)
{
  std::string inFileName;
  std::string outFilePrefix;
  int numParts = 8;

  for (int i=1;i<ac;i++) {
    std::string arg = av[i];
    if (arg[0] != '-')
      inFileName = arg;
    else if (arg == "-o")
      outFilePrefix = av[++i];
    else if (arg == "-n")
      numParts = std::stoi(av[++i]);
    else throw std::runtime_error("./miniSplitObjectSpace inFile.mini -n <N> -o prefix");
  }
  
  Scene::SP in = Scene::load(inFileName);
  std::map<Object::SP,std::vector<Instance::SP>> objects;
  for (auto inst : in->instances)
    objects[inst->object].push_back(inst);

  std::priority_queue<std::pair<double,Object::SP>> objectsByCost;
  for (auto obj : objects)
    objectsByCost.push({computeCost(obj.second),obj.first});

  std::vector<std::vector<Object::SP>> rankObjects(numParts);
  std::priority_queue<std::pair<double,int>> ranksByCost;
  for (int i=0;i<numParts;i++) ranksByCost.push({0.,0});

  while (!objectsByCost.empty()) {
    auto biggestObject = objectsByCost.top();
    PRINT(biggestObject.first);
    objectsByCost.pop();
    
    auto leastLoadedRank = ranksByCost.top();
    PRINT(leastLoadedRank.first);
    ranksByCost.pop();
    rankObjects[leastLoadedRank.second].push_back(biggestObject.second);

    ranksByCost.push({leastLoadedRank.first-biggestObject.first,leastLoadedRank.second});
  }

  for (int i=0;i<numParts;i++) {
    Scene::SP out = Scene::create();
    out->quadLights = in->quadLights;
    out->dirLights = in->dirLights;
    out->envMapLight = in->envMapLight;

    for (auto obj : rankObjects[i])
      for (auto inst : objects[obj])
        out->instances.push_back(inst);
    out->save(outFilePrefix+std::to_string(i)+".mini");
  }
}
