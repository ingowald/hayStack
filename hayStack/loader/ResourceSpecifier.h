// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/common.h"
#include <map>
#include <string>

namespace hs {
  namespace loader {

    /*! parses a data URL of the form
        <type>://[<numParts>@]<where>[:key[=value]]*
        into its type, location, number of parts, and key-value
        arguments */
    struct ResourceSpecifier {
      ResourceSpecifier(std::string s);
      bool has(const std::string &key) const;
      std::string get(const std::string &key, const std::string &defaultValue="") const;
    
      vec3f  get(const std::string &key, vec3f defaultValue) const
      { return get_vec3f(key,defaultValue); }
      vec2f  get(const std::string &key, vec2f defaultValue) const
      { return get_vec2f(key,defaultValue); }
      vec2f  get_vec2f(const std::string &key, vec2f defaultValue) const;
      vec3f  get_vec3f(const std::string &key, vec3f defaultValue) const;
      vec4f  get_vec4f(const std::string &key, vec4f defaultValue) const;
      size_t get_size(const std::string &key, size_t defaultValue) const;
      int    get_int(const std::string &key, int defaultValue) const;
      float  get_float(const std::string &key, float defaultValue) const;
      std::string where;
      std::string type;
      int numParts = 1;
      std::map<std::string,std::string> keyValuePairs;
    };

  }
}
