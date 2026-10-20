// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/common.h"
#if HS_USE_MULTI_SCATTERING
#include "hayStack/VolumeScatter.h"
#endif
#include <cstdint>
#include <vector>

namespace hs {
  // using namespace owl::common;
  //
  using range1f = mini::common::interval<float> ;
  
  struct TransferFunction {
    void load(const std::string &fileName);
    void save(const std::string &fileName) const;
    void set(const mini::common::vec4f *data, size_t size)
    { colorMap = { data, data+size }; };
      
    range1f getDomain() const;
    
    std::vector<mini::common::vec4f> colorMap
#if HS_USE_MULTI_SCATTERING
      = { mini::common::vec4f(1.f, 1.f, 1.f, 0.f),
          mini::common::vec4f(1.f) };
#else
      = { mini::common::vec4f(1.f), mini::common::vec4f(1.f) };
#endif

    range1f absDomain = { +1.f, -1.f };
    range1f relDomain = { 0.f, 100.f };
    
    float   baseDensity = 100.f;
  };

#if HS_USE_MULTI_SCATTERING
  /*! A second RGBA transfer function used as the principled volume's
      emission map. Alpha masks emission independently of opacity. */
  struct VolumeTransferFunctionSet {
    TransferFunction opacity;
    TransferFunction emission;
    VolumeScatterSettings scatterSettings;
  };

  void saveVolumeTransferFunctions(const std::string &fileName,
                                   const VolumeTransferFunctionSet &set);
  VolumeTransferFunctionSet
  loadVolumeTransferFunctions(const std::string &fileName);
#endif

  inline bool isUnsetTransferFunctionDomain(const range1f &domain)
  {
    return domain.upper < domain.lower;
  }
  
}
