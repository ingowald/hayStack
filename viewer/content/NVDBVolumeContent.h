// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if HS_USE_MULTI_SCATTERING

#include "viewer/DataLoader.h"
#include "hayStack/NanoVDBVolume.h"

#include <nanovdb/GridHandle.h>
#include <nanovdb/io/IO.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace hs {

  struct NVDBVolumeContent : public LoadableContent {
    NVDBVolumeContent(const std::string &fileName,
                      int thisPartID,
                      const box3i &cellRange,
                      vec3i fullIndexDims)
      : fileName(fileName),
        thisPartID(thisPartID),
        cellRange(cellRange),
        fullIndexDims(fullIndexDims)
    {}

    static void create(DataLoader *loader,
                       const ResourceSpecifier &dataURL)
    {
      std::vector<nanovdb::io::FileGridMetaData> meta;
      try {
        meta = nanovdb::io::readGridMetaData(dataURL.where);
      } catch (const std::exception &e) {
        throw std::runtime_error
          (std::string("NVDBVolumeContent: readGridMetaData failed for '")
           + dataURL.where + "': " + e.what());
      }
      if (meta.empty())
        throw std::runtime_error
          ("NVDBVolumeContent: no grids in '" + dataURL.where + "'");

      box3i initRegion = indexBBoxFromMeta(meta[0].indexBBox);

      std::string extractString = dataURL.get("extract");
      if (!extractString.empty()) {
        vec3i lower, size;
        int n = sscanf(extractString.c_str(), "%i,%i,%i,%i,%i,%i",
                       &lower.x, &lower.y, &lower.z,
                       &size.x, &size.y, &size.z);
        if (n != 6)
          throw std::runtime_error
            ("NVDBVolumeContent: could not parse 'extract' from '"
             + extractString + "' (expected i,i,i,i,i,i)");
        initRegion.lower = lower;
        initRegion.upper = lower + size - 1;
      }

      std::vector<box3i> regions;
      splitKDTree(regions, initRegion, dataURL.numParts);
      if (int(regions.size()) < dataURL.numParts)
        throw std::runtime_error
          ("NVDBVolumeContent: index domain too small to split into "
           + std::to_string(dataURL.numParts) + " parts");

      vec3i fullDims = initRegion.size() + 1;

      if (loader->myRank() == 0) {
        std::cout << "NanoVDB: file " << dataURL.where
                  << " index domain " << initRegion
                  << " will be read in the following bricks:" << std::endl;
        for (int i = 0; i < int(regions.size()); ++i)
          std::cout << " #" << i << " : " << regions[i] << std::endl;
      }

      for (int i = 0; i < dataURL.numParts; ++i) {
        loader->addContent(new NVDBVolumeContent(dataURL.where, i,
                                                 regions[i], fullDims));
      }
    }

    size_t projectedSize() override
    {
      vec3i partCells = cellRange.size() + 1;
      size_t fileSize = getFileSize(fileName);
      size_t fullCells
        = size_t(fullIndexDims.x) * fullIndexDims.y * fullIndexDims.z;
      size_t partCellCount = size_t(partCells.x) * partCells.y * partCells.z;
      if (fullCells == 0)
        return fileSize;
      return fileSize * partCellCount / fullCells;
    }

    void executeLoad(DataRank &dataGroup, bool verbose) override
    {
      nanovdb::GridHandle<> gridHandle;
      try {
        gridHandle = nanovdb::io::readGrid(fileName);
      } catch (const std::exception &e) {
        throw std::runtime_error
          (std::string("NVDBVolumeContent: readGrid failed for '")
           + fileName + "': " + e.what());
      }

      auto vol = std::make_shared<NanoVDBVolume>();
      vol->fileName = fileName;
      vol->partID = thisPartID;
      vol->cellRange = cellRange;
      vol->fullIndexDims = fullIndexDims;
      vol->data.resize(gridHandle.size());
      std::memcpy(vol->data.data(), gridHandle.data(), gridHandle.size());

      vol->bounds = worldBoundsForCellRange(gridHandle, cellRange);
      if (vol->bounds.empty()) {
        auto meta = gridHandle.gridMetaData();
        if (meta) {
          auto wmin = meta->worldBBox().min();
          auto wmax = meta->worldBBox().max();
          vol->bounds.lower = vec3f(float(wmin[0]), float(wmin[1]), float(wmin[2]));
          vol->bounds.upper = vec3f(float(wmax[0]), float(wmax[1]), float(wmax[2]));
        }
      }

      dataGroup.nanovdbVolumes.push_back(vol);

      if (verbose)
        std::cout << "#hs.nvdb: loaded part " << thisPartID << " of "
                  << fileName << " (" << vol->data.size() << " bytes), bounds "
                  << vol->bounds << std::endl;
    }

    std::string toString() override
    {
      std::stringstream ss;
      ss << "NVDBVolumeContent{#" << thisPartID
         << ",fileName=" << fileName
         << ",cellRange=" << cellRange << "}";
      return ss.str();
    }

    const std::string fileName;
    const int thisPartID;
    const vec3i fullIndexDims;
    const box3i cellRange;

  private:
    static int argMaxDim(vec3i size)
    {
      if (size.y > size.x && size.y >= size.z) return 1;
      if (size.z > size.x && size.z > size.y) return 2;
      return 0;
    }

    static void splitKDTree(std::vector<box3i> &regions,
                            box3i cellRange,
                            int numParts)
    {
      if (numParts == 1) {
        regions.push_back(cellRange);
        return;
      }

      vec3i size = cellRange.size();
      int dim = argMaxDim(size);
      if (size[dim] < 2) {
        regions.push_back(cellRange);
        return;
      }

      int nRight = numParts / 2;
      int nLeft = numParts - nRight;

      box3i lBox = cellRange, rBox = cellRange;
      lBox.upper[dim]
        = rBox.lower[dim]
        = cellRange.lower[dim] + (cellRange.size()[dim] * nLeft) / numParts;
      splitKDTree(regions, lBox, nLeft);
      splitKDTree(regions, rBox, nRight);
    }

    static box3i indexBBoxFromMeta(const nanovdb::CoordBBox &ib)
    {
      return {vec3i(ib[0][0], ib[0][1], ib[0][2]),
              vec3i(ib[1][0], ib[1][1], ib[1][2])};
    }

    static box3f worldBoundsForCellRange(const nanovdb::GridHandle<> &handle,
                                         const box3i &range)
    {
      auto meta = handle.gridMetaData();
      if (!meta)
        return box3f{};

      nanovdb::Coord lo(range.lower.x, range.lower.y, range.lower.z);
      nanovdb::Coord hi(range.upper.x, range.upper.y, range.upper.z);
      nanovdb::CoordBBox cb(lo, hi);
      auto wb = cb.transform(meta->map());
      auto wmin = wb.min();
      auto wmax = wb.max();
      return box3f{vec3f(float(wmin[0]), float(wmin[1]), float(wmin[2])),
                   vec3f(float(wmax[0]), float(wmax[1]), float(wmax[2]))};
    }
  };

}

#endif
