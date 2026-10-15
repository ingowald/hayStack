// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/ColorMapper.h"
#include "hayStack/ColorMap.h"

namespace hs {
  namespace loader {
    
    int ColorMappingMetaContent::currentIdx = 0;
    vec2f ColorMappingMetaContent::currentDomain = {0.f,0.f};


    inline float saturate(float f) { return min(1.f,max(0.f,f)); }
  
    inline vec3f hue_to_rgb(float hue)
    {
      float s = saturate( hue ) * 6.0f;
      float r = saturate( fabsf(s - 3.f) - 1.0f );
      float g = saturate( 2.0f - fabsf(s - 2.0f) );
      float b = saturate( 2.0f - fabsf(s - 4.0f) );
      return vec3f(r, g, b); 
    }
    
    inline vec3f temperature_to_rgb(float t)
    {
      float K = 4.0f / 6.0f;
      float h = K - K * t;
      float v = .5f + 0.5f * t;    return v * hue_to_rgb(h);
    }

    inline vec4f mapFloatToTable(float f, const std::vector<vec4f> &xf)
    {
      int N = xf.size();
      assert(!xf.empty());
      f = saturate(f);
      if (N == 1) return xf[0];

      float Nf = f * (N-1);
      int i0 = std::min(N-2,int(Nf));
      float ff = Nf - i0;
      vec4f v = (1.f-ff)*xf[i0] + ff*xf[i0+1];
      return v;
    }
    

    ColorMapper::ColorMapper(const ResourceSpecifier &data)
    {
      cmIndex = data.get_int("map-index",ColorMappingMetaContent::currentIdx);
      vec2f colorMapRange = data.get("map",vec2f(+1.f,-1.f));
      inputRange = {colorMapRange.x,colorMapRange.y};
      std::string mapColorsFile = data.get("map-colors","");
      if (!mapColorsFile.empty()) {
        FILE *file = fopen(mapColorsFile.c_str(),"r");
        if (!file) throw std::runtime_error
                     ("could not open color mapping file "+mapColorsFile);
        vec4f v;
        char line[10000];
        while (!feof(file) && fgets(line,10000,file)) {
          if (sscanf(line,"%f %f %f %f",&v.x,&v.y,&v.z,&v.w) == 4)
            mapColors.push_back(v);
          else
            throw std::runtime_error("cannot parse map-colors line '"
                                     +std::string(line)+"'");
        }
        fclose(file);
      }
      
      if (mapColors.empty() && cmIndex >= 0)
        mapColors = ColorMap::get(cmIndex);
    }
    
    bool ColorMapper::valid() const
    { return inputRange.lower <= inputRange.upper; }

    vec3f ColorMapper::map3f(float f)
    { vec4f v = map4f(f); return (const vec3f&)v; }
      
    vec4f ColorMapper::map4f(float f)
    {
      assert(isValid());
      f = saturate((f-inputRange.lower)/(inputRange.upper-inputRange.lower));
      vec4f color;
      if (mapColors.empty()) {
        (vec3f&)color = .6f*temperature_to_rgb(f);
        color.w = 1.f;
      } else {
        color = mapFloatToTable(f,mapColors);
      }
      return color;
    }
        
  }
      
}
