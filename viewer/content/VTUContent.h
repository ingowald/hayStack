#pragma once

#include "viewer/DataLoader.h"

namespace hs {

  /*! a VTK .vtu file of unstructured grid data, including polyhedral cells */
  struct VTUContent : public LoadableContent {
    VTUContent(const std::string &fileName);
    static void create(DataLoader *loader,
                       const std::string &dataURL);
    std::string toString() override;
    size_t projectedSize() override;
    void   executeLoad(DataRank &dataGroup, bool verbose) override;

    const std::string fileName;
    const size_t      fileSize;
  };

}
