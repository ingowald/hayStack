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
#include "viewer/content/CylindersFromFile.h"
#include <fstream>

namespace hs {

    struct FileEntry {
      int ID;
      int type;
      vec3f pos;
      float rad;
      int connect;
    };
    struct Vertex {
      vec3f pos, col;
      float rad;
    };
  inline bool operator<(const Vertex &a, const Vertex &b)
  {
    return memcmp(&a,&b,sizeof(a)) < 0;
  }
  void loadSWC(hs::Cylinders::SP result, const std::string &fileName)
  {
    std::vector<FileEntry> fileEntries;
    std::map<int,int>      indexByID;
      
    FILE *file = fopen(fileName.c_str(),"rb");
    if (!file)
      throw std::runtime_error("#hs.swc: could not open "+fileName);
      
    enum { LINE_SZ = 10000 };
    char line[LINE_SZ+1];

    while (fgets(line,LINE_SZ,file) && !feof(file)) {
      if (line[0] == '#')
        continue;
      FileEntry fe;
      int n = sscanf(line,"%i %i %f %f %f %f %i",
                     &fe.ID,
                     &fe.type,
                     &fe.pos.x,
                     &fe.pos.y,
                     &fe.pos.z,
                     &fe.rad,
                     &fe.connect);
      if (n != 7) {
        fclose(file);
        throw std::runtime_error("#hs.swc: could not parse line '"+std::string(line)+"'");
      }
      indexByID[fe.ID] = fileEntries.size();
      fileEntries.push_back(fe);
    }
    std::cout << "#hs.swc: done reading SWC file, found "
              << prettyNumber(fileEntries.size()) << " nodes" << std::endl;
    std::cout << "last line was " << line << std::endl;

    std::map<Vertex,int> vertices;
    for (int i=0;i<fileEntries.size();i++) {
      FileEntry head = fileEntries[i];
      if (head.connect < 0)
        continue;
      if (indexByID.find(head.connect) == indexByID.end())
        throw std::runtime_error("could not find connection...");
      if (indexByID[head.connect] < 0 || indexByID[head.connect] >=fileEntries.size())
        throw std::runtime_error("invalid connection");
      
      FileEntry tail = fileEntries[indexByID[head.connect]];

      auto getOrAddVertex = [&](FileEntry fe) -> int {
        Vertex v;
        v.pos = fe.pos;
        v.rad = fe.rad;
        v.col = owl::common::randomColor(fe.type);
        auto it = vertices.find(v);
        if (it != vertices.end())
          return it->second;
        int newID = vertices.size();
        vertices[v] = newID;
        return newID;
      };
      vec2i segment;
      segment.x = getOrAddVertex(head);
      segment.y = getOrAddVertex(tail);
      result->indices.push_back(segment);
    }

    result->colors.resize(vertices.size());
    result->colorPerVertex = true;
    result->vertices.resize(vertices.size());
    result->radii.resize(vertices.size());
    result->radiusPerVertex = true;
    for (auto it : vertices) {
      result->vertices[it.second] = it.first.pos;
      // result->vertices[it.second] = ((it.first.pos * scale) + shift);
      result->radii[it.second] = it.first.rad;

      result->colors[it.second] = it.first.col;
    }
    fclose(file);
  }
    
  CylindersFromFile::CylindersFromFile(const std::string &fileName,
                                       size_t fileSize,
                                       int thisPartID,
                                       int numPartsToSplitInto,
                                       float radius,
                                       vec3f shift,
                                       vec3f scale)
    : fileName(fileName),
      fileSize(fileSize),
      thisPartID(thisPartID),
      numPartsToSplitInto(numPartsToSplitInto),
      radius(radius),
      shift(shift),
      scale(scale)
  {}
  
  void CylindersFromFile::create(DataLoader *loader,
                                 const ResourceSpecifier &data)
  {
    vec3f scale = data.get("scale",vec3f(1.f));
    vec3f translate = data.get("translate",vec3f(1.f));
    
    const std::string fileName = data.where;
    const size_t fileSize = data.where == "sample" ? 1ull : getFileSize(data.where);
    for (int i=0;i<data.numParts;i++)
      loader->addContent(new CylindersFromFile(data.where,
                                               fileSize,i,
                                               data.numParts,
                                               loader->defaultRadius,
                                               translate,scale));
  }
  
