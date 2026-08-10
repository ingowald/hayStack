// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/loader/DataLoader.h"
#include <fstream>

namespace hs {
  namespace loader {
    
    /*! Erik Nielsen Fun3D data dump format */
    struct ENDumpContent : public LoadableContent {
      ENDumpContent(const std::string &fileName,
                    size_t fileSize,
                    int thisPartID,
                    int numPartsToSplitInto)
        : fileName(fileName),
          fileSize(fileSize),
          thisPartID(thisPartID),
          numPartsToSplitInto(numPartsToSplitInto)
      {}
      static void create(DataLoader *loader,
                         const std::string &dataURL)
      {
        assert(dataURL.substr(0,strlen("en-dump://")) == "en-dump://");
        const char *s = dataURL.c_str() + strlen("en-dump://");
        const char *atSign = strstr(s,"@");
        int numPartsToSplitInto = 1;
        if (atSign) {
          numPartsToSplitInto = atoi(s);
          s = atSign + 1;
        }
        const std::string fileName = s;
        size_t fileSize = getFileSize(fileName);
        for (int i=0;i<numPartsToSplitInto;i++)
          loader->addContent(new ENDumpContent(fileName,fileSize,i,numPartsToSplitInto));
      }
      size_t projectedSize() override { return fileSize; }
      void   executeLoad(OnePartition &dataGroup) override;

      std::string toString() override
      {
        return "EN-Nasa-DistData{fileName="+fileName+", part "+std::to_string(thisPartID)+" of "
          + std::to_string(numPartsToSplitInto)+", proj size "
          +prettyNumber(projectedSize())+"B}";
      }
      const std::string fileName;
      const size_t fileSize;
      const int thisPartID = 0;
      const int numPartsToSplitInto = 1;
    };

    inline void ENDumpContent::executeLoad(OnePartition &dg)
    {
      std::ifstream in(fileName.c_str(),std::ios::binary);
      size_t numVertices, numIndices;
      double d;
      in.read((char *)&numVertices,sizeof(size_t));
      in.read((char *)&numIndices,sizeof(size_t));
      std::vector<vec3f> vertices;
      for (int64_t i=0;i<numVertices;i++) {
        vec3f v;
        in.read((char *)&d,sizeof(d));
        v.x = (float)d;
        in.read((char *)&d,sizeof(d));
        v.y = (float)d;
        in.read((char *)&d,sizeof(d));
        v.z = (float)d;
        vertices.push_back(v);
      }

      Mesh::SP mesh = Mesh::create();//std::make_shared<Mesh>();

      for (int64_t i=0;i<numIndices;i++) {
        size_t idx;
        in.read((char *)&idx,sizeof(idx));
        mesh->vertices.push_back(vertices[idx-1]);
        in.read((char *)&idx,sizeof(idx));
        mesh->vertices.push_back(vertices[idx-1]);
        in.read((char *)&idx,sizeof(idx));
        mesh->vertices.push_back(vertices[idx-1]);
        mesh->indices.push_back(vec3i(3*i)+vec3i(0,1,2));
      }

      Scene::SP scene = std::make_shared<Scene>();
      Object::SP model = Object::create({mesh});//std::make_shared<Object>();
      scene->instances.push_back(std::make_shared<Instance>(model));
    
      dg.minis.push_back(scene);
    }  

  }
}
