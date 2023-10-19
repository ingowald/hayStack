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

#include "HayStack.h"

namespace hs {

  box3f ThisRankData::getBounds() const
  {
    box3f bounds;
    for (auto &dg : dataGroups) bounds.extend(dg.getBounds());
    return bounds;
  }

  box3f DataGroup::getBounds() const
  {
    box3f bounds;
    for (auto mini : minis)
      if (mini)
        bounds.extend(mini->getBounds());
    for (auto unst : unsts)
      if (unst) {
        umesh::box3f bb = unst->getBounds();
        bounds.extend((const box3f&)bb);
      }
    for (auto sphereSet : sphereSets)
      if (sphereSet)
        bounds.extend(sphereSet->getBounds());
    return bounds;
  }

  box3f SphereSet::getBounds() const
  {
    box3f bounds;
    for (int i=0;i<origins.size();i++) {
      float r = (i<radii.size())?radii[i]:radius;
      vec3f o = origins[i];
      bounds.extend({o-r,o+r});
    }
    return bounds;
  }
  
  
}
