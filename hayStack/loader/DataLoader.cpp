// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/DataLoader.h"
#include "hayStack/loader/TSTris.h"
#include "hayStack/loader/TriangleMesh.h"
#include "hayStack/loader/RAWVolumeContent.h"
#include "hayStack/loader/TAMRContent.h"
#include "hayStack/loader/CylindersFromFile.h"
#include "hayStack/loader/SpheresFromFile.h"
#include "hayStack/loader/MaterialsTest.h"
#include "hayStack/loader/BoxesFromFile.h"
#include "hayStack/loader/MiniContent.h"
#include "hayStack/loader/UMeshContent.h"
#include "hayStack/loader/OBJContent.h"
#include "hayStack/loader/DGEFContent.h"
#include "hayStack/loader/VTPContent.h"
#include "hayStack/loader/ColorMapper.h"
#include "hayStack/loader/VTUContent.h"
#include "hayStack/loader/XFContent.h"
#if HS_USD
#include "hayStack/loader/USD.h"
#endif
#include "hayStack/loader/ENDumpContent.h"
#include "hayStack/loader/Capsules.h"
#include "hayStack/loader/IsoDump.h"
# include "hayStack/loader/NVDBVolumeContent.h"
#include "hayStack/ColorMap.h"

namespace hs {
  namespace loader {
    bool verbose = false;
    // extern bool verbose;
    
    /*! default radius to use for spheres that do not have a radius specified */
    float DataLoader::defaultRadius = .1f;

    DataLoader::DataLoader(hs::Comm::SP workers)
      : workers(workers)
    {}

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
      if (!file)
        // return 0;
        throw std::runtime_error
          ("when trying to determine file size: could not open file "+fileName);
      fseek(file,0,SEEK_END);
#ifdef _WIN32
      size_t size = _ftelli64(file);
#else
      size_t size = ftell(file);
#endif
      fclose(file);
      return size;
    }

#if HS_USD
    mini::Scene::SP loadUSD(const std::string &fileName);
#endif
  
    mini::Scene::SP loadEnvMap(const std::string &fileName)
    {
      if (fileName.empty()) return mini::Scene::create();
      if (endsWith(fileName,".mini"))
        return mini::Scene::load(fileName);
      if (endsWith(fileName,".usda")) {
#if HS_USD
        mini::Scene::SP scene = loadUSD(fileName);
        scene->instances = {};
        return scene;
#else
        throw std::runtime_error("USD support not build in");
#endif
      }
      throw std::runtime_error("unsupported env-map light filename "+fileName);
    }

    /*! actually loads one rank's data, based on which content got
      assigned to which rank. must get called on every worker
      collaboratively - but only on active workers */
    void DataLoader::loadData(LocalPartitions &localPartitions,
                              int numDataRanks,
                              int dataPerRank)
    {
      ColorMapLibrary::init();
      if (dataPerRank == 0) {
        if (workers->size < numDataRanks) {
          dataPerRank = numDataRanks / workers->size;
        } else {
          dataPerRank = 1;
        }
      }
      assert(numDataRanks > 0);
      assert(dataPerRank > 0);

      if (numDataRanks % dataPerRank) {
        std::cout << "warning - num data groups is not a "
                  << "multiple of data groups per rank?!" << std::endl;
        std::cout << "increasing num data groups to " << numDataRanks
                  << " to ensure equal num data groups for each rank" << std::endl;
      }
  
      assignGroups(numDataRanks);
      std::vector<int> localDataRanks;

      for (int i=0;i<dataPerRank;i++) {
        int dataGroupID = (workers->rank*dataPerRank+i) % numDataRanks;
        if (1 || verbose) {
          std::stringstream ss;
          ss << "#hv: worker #" << workers->rank
             << " loading global data group ID " << dataGroupID
             << " into slot " << workers->rank << "." << i << ":";
          std::cout << ss.str()
                    << std::endl << std::flush;
        }
        localDataRanks.push_back(dataGroupID);
      }
      assert(!localDataRanks.empty());
      localPartitions.createPartitions(localDataRanks,numDataRanks);

      for (auto mp : localPartitions.myPartitions)
        loadPartition(mp);

      for (auto content : replicatedContent)
        content->executeLoad(localPartitions.replicated);

      if (workers->rank == 0)
        for (auto content : rank0Content)
          content->executeLoad(localPartitions.rank0);
        
      const SharedLights &sharedLights = localPartitions.replicated.sharedLights;
      if (!sharedLights.directional.empty() ||
          !sharedLights.quad.empty() ||
          sharedLights.envMap != "")
        for (auto mp : localPartitions.myPartitions) {
        // for (int i=0;i<localPartitions.numPartitionsOnThisRank();i++) {
          mini::Scene::SP lights
            = loadEnvMap(sharedLights.envMap);
          
          lights->dirLights = sharedLights.directional;
          lights->quadLights = sharedLights.quad;
          mp->minis.push_back(lights);
        }

      if (verbose) {
        workers->barrier();
        if (workers->rank == 0)
          std::cout << "#hv: all workers done loading their data..." << std::endl;
        workers->barrier();
      }
      assert(localPartitions.numPartitionsOnThisRank() > 0);
      assert(localPartitions.numPartitionsTotal() > 0);
    }

