// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "viewer/render/HayMaker.h"
#include "hayStack/loader/DataLoader.h"
#include "viewer/render/MPIRenderEngine.h"

namespace hm {

  struct HSServer {
    HSServer(RenderEngineInterface *backend);
    RenderEngineInterface *const backend;
  };

  struct HSClient : public MPIRenderEngine {
    HSClient(const std::string &server, int port);
    
    void resize(const vec2i &fbSize, uint32_t *hostRgba) override;
    void renderFrame() override;
    // BoundsData getWorldBounds() override;

    vec2i     fbSize;
    uint32_t *hostRgba;
  };
  
}
