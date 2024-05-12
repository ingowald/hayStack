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

/*! a "LocalModel" describes the kind of data -- organized in one or
    more data ranks -- that a given app / mpi rank has loaded. note
    that while 'regular' ranks do have at least one data rank, for
    certain processes (like a head node) it is allowd to not have any
    data at all */

#pragma once

#include "DataRank.h"

namespace hs {

  struct LocalModel {
    BoundsData getBounds() const;

    /*! returns whether this rank does *not* have any data; in this
      case it's a passive (head?-)node */
    bool empty() const;
    
    void resize(int numDataRanks);

    /*! returns the number of data groups *on this rank* */
    int size() const;

    /*! this is an optimization in particular for models (like lander)
      where one rank might get multiple "smaller" unstructured
      meshes -- if each of these become their own volumes, with
      their own acceleration strcutre, etc, then that may have some
      negative side effects on performance */
    void mergeUnstructuredMeshes();

    std::vector<DataRank> dataGroups;
  };

} // ::hs
