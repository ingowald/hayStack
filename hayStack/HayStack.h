// ======================================================================== //
// Copyright 2022-2025 Ingo Wald                                            //
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

/*! a hay-*stack* is a description of data-parallel data */

#pragma once

#include <miniScene/Scene.h>

#define HAYSTACK_NYI() throw std::runtime_error(std::string(__PRETTY_FUNCTION__)+" not yet implemented")

namespace hs {
  using namespace mini;
  using range1f = mini::common::interval<float>;
  
  struct BoundsData {
    void extend(const BoundsData &other)
    { spatial.extend(other.spatial); scalars.extend(other.scalars); }
    
    box3f   spatial;
    range1f scalars;
  };

  inline std::ostream &operator<<(std::ostream &o, const BoundsData &bd)
  { o << "{" << bd.spatial << ":" << bd.scalars << "}"; return o; }
  
} // ::hs