    void DataLoader::addContent(DistributedContent *content) 
    {
      distributedContent.push_back(
                           {-(double)content->projectedSize(),
                            int(distributedContent.size()),content}
                           );
    }

    void DataLoader::addContent(ReplicatedContent *content) 
    {
      replicatedContent.push_back(content);
    }

    void DataLoader::addContent(Rank0Content *content) 
    {
      rank0Content.push_back(content);
    }

    std::string addIfRequired(std::string prefix, std::string s)
    {
      if (s.substr(0,prefix.size()) == prefix)
        return s;
      else
        return prefix+s;
    }

    /*! helper function that creates an mini::ANARIMaterial from
        parameters specified in the given dataURL */
    mini::ANARIMaterial::SP parseAnariMaterial(const ResourceSpecifier &data)
    {
      mini::ANARIMaterial::SP mat      
        = std::make_shared<mini::ANARIMaterial>();
      mat->opacity   = data.get_float("opacity",1.f);
      mat->baseColor = data.get_vec3f("baseColor",vec3f(.7f));
      mat->metallic  = data.get_float("metallic",0.f);
      mat->roughness = data.get_float("roughness",.1f);
      mat->emissive  = data.get("emissive",vec3f(0.f));
      return mat;      
    }
    

    void DataLoader::addContent(const std::string &contentDescriptor)
    {
      bool hasExplicitType
        = contentDescriptor.find("://") != contentDescriptor.npos;

      if (hasExplicitType) {
        ResourceSpecifier url(contentDescriptor);
        if (url.type == "colorMapping" || url.type == "cm")
          loader::ColorMappingMetaContent::create(this,url);
        else if (url.type == "spheres")
          loader::SpheresFromFile::create(this,url);
        else if (url.type == "ts.tri") 
          TSTriContent::create(this,contentDescriptor);
        else if (url.type == "iso-dump") 
          IsoDumpContent::create(this,contentDescriptor);
        else if (url.type == "rgbtris") 
          loader::RGBTris::create(this,contentDescriptor);
        else if (url.type == "materialsTest") 
          MaterialsTest::create(this,contentDescriptor);
        else if (url.type == "capsules") 
          loader::Capsules::create(this,contentDescriptor);
        else if (url.type == "raw") 
          RAWVolumeContent::create(this,url);
        else if (url.type == "nvdb")
          NVDBVolumeContent::create(this,url);
        else if (url.type == "tamr") 
          TAMRContent::create(this,url);
        else if (url.type == "boxes") 
          BoxesFromFile::create(this,url/*contentDescriptor*/);
        else if (url.type == "cylinders") 
          loader::CylindersFromFile::create(this,url);
        else if (url.type == "spumesh")
          // spatially partitioned umeshes
          SpatiallyPartitionedUMeshContent::create(this,url);
        else if (url.type == "vtu")
          VTUContent::create(this,url);
        else if (url.type == "umesh")
          UMeshContent::create(this,url);
        else if (url.type == "xf")
          XFContent::create(this,url);
        else
          throw std::runtime_error
            ("could not recognize content type '"+url.type+"'");
        return;
      }    
      
      // first, parse all the files we recognise from file name
      // extension:
      if (endsWith(contentDescriptor,".umesh")) {
        UMeshContent::create(this,addIfRequired("umesh://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".vtp")) {
        VTPContent::create(this,contentDescriptor);
      } else if (endsWith(contentDescriptor,".vtu")) {
        VTUContent::create(this,addIfRequired("vtu://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".obj")) {
        OBJContent::create(this,contentDescriptor);
      } else if (endsWith(contentDescriptor,".dgef")) {
        DGEFContent::create(this,contentDescriptor);
#if HS_USD
      } else if (endsWith(contentDescriptor,".usda")) {
        USDContent::create(this,contentDescriptor);
#endif
      } else if (endsWith(contentDescriptor,".caps")) {
        loader::Capsules::create(this,addIfRequired("capsules://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".vmdcyls")) {
        loader::VMDCyls::create(this,addIfRequired("vmdcyls://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".vmdspheres")) {
        loader::VMDSpheres::create(this,addIfRequired("vmdspheres://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".vmdmesh")) {
        loader::VMDMesh::create(this,addIfRequired("vmdmesh://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".tri")) {
        loader::TSTriContent::create(this,addIfRequired("ts.tri://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".tstri1")) {
        loader::TSTriContent::create(this,addIfRequired("ts.tri://",contentDescriptor)+":stride=4:sidx=3");
      } else if (endsWith(contentDescriptor,".rgbtris")) {
        loader::RGBTris::create(this,addIfRequired("rgbtris://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".mini")) {
        MiniContent::create(this,contentDescriptor);
      } else if (endsWith(contentDescriptor,".hsmesh")) {
        loader::HSMesh::create(this,addIfRequired("hsmesh://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".raw")) {
        RAWVolumeContent::create(this,addIfRequired("raw://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".nvdb")) {
        NVDBVolumeContent::create(this,addIfRequired("nvdb://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".tamr")) {
        TAMRContent::create(this,addIfRequired("tamr://",contentDescriptor));
      } else if (endsWith(contentDescriptor,".xf")) {
        XFContent::create(this,addIfRequired("xf://",contentDescriptor));
      } else {
        throw std::runtime_error("no explicit 'loader://' specified, and could not determine type from file extension, either...!?");
      }
    }
    
    void DynamicDataLoader::assignGroups(int numDifferentDataRanks)
    {
      assert(numDifferentDataRanks > 0);
      contentOfGroup.resize(numDifferentDataRanks);

      std::priority_queue<std::pair<double,int>> loadedGroups;
      for (int i=0;i<numDifferentDataRanks;i++)
        loadedGroups.push({0,i});

      std::sort(distributedContent.begin(),distributedContent.end());
      for (auto addtl : distributedContent) {
        double addtlWeight = std::get<0>(addtl);
        DistributedContent *addtlContent = std::get<2>(addtl);
        auto currentlyLeastLoaded = loadedGroups.top(); loadedGroups.pop();
        double currentWeight = currentlyLeastLoaded.first;
        int groupID = currentlyLeastLoaded.second;
        contentOfGroup[groupID].push_back(addtlContent);
        loadedGroups.push({currentWeight+addtlWeight,groupID});
      }
      workers->barrier();
      if (workers->rank == 0) {
        std::cout << "content assignment: created " << contentOfGroup.size() << " data groups" << std::endl;
        for (int i=0;i<contentOfGroup.size();i++) {
          std::cout << "= data group " << i << std::endl;
          for (auto content : contentOfGroup[i])
            std::cout << "  - " << content->toString() << std::endl;
        }
      }
      workers->barrier();
    }
    
    void DynamicDataLoader::loadPartition(OnePartition *partition)
    {
      int dataGroupID = partition->partitionsIndex.global.index;
      if (contentOfGroup[dataGroupID].empty())
        std::cout << MINI_TERMINAL_RED
                  << "#hs: WARNING: data group "
                  << dataGroupID << " is empty!?"
                  << MINI_TERMINAL_DEFAULT << std::endl;
      for (auto content : contentOfGroup[dataGroupID]) {
        if (verbose)
          std::cout << " - #" << workers->rank << " loading content "
                    << content->toString() << std::endl << std::flush;
        content->executeLoad(*partition);
      }
      if (verbose)
        std::cout << " - #" << workers->rank << " done loading." << std::endl;
    }
  
  }
}
