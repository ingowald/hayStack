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

#include "HayMaker.h"

namespace hs {

  HayMaker::HayMaker(Comm &world,
                     bool isActiveWorker,
                     bool verbose)
    : world(world),
      workers(world.split(isActiveWorker)),
      isActiveWorker(isActiveWorker),
      verbose(verbose)
  {}

  void HayMaker::createBarney()
  {
    if (isActiveWorker) {
      std::vector<int> dataGroupIDs;
      for (auto dg : rankData.dataGroups)
        dataGroupIDs.push_back(dg.dataGroupID);
      barney = bnMPIContextCreate(world.comm,/*data*/dataGroupIDs.data(),dataGroupIDs.size(),
                               /*gpus*/nullptr,-1);
    } else
      barney = bnMPIContextCreate(world.comm,/*data*/nullptr,0,/*gpus*/nullptr,0);

    fb = bnFrameBufferCreate(barney,0);
    model = bnModelCreate(barney);
  }
  
  void HayMaker::loadData(DynamicDataLoader &loader,
                          int numDataGroups,
                          int dataPerRank)
  {
    if (!isActiveWorker)
      return;
    if (dataPerRank == 0) {
      if (workers.size < numDataGroups) {
        dataPerRank = numDataGroups / workers.size;
      } else {
        dataPerRank = 1;
      }
    }

    if (numDataGroups % dataPerRank) {
      std::cout << "warning - num data groups is not a "
                << "multiple of data groups per rank?!" << std::endl;
      std::cout << "increasing num data groups to " << numDataGroups
                << " to ensure equal num data groups for each rank" << std::endl;
    }
  
    loader.assignGroups(numDataGroups);
    rankData.resize(dataPerRank);
    for (int i=0;i<dataPerRank;i++) {
      int dataGroupID = (workers.rank*dataPerRank+i) % numDataGroups;
      if (verbose) {
        for (int r=0;r<workers.rank;r++) 
          workers.barrier();
        std::cout << "#hv: worker #" << workers.rank
                  << " loading data group " << dataGroupID << " into slot " << workers.rank << "." << i << ":"
                  << std::endl << std::flush;
        usleep(100);
        fflush(0);
      }
      loader.loadDataGroup(rankData.dataGroups[i],
                           dataGroupID,
                           verbose);
      if (verbose) 
        for (int r=workers.rank;r<workers.size;r++) 
          workers.barrier();
    }
    if (verbose) {
      workers.barrier();
      if (workers.rank == 0)
        std::cout << "#hv: all workers done loading their data..." << std::endl;
      workers.barrier();
    }
  }
  
  void HayMaker::resize(const vec2i &fbSize, uint32_t *hostRGBA)
  {
    assert(fb);
    bnFrameBufferResize(fb,fbSize.x,fbSize.y,(world.rank==0)?hostRGBA:nullptr);
  }
  
  void HayMaker::renderFrame()
  {
    assert(bn);
    BNCamera camera;
    bnRender(model,&camera,fb,nullptr);
  }
}
