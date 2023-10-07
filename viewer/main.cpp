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

namespace hs {

  struct FromCL {
    /*! data groups per rank */
    int dpr = 1;
    /*! num data groups */
    int ndg = 1;
    std::vector<std::string> inFileNames;

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

  struct LoadableContent {
    virtual size_t projectedSize() = 0;
    virtual void   executeLoad(DataGroup &dataGroup) = 0;
  };

  inline bool startsWith(const std::string &haystack,
                         const std::string &needle)
  {
    return haystack.substr(0,needle.size()) == needle;
  }
  
  inline size_t getFileSize(const std::string &fileName)
  {
    FILE *file = fopen(fileName.c_str(),"rb");
    if (!file) throw std::runtime_error
                 ("when trying to determine file size: could not open file "+fileName);
    fseek(file,0,SEEK_END);
    size_t size = ftell(file);
    fclose(file);
    return size;
  }
  
  struct SpheresFromFile : public LoadableContent {
    SpheresFromFile(const std::string &dataURL)
    {
      assert(dataURL.substr(0,strlen("spheres://")) == "spheres://");
      const char *s = dataURL.c_str() + strlen("spheres://");
      const char *atSign = strstr(s,"@");
      if (atSign) {
        numPartsToSplitInto = atoi(s);
        s = atSign + 1;
      }
      fileName = s;
      fileSize = getFileSize(fileName);
    }
    
    size_t projectedSize() override
    { return (100/12) * divRoundUp(fileSize, (size_t)numPartsToSplitInto); }
    
    void   executeLoad(DataGroup &dataGroup) override
    {
      SphereSet::SP spheres = SphereSet::create();
      spheres->radius = 0.1f;
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
      std::cout << "#hs: done loading " << prettyNumber(my_count)
                << " spheres from " << fileName << std::endl;
      fclose(file);
    }

    std::string fileName;
    
    size_t fileSize;
    int thisPartID = 0;
    int numPartsToSplitInto = 1;
  };
  


  struct DataLoader {
    virtual void loadData(ThisRankData &rankData,
                          BarnConfig &config,
                          FromCL &fromCL,
                          int mpiRank,
                          int mpiSize) = 0;
  };

  struct DynamicDataLoader : public DataLoader {

    std::vector<std::tuple<size_t /*projected size*/,
                           int    /* linear index, to ensure stable sorting */,
                           LoadableContent *>> allContent;
    std::vector<std::vector<LoadableContent *>> perDataGroup;
    
    void addContent(LoadableContent *content)
    {
      allContent.push_back({content->projectedSize(),int(allContent.size()),content});
    }
    
    void addContent(const std::string &fileName)
    {
      if (startsWith(fileName,"spheres://"))
        addContent(new SpheresFromFile(fileName));
      else
        throw std::runtime_error("un-recognized input file '"+fileName+"'");
    }
    
    void createContent(FromCL &cl)
    {
      if (cl.inFileNames.empty())
        throw std::runtime_error("no input file name(s) specified!?");
      for (auto in : cl.inFileNames)
        addContent(in);
    }
    
    void assignContent(FromCL &fromCL)
    {
      int numDifferentDataGroups = fromCL.ndg;
      perDataGroup.resize(numDifferentDataGroups);

      std::priority_queue<std::pair<size_t,int>> loadedGroups;
      for (int i=0;i<numDifferentDataGroups;i++)
        loadedGroups.push({0,i});

      std::sort(allContent.begin(),allContent.end());
      for (auto addtl : allContent) {
        size_t addtlWeight = std::get<0>(addtl);
        LoadableContent *addtlContent = std::get<2>(addtl);
        auto currentlyLeastLoaded = loadedGroups.top(); loadedGroups.pop();
        size_t currentWeight = currentlyLeastLoaded.first;
        int groupID = currentlyLeastLoaded.second;
        perDataGroup[groupID].push_back(addtlContent);
        loadedGroups.push({currentWeight+addtlWeight,groupID});
      }
    }
    
    void loadDataGroup(DataGroup &dataGroup, int dataGroupID)
    {
      dataGroup.dataGroupID = dataGroupID;
      for (auto content : perDataGroup[dataGroupID])
        content->executeLoad(dataGroup);
    }
    
    void loadData(ThisRankData &rankData,
                  BarnConfig &config,
                  FromCL &fromCL,
                  int mpiRank,
                  int mpiSize) override
    {
      if (fromCL.dpr < 1)
        throw std::runtime_error("invalid data-per-rank value - must be at least 1");
      int dataPerRank = fromCL.dpr;
    
      rankData.dataGroups.resize(dataPerRank);

      int numTotalDataSlots = fromCL.dpr * mpiSize;
      int numDifferentDataGroups = fromCL.ndg;
      if (numTotalDataSlots < numDifferentDataGroups)
        throw std::runtime_error
          ("invalid data group size - it is greater than the total"
           " num data slots we have");
      if (numTotalDataSlots % numDifferentDataGroups)
        throw std::runtime_error
          ("invalid data group size - total total number of data "
           "slots is not a multiple of data groups size");

      createContent(fromCL);
      assignContent(fromCL);
      for (int i=0;i<dataPerRank;i++)
        loadDataGroup(rankData.dataGroups[i],
                      (mpiRank*dataPerRank+i) % numDifferentDataGroups);
    }
  };
  

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
  
}

using namespace hs;

int main(int ac, char **av)
{
  FromCL fromCL;
  BarnConfig config;
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-') {
      fromCL.inFileNames.push_back(arg);
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

  ThisRankData rankData;

  DynamicDataLoader loader;
  loader.loadData(rankData,config,fromCL,0,1);
  
  return 0;
}
