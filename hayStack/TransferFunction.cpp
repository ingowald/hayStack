// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/TransferFunction.h"
#include <fstream>

namespace hs {

  const size_t xfSingleFileFormatMagic = 0x1235abc000;
  const size_t xfSetFileFormatMagic = 0x1235abc101;
  
  void TransferFunction::write(std::ostream &out) const
  {
    out.write((const char *)&this->baseDensity, sizeof(this->baseDensity));
    out.write((const char *)&this->domain, sizeof(this->domain));

    // old transfer function editor expected 'relative domain' as
    // awell as absolute one; ignore this
    range1f relDomain(0.f,100.f);
    out.write((const char *)&relDomain, sizeof(relDomain));
    
    const int count = int(this->colorMap.size());
    out.write((const char *)&count, sizeof(count));
    out.write((const char *)this->colorMap.data(),
              this->colorMap.size() * sizeof(this->colorMap[0]));
  }

  float lerp(range1f r, float f)
  {
    return r.lower + (r.upper-r.lower)*f;
  }

  void TransferFunction::read(std::istream &in)
  {
    in.read((char *)&this->baseDensity, sizeof(this->baseDensity));

    // old transfer function editor expected 'relative domain' as
    // well as 'absolute domain'; we read both and just merge them into one.
    range1f absDomain, relDomain;
    in.read((char *)&absDomain, sizeof(absDomain));
    in.read((char *)&relDomain, sizeof(relDomain));

    this->domain.lower = lerp(absDomain,relDomain.lower/100.f);
    this->domain.upper = lerp(absDomain,relDomain.upper/100.f);
    
    int count = 0;
    in.read((char *)&count, sizeof(count));
    if (!in || count < 1 || count > (1 << 24))
      throw std::runtime_error("invalid or truncated transfer function");
    this->colorMap.resize(count);
    in.read((char *)this->colorMap.data(),
            this->colorMap.size() * sizeof(this->colorMap[0]));
    if (!in)
      throw std::runtime_error("truncated transfer function");
  }

  
  void VolumeScatterSettings::write(std::ostream &out) const
  {
    const uint32_t enabled = this->enabled ? 1u : 0u;
    out.write((const char *)&enabled, sizeof(enabled));
    out.write((const char *)&this->maxVolumeBounces, sizeof(this->maxVolumeBounces));
    const auto &m = this->medium;
    out.write((const char *)&m.anisotropy, sizeof(m.anisotropy));
    out.write((const char *)&m.emissionStrength, sizeof(m.emissionStrength));
  }

  void VolumeScatterSettings::read(std::istream &in)
  {
    uint32_t enabled = 0;
    in.read((char *)&enabled, sizeof(enabled));
    in.read((char *)&this->maxVolumeBounces, sizeof(this->maxVolumeBounces));
    auto &m = this->medium;
    in.read((char *)&m.anisotropy, sizeof(m.anisotropy));
    in.read((char *)&m.emissionStrength, sizeof(m.emissionStrength));
    if (!in)
      throw std::runtime_error("truncated scatter settings file");
    this->enabled = enabled != 0;
  }

  // void TransferFunction::save(const std::string &fileName) const
  // {
  //   std::ofstream out(fileName,std::ios::binary);
  //   out.write((char*)&xfSingleFileFormatMagic,sizeof(xfSingleFileFormatMagic));

  //   write(out);
  //   std::cout << "#hs.Transferfunction: saved xf to "
  //             <<  fileName << std::endl;
  // }  

  range1f TransferFunction::getDomain() const
  {
    return domain;
  }

//   void TransferFunction::load(const std::string &fileName)
//   {
// #if HS_USE_MULTI_SCATTERING
//     *this = loadVolumeTransferFunctions(fileName).opacity;
// #else
//     std::cout << "#hs: loading transfer function " << fileName << std::endl;
//     std::ifstream in(fileName.c_str(),std::ios::binary);
//     size_t magic;
//     in.read((char*)&magic,sizeof(xfFileFormatMagic));

//     in.read((char*)&baseDensity,sizeof(baseDensity));
//     in.read((char*)&absDomain,sizeof(absDomain));
//     in.read((char*)&relDomain,sizeof(relDomain));

//     int numColorMapValues;
//     in.read((char*)&numColorMapValues,sizeof(numColorMapValues));
//     colorMap.resize(numColorMapValues);
//     in.read((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
// #endif
//   }

  void VolumeTransferFunctionSet::save(const std::string &fileName) const
  {
    std::ofstream out(fileName, std::ios::binary);
    if (!out)
      throw std::runtime_error("could not open file: " + fileName);
    out.write((const char *)&xfSetFileFormatMagic, sizeof(xfSetFileFormatMagic));
    opacity.write(out);
    emission.write(out);
    scatterSettings.write(out);
    if (!out)
      throw std::runtime_error("failed writing transfer function: " + fileName);
    std::cout << "#hs.Transferfunction: saved xf to "
              << fileName << std::endl;
  }

  VolumeTransferFunctionSet
  VolumeTransferFunctionSet::load(const std::string &fileName)
  {
    std::cout << "#hs: loading transfer function " << fileName << std::endl;
    std::ifstream in(fileName, std::ios::binary);
    if (!in)
      throw std::runtime_error("could not read transfer function: " + fileName);
    size_t magic = 0;
    in.read((char *)&magic, sizeof(magic));
    VolumeTransferFunctionSet set;
    try {
      if (magic == xfSingleFileFormatMagic) {
        set.opacity.read(in);
        return set;
      }
      if (magic == xfSetFileFormatMagic) {
        set.opacity.read(in);
        set.emission.read(in);
        set.scatterSettings.read(in);
        return set;
      }
      throw std::runtime_error("not a valid .xf file");
    } catch (std::exception &e) {
      throw std::runtime_error("error trying to load transfer function from '"
                               +fileName+"' : "+e.what());
    }
  }

}
