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

#include "hayStack/DataRank.h"
#include "hayStack/MPIWrappers.h"
#include "hayStack/LocalModel.h"

namespace hs {

  struct ResourceSpecifier {
    ResourceSpecifier(std::string s,
                      bool fileNameOnly=false);
    bool has(const std::string &key) const;
    std::string get(const std::string &key, const std::string &defaultValue="") const;
    
    vec3f  get(const std::string &key, vec3f defaultValue) const
    { return get_vec3f(key,defaultValue); }
    vec2f  get(const std::string &key, vec2f defaultValue) const
    { return get_vec2f(key,defaultValue); }
    vec3f  get_vec3f(const std::string &key, vec3f defaultValue) const;
    vec2f  get_vec2f(const std::string &key, vec2f defaultValue) const;
    size_t get_size(const std::string &key, size_t defaultValue) const;
    int    get_int(const std::string &key, int defaultValue) const;
    float  get_float(const std::string &key, float defaultValue) const;
    std::string where;
    std::string type;
    int numParts = 1;
    std::map<std::string,std::string> keyValuePairs;
  };
  
  /*! helper function that checks if a given string has another as prefix */
  bool startsWith(const std::string &haystack,
                  const std::string &needle);
  /*! helper function that checks if a given string has another as prefix */
  bool endsWith(const std::string &haystack,
                const std::string &needle);
  
  /*! helper function that returns the size (in bytes) of a given file
    (or throws an exception if this file cannot be opened */
  size_t getFileSize(const std::string &fileName);
  
  /*! abstraction for some piece of renderable content, such as a
    triangle mesh, a mini scene (or part thereof), a umesh, a
    sub-set of spheres or triangles from a ray spheres/triangles
    file, etc. Key feature of a `LoadableContent` is that it can do
    'deferred' loading in the sense that it can execute the actual
    loading only after different pieces of content have already been
    assinged to different ranks/data groups. */
  struct LoadableContent;

  /*! abstraction for an entity that can load one or more pieces of
    renderable data (i.e., "content") from a file */
  struct DataLoader {
    DataLoader(hs::mpi::Comm &workers)
      : workers(workers)
    {}

    /*! returns rank of process loading the data */
    int myRank() const { return workers.rank; }
    void addContent(const std::string &contentDescriptor);
    
    /*! interface for any type of loadablecontent to add one or more
      different pieces of content */
    void addContent(LoadableContent *);
    
    virtual void assignGroups(int numDataRanks) = 0;
    
    /*! interface for the app to load one particular rank's data
      group(s) */
    virtual void loadDataRank(DataRank &dg,
                               int dataGroupID,
                               bool verbose) = 0;

    /*! actually loads one rank's data, based on which content got
        assigned to which rank. must get called on every worker
        collaboratively - but only on active workers */
    void loadData(LocalModel &localModel,
                  int numDataRanks,
                  int dataPerRank,
                  bool verbose);
    
    /*! list of all (abstract) pieces of content in the scene. */
    std::vector<std::tuple<double /*projected size*/,
                           int    /* linear index, to ensure stable sorting */,
                           LoadableContent *>> allContent;

    struct {
      std::vector<mini::DirLight> directional;
      std::string envMap;
    } sharedLights;
    /*! default radius to use for spheres that do not have a radius specified */
    static float defaultRadius;
    hs::mpi::Comm workers;
  };

  /*! abstraction for some piece of renderable content, such as a
    triangle mesh, a mini scene (or part thereof), a umesh, a
    sub-set of spheres or triangles from a ray spheres/triangles
    file, etc. Key feature of a `LoadableContent` is that it can do
    'deferred' loading in the sense that it can execute the actual
    loading only after different pieces of content have already been
    assinged to different ranks/data groups. */
  struct LoadableContent {
    
    virtual std::string toString() = 0;
    
    /*! return an estimate of how much (GPU) memory this specific
      piece of content is probably going to cost. this can/will be
      used to assign different content to differnet gpus based on
      weight */
    virtual size_t projectedSize() = 0;

    /*! make this content execute the actual load, and add the
      actually loaded content to the specific data group */
    virtual void   executeLoad(DataRank &dataGroup, bool verbose) = 0;
  };

  /*! a data loader that assigns objects dynamically to data groups
    based on their projected weight */
  struct DynamicDataLoader : public DataLoader {
    DynamicDataLoader(hs::mpi::Comm &workers)
      : DataLoader(workers)
    {}
    void assignGroups(int numDataRanks) override;

    void loadDataRank(DataRank &dg,
                       int dataGroupID,
                       bool verbose) override;
  private:
    /*! loadable content per data group, after assigning it */
    std::vector<std::vector<LoadableContent *>> contentOfGroup;
    
  };

}

