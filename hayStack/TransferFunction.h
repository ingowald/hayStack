// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/common.h"
#include "hayStack/VolumeScatter.h"
#include <cstdint>
#include <vector>

namespace hs {
  using range1f = mini::common::interval<float> ;
  
  struct TransferFunction {
    void read(std::istream &in);
    void write(std::ostream &out) const;
    
    void set(const mini::common::vec4f *data, size_t size)
    { colorMap = { data, data+size }; };
      
    range1f getDomain() const;
    std::string toString() const;
    
    std::vector<mini::common::vec4f> colorMap
    //       = { mini::common::vec4f(1.f, 1.f, 1.f, 0.f),
    //           mini::common::vec4f(1.f) };
    = { mini::common::vec4f(1.f), mini::common::vec4f(1.f) };
    
    range1f domain = { +1.f, -1.f };
    
    float   baseDensity = 100.f;
  };

  /*! A second RGBA transfer function used as the principled volume's
    emission map. Alpha masks emission independently of opacity. */
  struct VolumeTransferFunctionSet {
    typedef std::shared_ptr<VolumeTransferFunctionSet> SP;

    /*! initializes the opacity color map to the color map library's
        default map (#0), and the emission color map to its "white"
        map */
    VolumeTransferFunctionSet();
    
    void save(const std::string &fileName) const;
    static VolumeTransferFunctionSet load(const std::string &fileName);
    std::string toString() const;
    
    TransferFunction opacity;
    TransferFunction emission;
    VolumeScatterSettings scatterSettings;
  };
  
  
  
  inline bool isUnsetTransferFunctionDomain(const range1f &domain)
  {
    return domain.upper < domain.lower;
  }
  
}
