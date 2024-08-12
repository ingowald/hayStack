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

float getFloat(int &i, int ac, char **av)
{
  if (i >= ac) throw std::runtime_error("cannot find cmdline argument");
  float f = std::stof(av[i++]);
  return f;
}

vec3f getFloat3(int &i, int ac, char **av)
{
  float x = getFloat(i,ac,av);
  float y = getFloat(i,ac,av);
  float z = getFloat(i,ac,av);
  return {x,y,z};
}

int main(int ac, char **av)
{
  std::string inFileName;
  std::string outFileName;

  mini::BlenderMaterial::SP mat = mini::BlenderMaterial::create();
  
  for (int i=1;i<ac;i++) {
    std::string arg = av[i];
    if (arg[0] != '-')
      inFileName = arg;
    else if (arg == "-o")
      outFileName = av[++i];
    else if (arg == "-rough" || arg == "--roughness")
      mat->roughness = getFloat(i,ac,av);
    else if (arg == "-base" || arg == "--basecolor")
      mat->baseColor = getFloat3(i,ac,av);
    else
      throw std::runtime_error("./miniSplitObjectSpace inFile.mini <args>\n"
                               "--roughness <float>\n"
                               "--basecolor <float3>\n"
                               );
  }
  
  Scene::SP in = Scene::load(inFileName);
  for (auto inst : in->instances)
    for (auto mesh : inst->object->meshes)
      mesh->material = mat;
  
  in->save(outFileName);
  return 0;
}
