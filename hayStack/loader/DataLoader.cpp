// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/DataLoader.h"
#include "hayStack/loader/TSTris.h"
#include "hayStack/loader/TriangleMesh.h"
#include "hayStack/loader/RAWVolumeContent.h"
#include "hayStack/loader/GESTS.h"
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
#if HS_USD
#include "hayStack/loader/USD.h"
#endif
#include "hayStack/loader/ENDumpContent.h"
#include "hayStack/loader/Capsules.h"
#include "hayStack/loader/IsoDump.h"
#if HS_USE_MULTI_SCATTERING
# include "hayStack/loader/NVDBVolumeContent.h"
#endif
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

    ResourceSpecifier::ResourceSpecifier(std::string resource)
    {
      int pos = resource.find("://");
      if (pos == resource.npos)
        throw std::runtime_error
          ("could not parse resource specifier '"+resource
           +"' - couldn't find '://' in there!?");
      type = resource.substr(0,pos);
      resource = resource.substr(pos+3);

      int colon = resource.find(":");
      if (colon == resource.npos) {
        where = resource;
      } else {
        where = resource.substr(0,colon);
        std::string args = resource.substr(colon+1);
        std::vector<std::string> betweenColons;
        while (true) {
          colon = args.find(":");
          if (colon == args.npos) {
            betweenColons.push_back(args);
            break;
          } else {
            betweenColons.push_back(args.substr(0,colon));
            args = args.substr(colon+1);
          }
        }
        for (auto arg : betweenColons) {
          std::string key=args, value="";
          int equals = arg.find("=");
          if (equals != key.npos) {
            key   = arg.substr(0,equals);
            value = arg.substr(equals+1);
          }
          keyValuePairs[key] = value;
        }
      }

      pos = where.find("@");
      if (pos != where.npos) {
        numParts = stoi(where.substr(0,pos));
        where = where.substr(pos+1);
      }

      if (where[0] == '~')
        where = std::string(std::getenv("HOME"))+where.substr(1);
      
#ifdef _WIN32
      // the name of drive
      pos = where.find("$"); // Find the position of "$"
      if (pos != std::string::npos) { // npos is a constant representing not found
        where.replace(pos, 1, ":"); // Replace 1 character at pos with ":"
      }
#endif
    }
  
    bool ResourceSpecifier::has(const std::string &key) const
    {
      return keyValuePairs.find(key) != keyValuePairs.end();
    }
  
    std::string ResourceSpecifier::get(const std::string &key,
                                       const std::string &defaultValue)
      const
    {
      if (!has(key)) return defaultValue;
      return keyValuePairs.find(key)->second;
    }
  
    vec3f ResourceSpecifier::get_vec3f(const std::string &key,
                                       vec3f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec3f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f,%f",&v.x,&v.y,&v.z);
      else
        n = sscanf(value.c_str(),"%f %f %f",&v.x,&v.y,&v.z);
      if (n != 3)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    vec4f ResourceSpecifier::get_vec4f(const std::string &key,
                                       vec4f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec4f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f,%f,%f",&v.x,&v.y,&v.z,&v.w);
      else
        n = sscanf(value.c_str(),"%f %f %f %f",&v.x,&v.y,&v.z,&v.w);
      if (n != 4)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    vec2f ResourceSpecifier::get_vec2f(const std::string &key,
                                       vec2f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec2f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f",&v.x,&v.y);
      else
        n = sscanf(value.c_str(),"%f %f",&v.x,&v.y);
      if (n != 2)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    int   ResourceSpecifier::get_int(const std::string &key,
                                     int defaultValue) const
    {
      if (!has(key)) return defaultValue;
      std::string value = keyValuePairs.find(key)->second;
      return std::stoi(value);
    }
  
    size_t ResourceSpecifier::get_size(const std::string &key,
                                       size_t defaultValue) const
    {
      if (!has(key)) return defaultValue;

      std::string value = keyValuePairs.find(key)->second;
    
      char magnitude = ' ';
      size_t result;
      sscanf(value.c_str(),"%li%c",&result,&magnitude);
      if (magnitude == ' ')
        ;
      else if (magnitude == 'M')
        result *= 1000000ull;
      else if (magnitude == 'K')
        result *= 1000ull;
      else if (magnitude == 'G')
        result *= 1000000000ull;
      else
        throw std::runtime_error
          ("invalid magnitude specifier '"+std::to_string(magnitude)
           +"' (was expecting 'K', 'M', etc)");
    
      return result;
    }
  
    float ResourceSpecifier::get_float(const std::string &key,
                                       float defaultValue) const
    {
      if (!has(key)) return defaultValue;
      std::string value = keyValuePairs.find(key)->second;
      return std::stof(value);
    }
  
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
    LocalPartitions *DataLoader::loadData(int numDataRanks,
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
      LocalPartitions *localPartitions
        = new LocalPartitions(localDataRanks,numDataRanks);

      for (auto mp : localPartitions->myPartitions)
        loadPartition(mp);
        
      if (!sharedLights.directional.empty() ||
          !sharedLights.quad.empty() ||
          sharedLights.envMap != "")
        for (auto mp : localPartitions->myPartitions) {
        // for (int i=0;i<localPartitions->numPartitionsOnThisRank();i++) {
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
      assert(localPartitions);
      assert(localPartitions->numPartitionsOnThisRank() > 0);
      assert(localPartitions->numPartitionsTotal() > 0);
      return localPartitions;
    }

    void DataLoader::addContent(LoadableContent *content) 
    {
      allContent.push_back(
                           {-(double)content->projectedSize(),
                            int(allContent.size()),content}
                           );
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
#if HS_USE_MULTI_SCATTERING
        else if (url.type == "nvdb")
          NVDBVolumeContent::create(this,url);
#endif
        else if (url.type == "gests") 
          GESTSVolumeContent::create(this,url);
        else if (url.type == "tamr") 
          TAMRContent::create(this,url);
        else if (url.type == "boxes") 
          BoxesFromFile::create(this,url/*contentDescriptor*/);
        else if (url.type == "cylinders") 
          loader::CylindersFromFile::create(this,url);
        else if (url.type == "spumesh")
          // spatially partitioned umeshes
          SpatiallyPartitionedUMeshContent::create(this,url);
        else
          throw std::runtime_error
            ("could not recognize content type '"+url.type+"'");
        return;
      }    
      
      // first, parse all the files we recognise from file name
      // extension:
      if (endsWith(contentDescriptor,".umesh")) {
        UMeshContent::create(this,contentDescriptor);
      } else if (endsWith(contentDescriptor,".vtp")) {
        VTPContent::create(this,contentDescriptor);
      } else if (endsWith(contentDescriptor,".vtu")) {
        VTUContent::create(this,contentDescriptor);
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
#if HS_USE_MULTI_SCATTERING
      } else if (endsWith(contentDescriptor,".nvdb")) {
        NVDBVolumeContent::create(this,addIfRequired("nvdb://",contentDescriptor));
#endif
      } else if (endsWith(contentDescriptor,".tamr")) {
        TAMRContent::create(this,addIfRequired("tamr://",contentDescriptor));
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

      std::sort(allContent.begin(),allContent.end());
      for (auto addtl : allContent) {
        double addtlWeight = std::get<0>(addtl);
        LoadableContent *addtlContent = std::get<2>(addtl);
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
