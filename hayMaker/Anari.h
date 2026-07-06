// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include <anari/anari_cpp.hpp>
#include <anari/anari_cpp/ext/linalg.h>

namespace hm {
  
  typedef anari::Material MaterialHandle;
  typedef anari::Sampler  TextureHandle;
  typedef anari::Group    GroupHandle;
  typedef anari::Light    LightHandle;
  typedef anari::Surface  GeomHandle;
  typedef anari::Volume   VolumeHandle;

}


