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

#include "hayStack/common.h"
#include <vector>

/* parallel renderer abstraction */
namespace hs {
  // using namespace owl::common;
  //
  using range1f = mini::common::interval<float> ;
  
  struct TransferFunction {
    void load(const std::string &fileName);
    
    std::vector<vec4f> colorMap = { vec4f(1.f), vec4f(1.f) };
    range1f domain = { 0.f, 0.f };
    float   baseDensity = 1.f;
  };
  
}
