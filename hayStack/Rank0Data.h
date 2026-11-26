// SPDX-FileCopyrightText: Copyright (c) 2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"
#include "hayStack/TransferFunction.h"
#include <map>
#include <string>

namespace hs {

  /*! data that only rank 0 has (ie, the result of loading
      Rank0Content); on all other ranks this stays empty */
  struct Rank0Data {
    /*! transfer functions loaded via xf:// content, by name (ie, the
        name that volumes' xfName refers to) */
    std::map<std::string,VolumeTransferFunctionSet::SP> transferFunctions;
  };

}
