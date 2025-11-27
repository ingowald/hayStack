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

#include "viewer/DataLoader.h"

namespace hs {

  /*! a 'umesh' file of unstructured mesh data */
  struct USDContent : public LoadableContent {
    USDContent(const std::string &fileName)
      : fileName(fileName),
        fileSize(getFileSize(fileName))
    {}

    static void create(DataLoader *loader,
                       const std::string &dataURL)
    {
      loader->addContent(new USDContent(/* this is a plain filename for umesh:*/dataURL));
    }
    
    std::string toString() override
    {
      return "USD{fileName="+fileName+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    size_t projectedSize() override
    { return 2 * fileSize; }
    
    void   executeLoad(DataRank &dataGroup, bool verbose) override;

    const std::string fileName;
    const size_t      fileSize;
  };

}
