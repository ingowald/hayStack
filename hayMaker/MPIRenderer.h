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

#include "hayMaker/Renderer.h"
#include "barney/MPIWrappers.h"

/* parallel renderer abstraction */
namespace hs {

  using barney::mpi::Comm;
  
  /*! base abstraction for any renderer - no matter whether its a
    single node or multiple workers on the back */
  struct MPIRenderer : public Renderer {
    MPIRenderer(Comm &comm,
                Renderer *passThrough = 0);
    
    void renderFrame() override;
    void resize(const vec2i &fbSize, uint32_t *hostRgba) override;
    void resetAccumulation() override;
    void setCamera(const Camera &camera) override;
    // void setXF(const range1f &domain,
    //            const std::vector<vec4f> &colors) override;
    void setTransferFunction(const TransferFunction &xf) override;
    void screenShot() override;
    void terminate() override;
    void setLights(float ambient,
                   const std::vector<PointLight> &pointLights,
                   const std::vector<DirLight> &dirLights) override;

    static void runWorker(Comm &comm,
                          Renderer *client);

  private:
    template<typename T>
    void sendToWorkers(const std::vector<T> &t);
    
    template<typename T>
    void sendToWorkers(const T &t);

    void checkEndOfMessage();
    void sendEndOfMessage();
    
    Comm &comm;
    
    /*! passthrough-renderer on master node */
    Renderer *passThrough = 0;

    int eomIdentifierBase = 0x12345;
  };

}
