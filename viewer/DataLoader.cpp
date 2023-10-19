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
#include "viewer/content/TSTris.h"
#include "viewer/content/SpheresFromFile.h"
#include "viewer/content/MiniContent.h"
#include "viewer/content/UMeshContent.h"
#include "viewer/content/OBJContent.h"

namespace hs {

  /*! default radius to use for spheres that do not have a radius specified */
  float DataLoader::defaultRadius = .1f;

  bool startsWith(const std::string &haystack,
                         const std::string &needle)
  {
    return haystack.substr(0,needle.size()) == needle;
  }

  bool endsWith(const std::string &haystack,
                const std::string &needle)
  {
    if (haystack.size() < needle.size()) return false;
    return haystack.substr(haystack.size()-needle.size(),haystack.size()) == needle;
  }
  
  size_t getFileSize(const std::string &fileName)
  {
    FILE *file = fopen(fileName.c_str(),"rb");
    if (!file) throw std::runtime_error
                 ("when trying to determine file size: could not open file "+fileName);
    fseek(file,0,SEEK_END);
    size_t size = ftell(file);
    fclose(file);
    return size;
  }

    /*! actually loads one rank's data, based on which content got
        assigned to which rank. must get called on every worker
        collaboratively - but only on active workers */
  void DataLoader::loadData(barney::mpi::Comm &workers,
                            ThisRankData &rankData,
                            int numDataGroups,
                            int dataPerRank,
                            bool verbose)
  {
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
  
    assignGroups(numDataGroups);
    rankData.resize(dataPerRank);
    for (int i=0;i<dataPerRank;i++) {
      int dataGroupID = (workers.rank*dataPerRank+i) % numDataGroups;
      if (verbose) {
        for (int r=0;r<workers.rank;r++) 
          workers.barrier();
        std::cout << "#hv: worker #" << workers.rank
                  << " loading global data group ID " << dataGroupID
                  << " into slot " << workers.rank << "." << i << ":"
                  << std::endl << std::flush;
        usleep(100);
        fflush(0);
      }
      loadDataGroup(rankData.dataGroups[i],
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
  






  void DataLoader::addContent(LoadableContent *content) 
  {
    allContent.push_back({content->projectedSize(),int(allContent.size()),content});
  }
    
  void DataLoader::addContent(const std::string &contentDescriptor)
  {
    if (startsWith(contentDescriptor,"spheres://")) {
      SpheresFromFile::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".umesh")) {
      UMeshContent::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".obj")) {
      OBJContent::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".mini")) {
      MiniContent::create(this,contentDescriptor);
    } else if (startsWith(contentDescriptor,"ts.tri://")) {
      TSTriContent::create(this,contentDescriptor);
    } else
      throw std::runtime_error("un-recognized content descriptor '"+contentDescriptor+"'");
  }
    
  void DynamicDataLoader::assignGroups(int numDifferentDataGroups)
  {
    contentOfGroup.resize(numDifferentDataGroups);

    std::priority_queue<std::pair<double,int>> loadedGroups;
    for (int i=0;i<numDifferentDataGroups;i++)
      loadedGroups.push({0,i});

    std::sort(allContent.begin(),allContent.end());
    for (auto addtl : allContent) {
      double addtlWeight = - std::get<0>(addtl);
      LoadableContent *addtlContent = std::get<2>(addtl);
      auto currentlyLeastLoaded = loadedGroups.top(); loadedGroups.pop();
      size_t currentWeight = currentlyLeastLoaded.first;
      int groupID = currentlyLeastLoaded.second;
      contentOfGroup[groupID].push_back(addtlContent);
      loadedGroups.push({currentWeight+addtlWeight,groupID});
    }
  }
    
  void DynamicDataLoader::loadDataGroup(DataGroup &dataGroup,
                                        int dataGroupID,
                                        bool verbose)
  {
    dataGroup.dataGroupID = dataGroupID;
    if (contentOfGroup[dataGroupID].empty())
      std::cout << OWL_TERMINAL_RED
                << "#hs: WARNING: data group " << dataGroupID << " is empty!?"
                << OWL_TERMINAL_DEFAULT << std::endl;
    for (auto content : contentOfGroup[dataGroupID]) {
      if (verbose)
        std::cout << " - loading content " << content->toString() << std::endl << std::flush;
      content->executeLoad(dataGroup, verbose);
    }
  }
  
}
