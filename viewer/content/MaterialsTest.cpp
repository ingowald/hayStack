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
#include "viewer/content/MaterialsTest.h"

namespace hs {
  
  MaterialsTest::MaterialsTest(const ResourceSpecifier &data,
                               int thisPartID)
    : data(data),
      thisPartID(thisPartID)
  {}
  
  std::string MaterialsTest::toString() 
  {
    return "Spheres{fileName="+data.where
      +", part "+std::to_string(thisPartID)+" of "
      + std::to_string(data.numParts)+", proj size "
      +prettyNumber(projectedSize())+"B}";
    }

  void MaterialsTest::create(DataLoader *loader,
                               const ResourceSpecifier &dataURL)
  {
    for (int i=0;i<gridRes*gridRes;i++)
      loader->addContent(new MaterialsTest(dataURL,i));
  }
    
  size_t MaterialsTest::projectedSize() 
  { return 100 * (size_t)data.numParts; }

  void   MaterialsTest::executeLoad(DataRank &dataGroup, bool verbose) 
  {
    for (int iy=0;iy<gridRes;iy++)
      for (int ix=0;ix<gridRes;ix++) {
        int sphereID = ix + gridRes * iy;
        if (sphereID%data.numParts != thisPartID)
          continue;
        float fx = ix / (gridRes-1.f);
        float fy = iy / (gridRes-1.f);
        
        SphereSet::SP spheres = SphereSet::create();
        spheres->radius = .4f;
        
        vec3f v(ix,iy,0.f);
        spheres->origins.push_back(v);
        mini::DisneyMaterial::SP material = mini::DisneyMaterial::create();
        material->baseColor = vec3f(.2f,.8f,.2f);
        material->roughness = fx;
        material->metallic  = fy;
        spheres->material = material;
        dataGroup.sphereSets.push_back(spheres);
      }
  }
  


}
