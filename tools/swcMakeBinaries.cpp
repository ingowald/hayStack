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
#include <fstream>

using namespace mini;

struct Node {
  vec3f   pos;
  float   radius;
  int64_t connectsTo;
  int     label;
};

struct FatCapsule {
  struct {
    vec3f   pos;
    float   radius;
    vec3f   color;
  } vertex[2];
};

std::vector<Node> nodes;

std::map<int,size_t> swcPointToNodeID;

void importSWC(const std::string &fileName)
{
  std::cout << "# importing " << fileName << "... " << std::flush;
  std::ifstream in(fileName.c_str());
  std::string line;
  size_t whereWeStartedToAddNodes = nodes.size();
  while (std::getline(in,line)) {
    if (line[0] == '#') continue;
    Node node;
    int nodeID, connectsTo;
    float x,y,z,r;
    // 1 1 242182.4 308667.53 122759.62 1876 -1
    if (sscanf(line.c_str(),"%i %i %f %f %f %f %i",
               &nodeID,
               &node.label,
               &node.pos.x,
               &node.pos.y,
               &node.pos.z,
               &node.radius,
               &connectsTo) != 7)
      continue;
    node.connectsTo = connectsTo;
    swcPointToNodeID[nodeID] = nodes.size();
    nodes.push_back(node);
  }

  for (size_t i=whereWeStartedToAddNodes;i<nodes.size();i++) {
    auto &node = nodes[i];
    if (node.connectsTo == -1)
      continue;
    if (swcPointToNodeID.find(node.connectsTo) == swcPointToNodeID.end())
      std::cout << "Warning (in " << fileName << "): node #"
                << std::to_string(i-whereWeStartedToAddNodes+1)
                << " connects to unknown/undefined other node #"
                << std::to_string(node.connectsTo) << " !?" << std::endl;
    node.connectsTo = swcPointToNodeID[node.connectsTo];
  }
  // std::cout << "now have " << prettyNumber(fatCapsules.size()) << " capsules" << std::endl;
  std::cout << "now have " << prettyNumber(nodes.size()) << " nodes" << std::endl;
}

int main(int ac, char **av)
{
  std::vector<std::string> inFileNames;
  std::string inFilesFileName;
  std::string outFileName;
  bool storeAsCapsules = false;
  bool colorByFile = false;

  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg == "-i")
      inFilesFileName = av[++i];
    else if (arg == "-o")
      outFileName = av[++i];
    else if (arg == "--color-by-file")
      colorByFile = true;
    else if (arg == "-c" || arg == "-fc" || arg == "--fat-capsules" || arg == "--capsules")
      storeAsCapsules = true;
    else if (arg[0] != '-')
      inFileNames.push_back(arg);
    else
      throw std::runtime_error
        ("usage: ./swcMakeBinaries [-o out.fcbin]"
         " [-i fileWithFileNames] file.swc*");
  }

  if (!inFilesFileName.empty()) {
    std::ifstream file(inFilesFileName.c_str());
    std::string line;
    while (std::getline(file,line))
      inFileNames.push_back(line);
      // inFileNames.push_back(line.substr(0,line.find("\n")));
  }
  
  for (auto fileName : inFileNames)
    importSWC(fileName);

  if (!outFileName.empty()) {
    std::ofstream out(outFileName.c_str(),std::ios::binary);
    if (storeAsCapsules) {
      static int fileID = 123;
      vec3f fileColor = randomColor(fileID++);
      for (auto node : nodes) {
        if (node.connectsTo < 0)
          continue;
        FatCapsule fc;
        fc.vertex[0].pos    = node.pos;
        fc.vertex[0].radius = node.radius;
        fc.vertex[0].color  = colorByFile?fileColor:randomColor(node.label);

        auto &other = nodes[node.connectsTo];
        fc.vertex[1].pos    = other.pos;
        fc.vertex[1].radius = other.radius;
        fc.vertex[1].color  = colorByFile?fileColor:randomColor(other.label);

        out.write((const char *)&fc,sizeof(fc));
      }
    } else {
      out.write((const char *)nodes.data(),nodes.size()*sizeof(nodes[0]));
    }
  }
    
}
