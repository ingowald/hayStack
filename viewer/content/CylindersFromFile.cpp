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

namespace hs {
  
  CylindersFromFile::CylindersFromFile(const std::string &fileName,
                                       size_t fileSize,
                                       int thisPartID,
                                       int numPartsToSplitInto,
                                       float radius)
    : fileName(fileName),
      fileSize(fileSize),
      thisPartID(thisPartID),
      numPartsToSplitInto(numPartsToSplitInto),
      radius(radius)
  {}
  
  void CylindersFromFile::create(DataLoader *loader,
                                 const std::string &dataURL)
  {
    assert(dataURL.substr(0,strlen("cylinders://")) == "cylinders://");
    const char *s = dataURL.c_str() + strlen("cylinders://");
    const char *atSign = strstr(s,"@");
    int numPartsToSplitInto = 1;
    if (atSign) {
      numPartsToSplitInto = atoi(s);
      s = atSign + 1;
    }
    const std::string fileName = s;
    size_t fileSize = getFileSize(fileName);
    for (int i=0;i<numPartsToSplitInto;i++)
      loader->addContent(new CylindersFromFile(fileName,fileSize,i,numPartsToSplitInto,
                                               loader->defaultRadius));
  }
  
  size_t CylindersFromFile::projectedSize() 
  { return 100; }
    
  void   CylindersFromFile::executeLoad(DataGroup &dataGroup, bool verbose) 
  {
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
    
    hs::Cylinders::SP cs = hs::Cylinders::create();
    for (int i=0;i<12;i++) {
      vec3f a = vec3f(cylinders[i].ax,cylinders[i].ay,cylinders[i].az);
      vec3f b = vec3f(cylinders[i].bx,cylinders[i].by,cylinders[i].bz);
      float r = cylinders[i].r;
      cs->points.push_back(a);
      cs->points.push_back(b);
      cs->radii.push_back(r);
    }
    dataGroup.cylinderSets.push_back(cs);
  }
  


}
