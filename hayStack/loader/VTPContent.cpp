// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/VTPContent.h"
#include "hayStack/TriangleMesh.h"

#include <cstring>
#include <fstream>
#include <limits>

namespace hs {
  namespace loader {
    namespace {

      std::string attribute(const std::string &tag, const std::string &name)
      {
        const std::string prefix = name + "=\"";
        const size_t begin = tag.find(prefix);
        if (begin == std::string::npos)
          throw std::runtime_error("VTP: missing '" + name + "' attribute");
        const size_t valueBegin = begin + prefix.size();
        const size_t end = tag.find('"', valueBegin);
        if (end == std::string::npos)
          throw std::runtime_error("VTP: malformed '" + name + "' attribute");
        return tag.substr(valueBegin, end - valueBegin);
      }

      std::string findArrayTag(const std::string &xml,
                               const std::string &section,
                               const std::string &name = "")
      {
        const size_t sectionBegin = xml.find("<" + section);
        const size_t sectionEnd = xml.find("</" + section + ">", sectionBegin);
        if (sectionBegin == std::string::npos || sectionEnd == std::string::npos)
          throw std::runtime_error("VTP: missing <" + section + "> section");

        size_t pos = sectionBegin;
        while ((pos = xml.find("<DataArray", pos)) < sectionEnd) {
          const size_t end = xml.find("/>", pos);
          if (end == std::string::npos || end > sectionEnd)
            throw std::runtime_error("VTP: malformed DataArray in " + section);
          const std::string tag = xml.substr(pos, end - pos + 2);
          if (name.empty() ||
              tag.find("Name=\"" + name + "\"") != std::string::npos)
            return tag;
          pos = end + 2;
        }
        throw std::runtime_error("VTP: missing DataArray '" + name
                                 + "' in " + section);
      }

      template<typename T>
      T readValue(const std::vector<char> &file, size_t offset)
      {
        if (offset + sizeof(T) > file.size())
          throw std::runtime_error("VTP: appended data extends past end of file");
        T value;
        std::memcpy(&value, file.data() + offset, sizeof(T));
        return value;
      }

      template<typename T>
      std::vector<T> readArray(const std::vector<char> &file,
                               size_t appendedBegin,
                               const std::string &tag,
                               const std::string &expectedType)
      {
        if (attribute(tag, "format") != "appended")
          throw std::runtime_error("VTP: only appended arrays are supported");
        if (attribute(tag, "type") != expectedType)
          throw std::runtime_error("VTP: expected " + expectedType + " array");

        const size_t block = appendedBegin
          + std::stoull(attribute(tag, "offset"));
        const uint64_t numBytes = readValue<uint64_t>(file, block);
        if (numBytes % sizeof(T) != 0 ||
            block + sizeof(uint64_t) + numBytes > file.size())
          throw std::runtime_error("VTP: invalid appended array size");

        std::vector<T> values(numBytes / sizeof(T));
        std::memcpy(values.data(), file.data() + block + sizeof(uint64_t),
                    numBytes);
        return values;
      }

    }

    VTPContent::VTPContent(const std::string &fileName)
      : fileName(fileName), fileSize(getFileSize(fileName))
    {}

    void VTPContent::create(DataLoader *loader, const std::string &fileName)
    {
      loader->addContent(new VTPContent(fileName));
    }

    std::string VTPContent::toString()
    {
      return "VTP{fileName=" + fileName + ", proj size "
        + prettyNumber(projectedSize()) + "B}";
    }

    size_t VTPContent::projectedSize()
    {
      return 2 * fileSize;
    }

    void VTPContent::executeLoad(OnePartition &dataGroup)
    {
      std::ifstream input(fileName, std::ios::binary);
      if (!input)
        throw std::runtime_error("VTP: could not open " + fileName);
      std::vector<char> file((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());

      const std::string appendedTag = "<AppendedData";
      const auto tagPos = std::search(file.begin(), file.end(),
                                      appendedTag.begin(), appendedTag.end());
      if (tagPos == file.end())
        throw std::runtime_error("VTP: missing AppendedData section");
      const size_t tagOffset = tagPos - file.begin();
      const auto underscore = std::find(file.begin() + tagOffset,
                                        file.end(), '_');
      if (underscore == file.end())
        throw std::runtime_error("VTP: missing appended-data marker");
      const size_t appendedBegin = underscore - file.begin() + 1;
      const std::string xml(file.data(), tagOffset);

      if (xml.find("byte_order=\"LittleEndian\"") == std::string::npos ||
          xml.find("header_type=\"UInt64\"") == std::string::npos ||
          xml.find("compressor=") != std::string::npos)
        throw std::runtime_error(
          "VTP: only uncompressed little-endian UInt64-header files are supported");

      const auto points = readArray<float>(
        file, appendedBegin, findArrayTag(xml, "Points"), "Float32");
      const auto connectivity = readArray<int64_t>(
        file, appendedBegin, findArrayTag(xml, "Polys", "connectivity"), "Int64");
      const auto offsets = readArray<int64_t>(
        file, appendedBegin, findArrayTag(xml, "Polys", "offsets"), "Int64");

      if (points.size() % 3 != 0)
        throw std::runtime_error("VTP: point array is not made of xyz triples");

      TriangleMesh::SP mesh = TriangleMesh::create();
      mesh->vertices.resize(points.size() / 3);
      for (size_t i = 0; i < mesh->vertices.size(); ++i)
        mesh->vertices[i] = vec3f(points[3*i], points[3*i+1], points[3*i+2]);

      int64_t begin = 0;
      for (const int64_t end : offsets) {
        if (begin < 0 || end < begin ||
            end > static_cast<int64_t>(connectivity.size()))
          throw std::runtime_error("VTP: invalid polygon offsets");
        if (end - begin >= 3) {
          const int64_t first = connectivity[begin];
          for (int64_t i = begin + 1; i + 1 < end; ++i) {
            const int64_t b = connectivity[i];
            const int64_t c = connectivity[i+1];
            const int64_t vertexCount = mesh->vertices.size();
            if (first < 0 || b < 0 || c < 0 ||
                first >= vertexCount || b >= vertexCount || c >= vertexCount ||
                first > std::numeric_limits<int>::max() ||
                b > std::numeric_limits<int>::max() ||
                c > std::numeric_limits<int>::max())
              throw std::runtime_error("VTP: polygon vertex index out of range");
            mesh->indices.push_back(vec3i((int)first, (int)b, (int)c));
          }
        }
        begin = end;
      }

      // Use Pressure as the default color-mapped point field when present.
      try {
        mesh->scalars.perVertex = readArray<float>(
          file, appendedBegin, findArrayTag(xml, "PointData", "Pressure"),
          "Float32");
        if (mesh->scalars.perVertex.size() != mesh->vertices.size())
          mesh->scalars.perVertex.clear();
      } catch (const std::runtime_error &) {
        // Geometry remains renderable when the optional field is absent.
      }

      mini::Matte::SP material = std::make_shared<mini::Matte>();
      material->reflectance = M_PI * .6f;
      mesh->material = material;
      dataGroup.triangleMeshes.push_back(mesh);

      std::cout << "#hs.vtp: loaded " << mesh->vertices.size()
                << " points and " << mesh->indices.size()
                << " triangles from " << fileName << std::endl;
    }

  }
}
