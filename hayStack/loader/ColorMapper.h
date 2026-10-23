// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

/*! @{ \file loader/ColorMapping.h - helper classes and function for
    on-the-fly color mapping during loading. allows the same
    functionality to be used in both triangles and spheres (and
    whereever else) */

#include "hayStack/loader/DataLoader.h"

namespace hs {
  namespace loader {

    /*! helper class for color mapping. this class will parse a
        dataURL's 'map', map-xf, and 'map-file' parameters (if
        present), and then allow to simply call 'map(float)" to get a
        color. if no float2 'map' parameter is set this will return
        valid()==false, and mapping should not be used. if a
        'map-file' is set it shold point to a text file with four
        floats (r g b a) per line.
     */
    struct ColorMapper {
      /*! construct itself from the dataURL, parsing the mapping
          related variables in there; will set to 'invalid' if none
          such found */
      ColorMapper(const ResourceSpecifier &dataURL);
      bool valid() const;

      vec3f map3f(float f);
      vec4f map4f(float f);
      
      /*! the expected input range for the color mapping; and value
          outside this range will be clamped to this range */
      range1f inputRange;
      /*! the from-file map of colors to map to; if empty we'll use
          mapping by temperature_to_rgb() */
      std::vector<vec4f> mapColors;
      /*! maps to one of the pre-builtin color maps (paraview etc);
          this will get used if no 'map-file' was specified. if set to
          -1, we'll use temperature_to_rgb mapping */
      int cmIndex;
    };

    struct ColorMappingMetaContent : public LoadableContent {
      static int   default_cmIndex;
      static vec2f default_domain;
      
      static void create(DataLoader *loader,
                         const ResourceSpecifier &dataURL)
      {
        default_cmIndex = dataURL.get_int("cmidx",default_cmIndex);
        default_domain  = dataURL.get_vec2f("map",default_domain);
      }
      void   executeLoad(OnePartition &dataGroup) { /* nothing */ }
    
      std::string toString() override;
    };
    
    
  }
}
