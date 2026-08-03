// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#if 0
# include "cuBQL/math/box.h"
# include "cuBQL/math/linear.h"
# include "cuBQL/math/affine.h"
#else
# include "miniScene/Scene.h"
#endif

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <iostream>

namespace dgef {
  using namespace mini;
  // using namespace cuBQL;
  
  struct Mesh {
    typedef std::shared_ptr<Mesh> SP;

    void write(std::ostream &out);
    static Mesh::SP read(std::istream &in);
    
    std::vector<vec3d>  vertices;
    std::vector<vec3ul> indices;
  };

  struct Instance {
    affine3d transform;
    int      meshID;
  };
  
  struct Model {
    typedef std::shared_ptr<Model> SP;

    static Model::SP read(const std::string &fileName);
    void write(const std::string &fileName);
    
    std::vector<Mesh::SP> meshes;
    std::vector<Instance> instances;
  };

  inline void Mesh::write(std::ostream &out)
  {
    size_t numVertices = vertices.size();
    out.write((char *)&numVertices,sizeof(numVertices));
    out.write((char *)vertices.data(),numVertices*sizeof(vertices[0]));

    size_t numIndices = indices.size();
    out.write((char *)&numIndices,sizeof(numIndices));
    out.write((char *)indices.data(),numIndices*sizeof(indices[0]));
  }
  
  inline void Model::write(const std::string &fileName)
  {
    std::ofstream out(fileName.c_str(),std::ios::binary);
    
    size_t magic = 0x33234567755ull;
    out.write((char *)&magic,sizeof(magic));
    size_t numMeshes = meshes.size();
    out.write((char *)&numMeshes,sizeof(numMeshes));
    for (auto mesh : meshes)
      mesh->write(out);
    size_t numInstances = instances.size();
    out.write((char *)&numInstances,sizeof(numInstances));
    out.write((char *)instances.data(),numInstances*sizeof(instances[0]));
  }

  inline Mesh::SP Mesh::read(std::istream &in)
  {
    Mesh::SP mesh = std::make_shared<Mesh>();
    
    size_t numVertices = 0;
    in.read((char *)&numVertices,sizeof(numVertices));
    mesh->vertices.resize(numVertices);
    in.read((char *)mesh->vertices.data(),numVertices*sizeof(mesh->vertices[0]));

    size_t numIndices = 0;
    in.read((char *)&numIndices,sizeof(numIndices));
    mesh->indices.resize(numIndices);
    in.read((char *)mesh->indices.data(),numIndices*sizeof(mesh->indices[0]));

    return mesh;
  }
  
  inline Model::SP Model::read(const std::string &fileName)
  {
    std::ifstream in(fileName.c_str(),std::ios::binary);
    Model::SP model = std::make_shared<Model>();
    
    size_t expected_magic = 0x33234567755ull;
    size_t magic = 0x33234567755ull;
    in.read((char *)&magic,sizeof(magic));
    assert(magic == expected_magic);
    
    size_t numMeshes = 0;
    in.read((char *)&numMeshes,sizeof(numMeshes));
    for (int meshID=0;meshID<numMeshes;meshID++) 
      model->meshes.push_back(Mesh::read(in));
    size_t numInstances = 0;
    in.read((char *)&numInstances,sizeof(numInstances));
    model->instances.resize(numInstances);
    in.read((char *)model->instances.data(),
            numInstances*sizeof(model->instances[0]));
    return model;
  }
  
}

