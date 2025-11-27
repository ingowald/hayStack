// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
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

#include "viewer/DataLoader.h"
#include <silo.h>

namespace hs {

  /*! a Silo file reader that can load structured and unstructured meshes */
  struct SiloContent : public LoadableContent {
    SiloContent(const std::string &fileName);
    SiloContent(const ResourceSpecifier &resource, int partID);
    ~SiloContent();
    
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    static void create(DataLoader *loader,
                       const ResourceSpecifier &resource);
    std::string toString() override;
    size_t projectedSize() override;
    void   executeLoad(DataRank &dataGroup, bool verbose) override;

    const std::string fileName;
    const size_t      fileSize;
    const int         partID;
    std::string       requestedVar; // Optional: specific variable to load
    
  private:
    // Helper methods to read different mesh types
    void readQuadMesh(DBfile *dbfile, const char *meshName, 
                      DataRank &dataRank, bool verbose);
    void readUcdMesh(DBfile *dbfile, const char *meshName,
                     DataRank &dataRank, bool verbose);
    void readPointMesh(DBfile *dbfile, const char *meshName,
                       DataRank &dataRank, bool verbose);
    
    // Helper to read variables
    void readQuadVar(DBfile *dbfile, const char *varName,
                     const char *meshName, DataRank &dataRank, bool verbose);
  };

}

