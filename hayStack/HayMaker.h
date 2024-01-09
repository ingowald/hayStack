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

/*! the hay-*maker* is the actual render for hay-stack type data,
    using barney */

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/DataGroup.h"
#include "hayStack/MPIRenderer.h"
#include "barney.h"

namespace hs {

  struct HayMaker : public Renderer
  {
    HayMaker(Comm &world,
             Comm &workers,
             ThisRankData &thisRankData,
             bool verbose);
    
    void createBarney();
    
    void buildDataGroup(int dgID);
    
    void resize(const vec2i &fbSize, uint32_t *hostRgba) override;

    void renderFrame() override;
    BoundsData getWorldBounds() const;
    void resetAccumulation() override;
    void setTransferFunction(const TransferFunction &xf) override;

    void setCamera(const Camera &camera);


    BNCamera     camera;
    /*! need this for the camera aspect ratio */
    vec2i        fbSize;
    Comm        &world;
    const bool   isActiveWorker;
    Comm         workers;
    ThisRankData rankData;
    bool         verbose;

    struct PerDG {
      std::vector<BNVolume> createdVolumes;
      BNGroup volumeGroup = 0;
    };
    std::vector<PerDG> perDG;
    
    BNContext barney = 0;
    BNModel   model = 0;
    BNFrameBuffer fb = 0;
  };

}
