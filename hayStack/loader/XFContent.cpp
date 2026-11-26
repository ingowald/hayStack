// SPDX-FileCopyrightText: Copyright (c) 2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/XFContent.h"
#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    XFContent::XFContent(const ResourceSpecifier &dataURL)
      : dataURL(dataURL),
        fileName(dataURL.where),
        name(dataURL.get("name","xf0"))
    {}

    void XFContent::create(DataLoader *loader,
                           const ResourceSpecifier &dataURL)
    {
      loader->addContent(new XFContent(dataURL));
    }

    std::string XFContent::toString()
    {
      return "XF{fileName="+fileName+", name="+name+"}";
    }

    void XFContent::executeLoad(Rank0Data &rank0)
    {
      rank0.transferFunctions[name]
        = std::make_shared<VolumeTransferFunctionSet>
        (VolumeTransferFunctionSet::load(fileName));
    }

  }
}
