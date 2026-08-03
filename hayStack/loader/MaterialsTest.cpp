// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/MaterialsTest.h"

namespace hs {
  namespace loader {
    
    MaterialsTest::MaterialsTest(const ResourceSpecifier &data,
                                 int thisPartID)
      : data(data),
        thisPartID(thisPartID)
    {}
  
    std::string MaterialsTest::toString() 
    {
      return "MaterialTest{fileName="+data.where
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

    void   MaterialsTest::executeLoad(OnePartition &dataGroup) 
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
          material->baseColor = vec3f(.9f,.1f,.1f);
          // material->baseColor = vec3f(.2f,.8f,.2f);
          material->roughness = fx;
          material->metallic  = fy;
          spheres->material = material;
          dataGroup.sphereSets.push_back(spheres);
        }
    }
  
  }
}
