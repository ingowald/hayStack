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

namespace hs {

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
    void addContent(const std::string &contentDescriptor);
    
    /*! interface for any type of loadablecontent to add one or more
      different pieces of content */
    void addContent(LoadableContent *);

    virtual void assignGroups(int numDataGroups) = 0;
    
    /*! interface for the app to load one particular rank's data
      group(s) */
    virtual void loadDataGroup(DataGroup &dg,
                               int dataGroupID,
                               bool verbose) = 0;
    

    /*! list of all (abstract) pieces of content in the scene. */
    std::vector<std::tuple<size_t /*projected size*/,
                           int    /* linear index, to ensure stable sorting */,
                           LoadableContent *>> allContent;

    /*! default radius to use for spheres that do not have a radius specified */
    static float defaultRadius;
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
    virtual void   executeLoad(DataGroup &dataGroup, bool verbose) = 0;
  };

  struct UMeshContent : public LoadableContent {
    UMeshContent(const std::string &fileName)
      : fileName(fileName),
        fileSize(getFileSize(fileName))
    {}

    static void create(DataLoader *loader,
                       const std::string &dataURL)
    {
      loader->addContent(new UMeshContent(/* this is a plain filename for umesh:*/dataURL));
    }
    
    std::string toString() override
    {
      return "UMesh{fileName="+fileName+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    size_t projectedSize() override
    { return 2 * fileSize; }
    
    void   executeLoad(DataGroup &dataGroup, bool verbose) override
    {
      dataGroup.unsts.push_back(umesh::UMesh::loadFrom(fileName));
    }

    const std::string fileName;
    const size_t      fileSize;
  };

  /*! a file of 'raw' spheres */
  struct SpheresFromFile : public LoadableContent {
    SpheresFromFile(const std::string &fileName,
                    size_t fileSize,
                    int thisPartID,
                    int numPartsToSplitInto,
                    float radius);
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    size_t projectedSize() override;
    void   executeLoad(DataGroup &dataGroup, bool verbose) override;

    std::string toString() override
    {
      return "Spheres{fileName="+fileName+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(numPartsToSplitInto)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }

    const float radius;
    const std::string fileName;
    const size_t fileSize;
    const int thisPartID = 0;
    const int numPartsToSplitInto = 1;
  };
  
  /*! "Tim Sandstrom" type ".tri" files */
  struct TSTriContent : public LoadableContent {
    TSTriContent(const std::string &fileName,
                 size_t fileSize,
                 int thisPartID,
                 int numPartsToSplitInto,
                 float radius);
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    size_t projectedSize() override;
    void   executeLoad(DataGroup &dataGroup, bool verbose) override;

    std::string toString() override
    {
      return "Tim-Triangles{fileName="+fileName+", part "+std::to_string(thisPartID)+" of "
        + std::to_string(numPartsToSplitInto)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    const std::string fileName;
    const size_t fileSize;
    const int thisPartID = 0;
    const int numPartsToSplitInto = 1;
  };
  
  /*! a data loader that assigns objects dynamically to data groups
    based on their projected weight */
  struct DynamicDataLoader : public DataLoader {
    void assignGroups(int numDataGroups) override;
    
    void loadDataGroup(DataGroup &dg,
                       int dataGroupID,
                       bool verbose) override;
  private:
    /*! loadable content per data group, after assigning it */
    std::vector<std::vector<LoadableContent *>> contentOfGroup;
    
  };

}

