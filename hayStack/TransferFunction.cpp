// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/TransferFunction.h"
#include <fstream>

namespace hs {

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

  void TransferFunction::save(const std::string &fileName) const
  {
    VolumeTransferFunctionSet set;
    set.opacity = *this;
    saveVolumeTransferFunctions(fileName, set);
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
    *this = loadVolumeTransferFunctions(fileName).opacity;
  }

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

}
