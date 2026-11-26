// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/ResourceSpecifier.h"

namespace hs {
  namespace loader {

    ResourceSpecifier::ResourceSpecifier(std::string resource)
    {
      int pos = resource.find("://");
      if (pos == resource.npos)
        throw std::runtime_error
          ("could not parse resource specifier '"+resource
           +"' - couldn't find '://' in there!?");
      type = resource.substr(0,pos);
      resource = resource.substr(pos+3);

      int colon = resource.find(":");
      if (colon == resource.npos) {
        where = resource;
      } else {
        where = resource.substr(0,colon);
        std::string args = resource.substr(colon+1);
        std::vector<std::string> betweenColons;
        while (true) {
          colon = args.find(":");
          if (colon == args.npos) {
            betweenColons.push_back(args);
            break;
          } else {
            betweenColons.push_back(args.substr(0,colon));
            args = args.substr(colon+1);
          }
        }
        for (auto arg : betweenColons) {
          std::string key=args, value="";
          int equals = arg.find("=");
          if (equals != key.npos) {
            key   = arg.substr(0,equals);
            value = arg.substr(equals+1);
          }
          keyValuePairs[key] = value;
        }
      }

      pos = where.find("@");
      if (pos != where.npos) {
        numParts = stoi(where.substr(0,pos));
        where = where.substr(pos+1);
      }

      if (where[0] == '~')
        where = std::string(std::getenv("HOME"))+where.substr(1);
      
#ifdef _WIN32
      // the name of drive
      pos = where.find("$"); // Find the position of "$"
      if (pos != std::string::npos) { // npos is a constant representing not found
        where.replace(pos, 1, ":"); // Replace 1 character at pos with ":"
      }
#endif
    }
  
    bool ResourceSpecifier::has(const std::string &key) const
    {
      return keyValuePairs.find(key) != keyValuePairs.end();
    }
  
    std::string ResourceSpecifier::get(const std::string &key,
                                       const std::string &defaultValue)
      const
    {
      if (!has(key)) return defaultValue;
      return keyValuePairs.find(key)->second;
    }
  
    vec3f ResourceSpecifier::get_vec3f(const std::string &key,
                                       vec3f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec3f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f,%f",&v.x,&v.y,&v.z);
      else
        n = sscanf(value.c_str(),"%f %f %f",&v.x,&v.y,&v.z);
      if (n != 3)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    vec4f ResourceSpecifier::get_vec4f(const std::string &key,
                                       vec4f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec4f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f,%f,%f",&v.x,&v.y,&v.z,&v.w);
      else
        n = sscanf(value.c_str(),"%f %f %f %f",&v.x,&v.y,&v.z,&v.w);
      if (n != 4)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    vec2f ResourceSpecifier::get_vec2f(const std::string &key,
                                       vec2f defaultValue) const
    {
      if (!has(key)) return defaultValue;
      vec2f v;
      std::string value = keyValuePairs.find(key)->second;
      int n = 0;
      if (strstr(value.c_str(),","))
        n = sscanf(value.c_str(),"%f,%f",&v.x,&v.y);
      else
        n = sscanf(value.c_str(),"%f %f",&v.x,&v.y);
      if (n != 2)
        throw std::runtime_error
          ("could not parse '"+value+"' for key '"+key+"'");
      return v;
    }
  
    int   ResourceSpecifier::get_int(const std::string &key,
                                     int defaultValue) const
    {
      if (!has(key)) return defaultValue;
      std::string value = keyValuePairs.find(key)->second;
      return std::stoi(value);
    }
  
    size_t ResourceSpecifier::get_size(const std::string &key,
                                       size_t defaultValue) const
    {
      if (!has(key)) return defaultValue;

      std::string value = keyValuePairs.find(key)->second;
    
      char magnitude = ' ';
      size_t result;
      sscanf(value.c_str(),"%li%c",&result,&magnitude);
      if (magnitude == ' ')
        ;
      else if (magnitude == 'M')
        result *= 1000000ull;
      else if (magnitude == 'K')
        result *= 1000ull;
      else if (magnitude == 'G')
        result *= 1000000000ull;
      else
        throw std::runtime_error
          ("invalid magnitude specifier '"+std::to_string(magnitude)
           +"' (was expecting 'K', 'M', etc)");
    
      return result;
    }
  
    float ResourceSpecifier::get_float(const std::string &key,
                                       float defaultValue) const
    {
      if (!has(key)) return defaultValue;
      std::string value = keyValuePairs.find(key)->second;
      return std::stof(value);
    }

  }
}
