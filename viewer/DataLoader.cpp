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
#include "viewer/content/TriangleMesh.h"
#include "viewer/content/RAWVolumeContent.h"
#include "viewer/content/CylindersFromFile.h"
#include "viewer/content/SpheresFromFile.h"
#include "viewer/content/MaterialsTest.h"
#include "viewer/content/BoxesFromFile.h"
#include "viewer/content/MiniContent.h"
#include "viewer/content/UMeshContent.h"
#include "viewer/content/OBJContent.h"
#include "viewer/content/DistData.h"
#include "viewer/content/Capsules.h"

namespace hs {

  /*! default radius to use for spheres that do not have a radius specified */
  float DataLoader::defaultRadius = .1f;

  ResourceSpecifier::ResourceSpecifier(std::string resource,
                                       bool fileNameOnly)
  {
    if (fileNameOnly) {
      where = resource;
      return;
    }
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
    if (!file) return 0;
    // throw std::runtime_error
    //              ("when trying to determine file size: could not open file "+fileName)
      // ;
    fseek(file,0,SEEK_END);
#ifdef _WIN32
    size_t size = _ftelli64(file);
#else
    size_t size = ftell(file);
#endif
    fclose(file);
    return size;
  }

  /*! actually loads one rank's data, based on which content got
    assigned to which rank. must get called on every worker
    collaboratively - but only on active workers */
  void DataLoader::loadData(LocalModel &localModel,
                            int numDataRanks,
                            int dataPerRank,
                            bool verbose)
  {
    if (dataPerRank == 0) {
      if (workers.size < numDataRanks) {
        dataPerRank = numDataRanks / workers.size;
      } else {
        dataPerRank = 1;
      }
    }

    if (numDataRanks % dataPerRank) {
      std::cout << "warning - num data groups is not a "
                << "multiple of data groups per rank?!" << std::endl;
      std::cout << "increasing num data groups to " << numDataRanks
                << " to ensure equal num data groups for each rank" << std::endl;
    }
  
    assignGroups(numDataRanks);
    localModel.resize(dataPerRank);

    for (int i=0;i<dataPerRank;i++) {
      int dataGroupID = (workers.rank*dataPerRank+i) % numDataRanks;
      if (verbose) {
        std::stringstream ss;
        ss << "#hv: worker #" << workers.rank
           << " loading global data group ID " << dataGroupID
           << " into slot " << workers.rank << "." << i << ":";
        std::cout << ss.str()
                  << std::endl << std::flush;
      }
      // if (verbose) {
      //   // for (int r=0;r<workers.rank;r++) 
      //   //   workers.barrier();
      //   std::cout << "#hv: worker #" << workers.rank
      //             << " loading global data group ID " << dataGroupID
      //             << " into slot " << workers.rank << "." << i << ":"
      //             << std::endl << std::flush;
      //   usleep(100);
      //   fflush(0);
      // }
      loadDataRank(localModel.dataGroups[i],
                    dataGroupID,
                    verbose);
      // if (verbose) 
      //   for (int r=workers.rank;r<workers.size;r++) 
      //     workers.barrier();
    }


    if (!sharedLights.directional.empty() || sharedLights.envMap != "")
      for (int i=0;i<localModel.dataGroups.size();i++) {
        mini::Scene::SP lights
          = (sharedLights.envMap == "")
          ? mini::Scene::create()
          : mini::Scene::load(sharedLights.envMap);
          
        lights->dirLights = sharedLights.directional;
        localModel.dataGroups[i].minis.push_back(lights);
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
    allContent.push_back({(double)content->projectedSize(),int(allContent.size()),content});
  }

  std::string addIfRequired(std::string prefix, std::string s)
  {
    if (s.substr(0,prefix.size()) == prefix)
      return s;
    else
      return prefix+s;
  }
  
  void DataLoader::addContent(const std::string &contentDescriptor)
  {
    // if (startsWith(contentDescriptor,"spheres://")) {
    //   SpheresFromFile::create(this,contentDescriptor);
    // } else if (startsWith(contentDescriptor,"cylinders://")) {
    //   CylindersFromFile::create(this,contentDescriptor);
    // } else

    // first, parse all the files we recognise from file name
    // extension:
    if (endsWith(contentDescriptor,".umesh")) {
      UMeshContent::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".obj")) {
      OBJContent::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".caps")) {
      content::Capsules::create(this,addIfRequired("capsules://",contentDescriptor));
    } else if (endsWith(contentDescriptor,".vmdcyls")) {
      content::VMDCyls::create(this,addIfRequired("vmdcyls://",contentDescriptor));
    } else if (endsWith(contentDescriptor,".vmdspheres")) {
      content::VMDSpheres::create(this,addIfRequired("vmdspheres://",contentDescriptor));
    } else if (endsWith(contentDescriptor,".vmdmesh")) {
      content::VMDMesh::create(this,addIfRequired("vmdmesh://",contentDescriptor));
    } else if (endsWith(contentDescriptor,".rgbtris")) {
      content::RGBTris::create(this,addIfRequired("rgbtris://",contentDescriptor));
    } else if (endsWith(contentDescriptor,".mini")) {
      MiniContent::create(this,contentDescriptor);
    } else if (endsWith(contentDescriptor,".raw")) {
      RAWVolumeContent::create(this,ResourceSpecifier(contentDescriptor,true));
    } else {
      ResourceSpecifier url(contentDescriptor);
      if (url.type == "spheres")
        content::SpheresFromFile::create(this,url);
      else if (url.type == "ts.tri") 
        TSTriContent::create(this,contentDescriptor);
      else if (url.type == "materialsTest") 
        MaterialsTest::create(this,contentDescriptor);
      else if (url.type == "capsules") 
        content::Capsules::create(this,contentDescriptor);
      // else if (url.type == "en-dump")
      //   ENDumpContent::create(this,contentDescriptor);
      else if (url.type == "raw") 
        RAWVolumeContent::create(this,url);
      else if (url.type == "boxes") 
        BoxesFromFile::create(this,url/*contentDescriptor*/);
      else if (url.type == "cylinders") 
        content::CylindersFromFile::create(this,url);
      else if (url.type == "spumesh")
        // spatially partitioned umeshes
        SpatiallyPartitionedUMeshContent::create(this,url);
      else
        throw std::runtime_error
          ("could not recognize content type '"+url.type+"'");
    }    
    // else if (startsWith(contentDescriptor,"en-dump://")) {
    //   ENDumpContent::create(this,contentDescriptor);
    // } else if (startsWith(contentDescriptor,"ts.tri://")) {
    //   TSTriContent::create(this,contentDescriptor);
    // } else
    //   throw std::runtime_error("un-recognized content descriptor '"+contentDescriptor+"'");
  }
    
  void DynamicDataLoader::assignGroups(int numDifferentDataRanks)
  {
    contentOfGroup.resize(numDifferentDataRanks);

    std::priority_queue<std::pair<double,int>> loadedGroups;
    for (int i=0;i<numDifferentDataRanks;i++)
      loadedGroups.push({0,i});

    std::sort(allContent.begin(),allContent.end());
    for (auto addtl : allContent) {
      double addtlWeight = - std::get<0>(addtl);
      LoadableContent *addtlContent = std::get<2>(addtl);
      auto currentlyLeastLoaded = loadedGroups.top(); loadedGroups.pop();
      double currentWeight = currentlyLeastLoaded.first;
      int groupID = currentlyLeastLoaded.second;
      contentOfGroup[groupID].push_back(addtlContent);
      loadedGroups.push({currentWeight+addtlWeight,groupID});
    }
  }
    
  void DynamicDataLoader::loadDataRank(DataRank &dataGroup,
                                        int dataGroupID,
                                        bool verbose)
  {
    dataGroup.dataGroupID = dataGroupID;
    if (contentOfGroup[dataGroupID].empty())
      std::cout << MINI_TERMINAL_RED
                << "#hs: WARNING: data group " << dataGroupID << " is empty!?"
                << MINI_TERMINAL_DEFAULT << std::endl;
    for (auto content : contentOfGroup[dataGroupID]) {
      if (verbose)
        std::cout << " - loading content " << content->toString() << std::endl << std::flush;
      content->executeLoad(dataGroup, verbose);
    }
  }
  
}
