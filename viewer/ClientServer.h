// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "viewer/render/HayMaker.h"
#include "hayStack/loader/DataLoader.h"
#include "viewer/render/MPIRenderEngine.h"

namespace hm {

  struct SocketComm;

  struct HSServer {
    HSServer(RenderEngineInterface *backend);
    RenderEngineInterface *const backend;

    struct {
      vec2i size;
      std::vector<uint32_t> color;
      std::vector<float> depth;
    } fb;
    std::shared_ptr<SocketComm> comm;
  };
  
  struct HSServerREI : public PassthroughREI {
    HSServerREI(HSServer *server,
                RenderEngineInterface *pt);
    void renderFrame() override;
    void resize(const vec2i &fbSize,
                uint32_t *fbColor,
                float *fbDepth) override;
    HSServer *const server;
  };

  struct HSClient : public MPIRenderEngine {
    HSClient(const std::string &server, int port);
    
    void resize(const vec2i &fbSize,
                uint32_t *fbColor,
                float *fbDepth) override;
    // void resize(const vec2i &fbSize, uint32_t *hostRgba) override;
    void renderFrame() override;
    // BoundsData getWorldBounds() override;

    vec2i     fbSize;
    uint32_t *hostColor;
    float    *hostDepth;
  };
  
}
