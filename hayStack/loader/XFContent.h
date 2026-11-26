// SPDX-FileCopyrightText: Copyright (c) 2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/LoadableContent.h"

namespace hs {
  namespace loader {

    /*! a transfer function (.xf) file. Registers the loaded transfer
        function set under the name given by the data URL's 'name'
        key (default "xf0"); volumes refer to that name via their own
        'xf' key; e.g., 'xf://foo.xf:name=myXF' together with
        'raw://...:xf=myXF' */
    struct XFContent : public Rank0Content {
      XFContent(const ResourceSpecifier &dataURL);

      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL);

      std::string toString() override;
      void   executeLoad(Rank0Data &rank0) override;

      const ResourceSpecifier dataURL;
      const std::string fileName;
      /*! name this transfer function gets registered under */
      const std::string name;
    };

  }
}
