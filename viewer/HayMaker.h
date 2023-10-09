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

#pragma once

#include "haystack/HayStack.h"
#include "barney/MPIWrappers.h"
#include "viewer/DataLoader.h"
#include "viewer/MPIRenderer.h"
#include "barney.h"

namespace hs {

  struct HayMaker : public Renderer
  {
    HayMaker(Comm &world,
             bool isActiveWorker,
             bool verbose);
    
    void createBarney();
    
    void loadData(DynamicDataLoader &loader,
                  int numDataGroups,
                  int dataPerRank);
    
    void resize(const vec2i &fbSize, uint32_t *hostRgba) override;

    void renderFrame() override;
    box3f getWorldBounds() const;
    
    Comm        &world;
    const bool   isActiveWorker;
    Comm         workers;
    ThisRankData rankData;
    bool         verbose;

    BNContext barney = 0;
    BNModel   model = 0;
    BNFrameBuffer fb = 0;
  };

}
