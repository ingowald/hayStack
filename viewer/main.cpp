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

#include "haystack/HayStack.h"
#include "viewer/DataLoader.h"
#include "barney/MPIWrappers.h"
// #include "mpiInf/Comms.h"

namespace hs {

  struct FromCL {
    /*! data groups per rank. '0' means 'auto - use few as we can, as
        many as we have to fit for given number of ranks */
    int dpr = 0;
    /*! num data groups */
    int ndg = 1;

    std::string outFileName = "";
    
    bool createHeadNode = false;
    int  numExtraDisplayRanks = 0;
  };
  
  void usage(const std::string &error="")
  {
    std::cout << "./haystack ... <args>" << std::endl;
    if (!error.empty())
      throw std::runtime_error("fatal error: " +error);
    exit(0);
  }

  // void loadDataGroup(DataGroup &dataGroup,
  //                    BarnConfig &config,
  //                    FromCL &fromCL,
  //                    int dataGroupID,
  //                    int numDataGroups)
  // {
  //   if (fromCL.dpr < 1)
  //     throw std::runtime_error("invalid data-per-rank value - must be at least 1");
  // }
  
  // void loadData(ThisRankData &rankData,
  //               BarnConfig &config,
  //               FromCL &fromCL,
  //               int mpiRank,
  //               int mpiSize)
  // {
  // }



  struct WorkersAbstraction {
    virtual void barrier() = 0;
    int rank = 0;
    int size = 1;
  };

  struct LocalWorker : public WorkersAbstraction
  {
    void barrier() override {}
  };

  struct MPIWorkers : public WorkersAbstraction
  {
    MPIWorkers(barney::mpi::Comm &comm)
      : comm(comm)
    { rank = comm.rank; size = comm.size; }
    
    void barrier() override { comm.barrier(); }
    
    barney::mpi::Comm &comm;
  };

  
}

using namespace hs;

int main(int ac, char **av)
{
  barney::mpi::init(ac,av);

  FromCL fromCL;
  BarnConfig config;
  bool verbose = true;

  DynamicDataLoader loader;
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      loader.addContent(arg);
    } else if (arg == "--default-radius") {
      loader.defaultRadius = std::stoi(av[++i]);
    } else if (arg == "-o") {
      fromCL.outFileName = av[++i];
    } else if (arg == "-ndg") {
      fromCL.ndg = std::stoi(av[++i]);
    } else if (arg == "-dpr") {
      fromCL.dpr = std::stoi(av[++i]);
    } else if (arg == "-h" || arg == "--help") {
      usage();
    } else {
      usage("unknown cmd-line argument '"+arg+"'");
    }    
  }

  barney::mpi::Comm world(MPI_COMM_WORLD);
  if (verbose) {
    world.barrier();
    if (world.rank == 0)
      std::cout << "#hv: hsviewer starting up" << std::endl; fflush(0);
    world.barrier();
  }
  
  // LocalWorker workers;
  MPIWorkers workers(world);
  
  int numDataGroups = fromCL.ndg;
  int dataPerRank = fromCL.dpr;
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
  ThisRankData rankData(dataPerRank);
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


  world.barrier();
  barney::mpi::finalize();
  return 0;
}
