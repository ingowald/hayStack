// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "miniScene/common/math/box.h"
#include <string.h>
#include <mutex>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

namespace hs {
  using namespace mini::common;
  // using namespace owl::common;

#define HAYSTACK_NYI() throw std::runtime_error(std::string(__PRETTY_FUNCTION__)+" not yet implemented")

}

