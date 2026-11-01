// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"

namespace hs {
  
  /*! convert PNG rgb image into a color map; if PNG doesn't have an
    alpha channel, alpha channel will be set to constant 1.f */
  struct ColorMap : public std::vector<vec4f>
  {
    typedef std::shared_ptr<ColorMap> SP;
    
    static ColorMap fromPNG(const uint8_t asPNG[],
                            size_t numBytes);
  
    /*! returns re-sampled version of this same map, with numSamples
      samples */
    ColorMap resampledTo(int numSamples) const;
  };

  
  /*! class that abstracts a library of different, predefined color
    maps that are embedded in this library */
  struct ColorMapLibrary
  {
    static void init();
    // ColorMapLibrary();

    /*! adds a color map from a png file to the library */
    static void addColorMap(const ColorMap &cm,
                            const std::string &name);
    
    /*! returns a std::vector with all the names of color maps known
      in this library */
    static std::vector<std::string> getNames();

    /*! find color map with given name, and reutrn pointer to it; or
      null if this does not exist */
    static const ColorMap *find(const std::string &name);

    /*! return map with given index */
    static const ColorMap &getMap(const int mapID);

    /*! return _name_ of map with given index */
    static const std::string getMapName(const int mapID);

  private:
    static ColorMapLibrary singleton;
    /*! list of color maps stored in this library */
    std::vector<std::pair<std::string,ColorMap::SP>> knownMaps;
  };


  
  struct ScalarMapper : public ColorMap {
    range1f domain { 0.f, 1.f };
  };
  
  
}
