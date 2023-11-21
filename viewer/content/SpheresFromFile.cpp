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
  
  SpheresFromFile::SpheresFromFile(const ResourceSpecifier &data,
                                   int thisPartID,
                                   float defaultRadius)
    : data(data),
      fileSize(getFileSize(data.where)),
      thisPartID(thisPartID),
      radius(data.get_float("radius",defaultRadius))
  {}

  std::string SpheresFromFile::toString() 
  {
    return "Spheres{fileName="+data.where
      +", part "+std::to_string(thisPartID)+" of "
      + std::to_string(data.numParts)+", proj size "
      +prettyNumber(projectedSize())+"B}";
    }

  
  void SpheresFromFile::create(DataLoader *loader,
                               const ResourceSpecifier &dataURL)
  {
    for (int i=0;i<dataURL.numParts;i++)
      loader->addContent(new SpheresFromFile(dataURL,i,
                                             loader->defaultRadius));
  }
    
  size_t SpheresFromFile::projectedSize() 
  { return (100/12) * divRoundUp(fileSize, (size_t)data.numParts); }
    
  void   SpheresFromFile::executeLoad(DataGroup &dataGroup, bool verbose) 
  {
    SphereSet::SP spheres = SphereSet::create();
    spheres->radius = radius;
    FILE *file = fopen(data.where.c_str(),"rb");
    assert(file);
    size_t sizeOfSphere
      = (data.get("format")=="xyzf")
      ? sizeof(vec4f)
      : sizeof(vec3f);
    int64_t numSpheresInFile = fileSize / sizeOfSphere;
    int64_t numSpheresToLoad
      = std::min((int64_t)data.get_size("count",numSpheresInFile),
                 (int64_t)(numSpheresInFile-data.get_size("begin",0)));
    if (numSpheresToLoad <= 0)
      throw std::runtime_error("no spheres to load for these begin/count values!?");
  
    size_t my_begin
      = data.get_size("begin",0)
      + (numSpheresToLoad * (thisPartID+0)) / data.numParts;
    size_t my_end
      = data.get_size("begin",0)
      + (numSpheresToLoad * (thisPartID+1)) / data.numParts;
    size_t my_count = my_end - my_begin;
    // spheres->origins.resize(my_count);
    fseek(file,my_begin*sizeOfSphere,SEEK_SET);
    const std::string format = data.get("format","xyz");
    if (format =="xyzf") {
      vec4f v;
      for (size_t i=0;i<my_count;i++) {
        int rc = fread((char*)&v,sizeof(v),1,file);
        assert(rc);
        spheres->origins.push_back(vec3f{v.x,v.y,v.z});
      }
    } else if (format == "xyz") {
      vec3f v;
      for (size_t i=0;i<my_count;i++) {
        int rc = fread((char*)&v,sizeof(v),1,file);
        assert(rc);
        spheres->origins.push_back(vec3f{v.x,v.y,v.z});
      }
    } else
      throw std::runtime_error("un-recognized spheres format '"+format+"'");

    if (verbose) {
      std::cout << "   ... done loading " << prettyNumber(my_count)
                << " spheres from " << data.where << std::endl << std::flush;
      fflush(0);
    }
    fclose(file);

    dataGroup.sphereSets.push_back(spheres);
  }
  


}
