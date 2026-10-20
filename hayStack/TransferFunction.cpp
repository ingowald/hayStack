// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/TransferFunction.h"
#include <fstream>

namespace hs {

#if HS_USE_MULTI_SCATTERING
  namespace {
    const size_t xfFileFormatMagic = 0x1235abc100;

    void writeTransferFunction(std::ostream &out, const TransferFunction &xf)
    {
      out.write((const char *)&xf.baseDensity, sizeof(xf.baseDensity));
      out.write((const char *)&xf.absDomain, sizeof(xf.absDomain));
      out.write((const char *)&xf.relDomain, sizeof(xf.relDomain));
      const int count = int(xf.colorMap.size());
      out.write((const char *)&count, sizeof(count));
      out.write((const char *)xf.colorMap.data(),
                xf.colorMap.size() * sizeof(xf.colorMap[0]));
    }

    void readTransferFunction(std::istream &in, TransferFunction &xf,
                              const std::string &fileName)
    {
      in.read((char *)&xf.baseDensity, sizeof(xf.baseDensity));
      in.read((char *)&xf.absDomain, sizeof(xf.absDomain));
      in.read((char *)&xf.relDomain, sizeof(xf.relDomain));
      int count = 0;
      in.read((char *)&count, sizeof(count));
      if (!in || count < 1 || count > (1 << 24))
        throw std::runtime_error(fileName + ": invalid or truncated transfer function");
      xf.colorMap.resize(count);
      in.read((char *)xf.colorMap.data(),
              xf.colorMap.size() * sizeof(xf.colorMap[0]));
      if (!in)
        throw std::runtime_error(fileName + ": truncated transfer function");
    }

    void writeScatter(std::ostream &out, const VolumeScatterSettings &s)
    {
      const uint32_t enabled = s.enabled ? 1u : 0u;
      out.write((const char *)&enabled, sizeof(enabled));
      out.write((const char *)&s.maxVolumeBounces, sizeof(s.maxVolumeBounces));
      const auto &m = s.medium;
      out.write((const char *)&m.anisotropy, sizeof(m.anisotropy));
      out.write((const char *)&m.emissionStrength, sizeof(m.emissionStrength));
    }

    void readScatter(std::istream &in, VolumeScatterSettings &s,
                     const std::string &fileName)
    {
      uint32_t enabled = 0;
      in.read((char *)&enabled, sizeof(enabled));
      in.read((char *)&s.maxVolumeBounces, sizeof(s.maxVolumeBounces));
      auto &m = s.medium;
      in.read((char *)&m.anisotropy, sizeof(m.anisotropy));
      in.read((char *)&m.emissionStrength, sizeof(m.emissionStrength));
      if (!in)
        throw std::runtime_error(fileName + ": truncated scatter settings");
      s.enabled = enabled != 0;
    }
  }
#else
  const size_t xfFileFormatMagic = 0x1235abc000;
#endif

  void TransferFunction::save(const std::string &fileName) const
  {
#if HS_USE_MULTI_SCATTERING
    VolumeTransferFunctionSet set;
    set.opacity = *this;
    saveVolumeTransferFunctions(fileName, set);
#else
    std::ofstream out(fileName,std::ios::binary);
    out.write((char*)&xfFileFormatMagic,sizeof(xfFileFormatMagic));

    out.write((char*)&baseDensity,sizeof(baseDensity));
    out.write((char*)&absDomain,sizeof(absDomain));
    out.write((char*)&relDomain,sizeof(relDomain));

    int numColorMapValues = colorMap.size();
    out.write((char*)&numColorMapValues,sizeof(numColorMapValues));
    out.write((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
    std::cout << "#hs.Transferfunction: saved xf to "
              <<  fileName << std::endl;
#endif
  }  

  range1f TransferFunction::getDomain() const
  {
    float absDomainSize = absDomain.upper - absDomain.lower;
    range1f domain;
    domain.lower = absDomain.lower + (relDomain.lower/100.f) * absDomainSize;
    domain.upper = absDomain.lower + (relDomain.upper/100.f) * absDomainSize;
    return domain;
  }

  void TransferFunction::load(const std::string &fileName)
  {
#if HS_USE_MULTI_SCATTERING
    *this = loadVolumeTransferFunctions(fileName).opacity;
#else
    std::cout << "#hs: loading transfer function " << fileName << std::endl;
    std::ifstream in(fileName.c_str(),std::ios::binary);
    size_t magic;
    in.read((char*)&magic,sizeof(xfFileFormatMagic));

    in.read((char*)&baseDensity,sizeof(baseDensity));
    in.read((char*)&absDomain,sizeof(absDomain));
    in.read((char*)&relDomain,sizeof(relDomain));

    int numColorMapValues;
    in.read((char*)&numColorMapValues,sizeof(numColorMapValues));
    colorMap.resize(numColorMapValues);
    in.read((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));
#endif
  }

#if HS_USE_MULTI_SCATTERING
  void saveVolumeTransferFunctions(const std::string &fileName,
                                   const VolumeTransferFunctionSet &set)
  {
    std::ofstream out(fileName, std::ios::binary);
    if (!out)
      throw std::runtime_error("could not write transfer function: " + fileName);
    out.write((const char *)&xfFileFormatMagic, sizeof(xfFileFormatMagic));
    writeTransferFunction(out, set.opacity);
    writeTransferFunction(out, set.emission);
    writeScatter(out, set.scatterSettings);
    if (!out)
      throw std::runtime_error("failed writing transfer function: " + fileName);
    std::cout << "#hs.Transferfunction: saved xf to "
              << fileName << std::endl;
  }

  VolumeTransferFunctionSet
  loadVolumeTransferFunctions(const std::string &fileName)
  {
    std::cout << "#hs: loading transfer function " << fileName << std::endl;
    std::ifstream in(fileName, std::ios::binary);
    if (!in)
      throw std::runtime_error("could not read transfer function: " + fileName);
    size_t magic = 0;
    in.read((char *)&magic, sizeof(magic));
    if (magic != xfFileFormatMagic)
      throw std::runtime_error(fileName + ": not a valid .xf file");

    VolumeTransferFunctionSet set;
    readTransferFunction(in, set.opacity, fileName);
    readTransferFunction(in, set.emission, fileName);
    readScatter(in, set.scatterSettings, fileName);
    return set;
  }
#endif

}