  size_t CylindersFromFile::projectedSize() 
  { return 100; }
    
  void   CylindersFromFile::executeLoad(DataRank &dataGroup, bool verbose) 
  {
    hs::Cylinders::SP cs = hs::Cylinders::create();
    if (fileName == "sample") {
      struct Cylinder { float ax,ay,az,r,bx,by,bz; };
      /*! stolen from shadertoy example, https://www.shadertoy.com/view/Md3cWj */
      Cylinder cylinders[12]
        = {
        {      0,     -1.3,     0,    10,      0,          -2,     0 },
        {      0,       -1,     0,   3.5,      0,          -2,     0 },
        { -.7071,-1.+.7071,     0,     1,  .7071,-1.+3.*.7071,     0 },
        {      0,       -1,     3,    .2,      0,           4,     3 },
         
        {   2.12,       -1,  2.12,   .2f,   2.12,           4,  2.12 },
        {      3,       -1,     0,   .2f,      3,           4,     0 },
        {   2.12,       -1, -2.12,   .2f,   2.12,           4, -2.12 },
        {      0,       -1,    -3,   .2f,      0,           4,    -3 },
         
        { -2.12f,     -1.f,-2.12f,   .2f, -2.12f,         4.f,-2.12f },
        {   -3.f,     -1.f,   0.f,   .2f,   -3.f,         4.f,   0.f },
        { -2.12f,     -1.f, 2.12f,   .2f, -2.12f,         4.f, 2.12f },
        {      0,        4,     0,   3.5,      0,           5,     0 }
      };
    
      for (int i=0;i<12;i++) {
        vec3f a = vec3f(cylinders[i].ax,cylinders[i].ay,cylinders[i].az);
        vec3f b = vec3f(cylinders[i].bx,cylinders[i].by,cylinders[i].bz);
        float r = cylinders[i].r;
        cs->vertices.push_back(a);
        cs->vertices.push_back(b);
        cs->radii.push_back(r);
        static int primID = 1231234;
        cs->colors.push_back(randomColor(++primID));
      }
      cs->radiusPerVertex = false;
    } else if (endsWith(fileName,".raw")) {
      // serkan dumped files
      std::ifstream in(fileName.c_str(),std::ios::binary);
      size_t numTransforms;
      in.read((char*)&numTransforms,sizeof(numTransforms));
      for (int i=0;i<std::min(1000000,(int)numTransforms);i++) {
        affine3f xfm;
        in.read((char*)&xfm,sizeof(xfm));
        float thick = .002f;//1000.f;//1e10f;

        // xfm = rcp(xfm);
        float scale = 1.f;///100000.f;
        thick *=scale;
        xfm.l.vx *= scale;
        xfm.l.vy *= scale;
        xfm.l.vz *= scale;
        // thick = std::min(thick,length(xfm.l.vx));
        // thick = std::min(thick,length(xfm.l.vy));
        // thick = std::min(thick,length(xfm.l.vz));
        // PRINT(thick);
        // thick *= .3f;
        int idx = cs->vertices.size();
        cs->vertices.push_back(xfm.p);
        cs->vertices.push_back(xfm.p+xfm.l.vx);
        cs->vertices.push_back(xfm.p+xfm.l.vy);
        cs->vertices.push_back(xfm.p+xfm.l.vz);
        cs->indices.push_back(vec2i(idx,idx+1));
        cs->indices.push_back(vec2i(idx,idx+2));
        cs->indices.push_back(vec2i(idx,idx+3));
        cs->radii.push_back(thick);
        cs->radii.push_back(thick);
        cs->radii.push_back(thick);
        cs->colors.push_back(randomColor(idx));
        cs->colors.push_back(randomColor(idx));
        cs->colors.push_back(randomColor(idx));
      }
      cs->radiusPerVertex = false;
    } else {
      loadSWC(cs,fileName);
      for (auto &vtx : cs->vertices)
        vtx = vtx * scale + shift;
    }
    dataGroup.cylinderSets.push_back(cs);
  }
  


}
