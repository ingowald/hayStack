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
#include "viewer/content/SpheresFromFile.h"

namespace hs {
  
  SpheresFromFile::SpheresFromFile(const std::string &fileName,
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
    
  void SpheresFromFile::create(DataLoader *loader,
                               const std::string &dataURL)
  {
    assert(dataURL.substr(0,strlen("spheres://")) == "spheres://");
    const char *s = dataURL.c_str() + strlen("spheres://");
    const char *atSign = strstr(s,"@");
    int numPartsToSplitInto = 1;
    if (atSign) {
      numPartsToSplitInto = atoi(s);
      s = atSign + 1;
    }
    const std::string fileName = s;
    size_t fileSize = getFileSize(fileName);
    for (int i=0;i<numPartsToSplitInto;i++)
      loader->addContent(new SpheresFromFile(fileName,fileSize,i,numPartsToSplitInto,
                                             loader->defaultRadius));
  }
    
  size_t SpheresFromFile::projectedSize() 
  { return (100/12) * divRoundUp(fileSize, (size_t)numPartsToSplitInto); }
    
  void   SpheresFromFile::executeLoad(DataGroup &dataGroup, bool verbose) 
  {
    SphereSet::SP spheres = SphereSet::create();
    spheres->radius = radius;
    FILE *file = fopen(fileName.c_str(),"rb");
    assert(file);
    size_t sizeOfSphere = sizeof(spheres->origins[0]);
    size_t numSpheresTotal = fileSize / sizeOfSphere;
    size_t my_begin = (numSpheresTotal * (thisPartID+0)) / numPartsToSplitInto;
    size_t my_end = (numSpheresTotal * (thisPartID+1)) / numPartsToSplitInto;
    size_t my_count = my_end - my_begin;
    spheres->origins.resize(my_count);
    fseek(file,my_begin*sizeOfSphere,SEEK_SET);
    size_t numRead = fread(spheres->origins.data(),sizeOfSphere,my_count,file);
    assert(numRead == my_count);
    if (verbose) {
      std::cout << "   ... done loading " << prettyNumber(my_count)
                << " spheres from " << fileName << std::endl << std::flush;
      fflush(0);
    }
    fclose(file);

    dataGroup.sphereSets.push_back(spheres);
  }
  


}
