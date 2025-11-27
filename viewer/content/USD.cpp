// Copyright 2024-2025 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

// #include "tsd/core/ColorMapUtil.hpp"
// #include "tsd/core/Logging.hpp"
// #include "tsd/core/TSDMath.hpp"
// #include "tsd/core/scene/objects/Array.hpp"
// #include "tsd/io/importers.hpp"
// #include "tsd/io/importers/detail/HDRImage.h"
// #include "tsd/io/importers/detail/importer_common.hpp"
// usd
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdVol/volume.h>
// pxr
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/assetPath.h>
// std
#include <limits>
#include <string>
#include <vector>
#include <stack>

#include "viewer/content/USD.h"
#include "stb/stb_image.h"


namespace hs {
  using namespace pxr;
  
  std::string pathOf(const std::string &filepath);
  
  inline affine3f to_mini(const pxr::GfMatrix4d &m)
  {
    affine3f out;
    for (int i=0;i<3;i++) {
      out.l.vx[i] = m[0][i];
      out.l.vy[i] = m[1][i];
      out.l.vz[i] = m[2][i];
      out.p[i] = m[3][i];
    }
    return out;
  }

#ifdef _WIN32
  constexpr char path_sep = '\\';
#else
  constexpr char path_sep = '/';
#endif

  std::string pathOf(const std::string &filepath)
  {
    size_t pos = filepath.find_last_of(path_sep);
    if (pos == std::string::npos)
      return "";
    return filepath.substr(0, pos + 1);
  }

  std::string fileOf(const std::string &filepath)
  {
    size_t pos = filepath.find_last_of(path_sep);
    if (pos == std::string::npos)
      return "";
    return filepath.substr(pos + 1, filepath.size());
  }

  std::string extensionOf(const std::string &filepath)
  {
    size_t pos = filepath.rfind('.');
    if (pos == filepath.npos)
      return "";
    return filepath.substr(pos);
  }


  Texture::SP doLoadTexture(const std::string &fileName)
  {
    Texture::SP texture;
    vec2i res;
    int   comp;
    unsigned char* image = stbi_load(fileName.c_str(),
                                     &res.x, &res.y, &comp, STBI_rgb_alpha);
    // int textureID = -1;
    if (image) {
      for (int y=0;y<res.y/2;y++) {
        uint32_t *line_y = ((uint32_t*)image) + y * res.x;
        uint32_t *mirrored_y = ((uint32_t*)image) + (res.y-1-y) * res.x;
        for (int x=0;x<res.x;x++) {
          std::swap(line_y[x],mirrored_y[x]);
        }
      }

      texture = std::make_shared<Texture>();
      texture->size = res;
      texture->data.resize(res.x*res.y*sizeof(int));
      texture->format = Texture::RGBA_UINT8;
      memcpy(texture->data.data(),image,texture->data.size());

      /* iw - actually, it seems that stbi loads the pictures
         mirrored along the y axis - mirror them here */
      free(image);
      // STBI_FREE(image);
      std::cout << "#hs.USD: successfully loaded texture " << fileName << std::endl;
    } else {
      std::cout << "#hs.USD(!!!): could not load texture " << fileName << std::endl;
    }

    return texture;
  }

  
  struct USDScene {
    USDScene(const std::string &fileName)
      : fileName(fileName),
        basePath(pathOf(fileName))
    {}
    
    const std::string fileName;
    const std::string basePath;
    
    std::map<std::string,mini::Material::SP> materials;
    std::map<std::string,mini::Texture::SP> textures;

    mini::Texture::SP getTexture(const std::string &name)
    {
      if (textures.find(name) != textures.end())
        return textures[name];

      std::string fileName = name;//basePath + name;
      if (!fileName.empty() && fileName[0] != '/')
        fileName = basePath+name;
      // std::string resolvedPath = metallicTexPath;
      // if (!resolvedPath.empty() && resolvedPath[0] != '/') {
      //   resolvedPath = basePath + metallicTexPath;
      // }
      
      Texture::SP tex = doLoadTexture(fileName);
      textures[name] = tex;
      return tex;
    }
    
    mini::Material::SP getMaterial(const std::string &name)
    {
      if (materials.find(name) == materials.end())
        return mini::DisneyMaterial::create();
      return materials[name];
    }
    
    std::stack<affine3f> savedXFs;
    affine3f currentXF;
    box3f bounds;

    std::vector<mini::Mesh::SP> meshes;

    void push(mini::Mesh::SP mesh) { meshes.push_back(mesh); }
    
    void pushTransform(const affine3f &xf)
    {
      savedXFs.push(currentXF);
      currentXF = xf;
    }
    
    void popTransform()
    {
      currentXF = savedXFs.top();
      savedXFs.pop();
    }
  };




  // // Template helpers for setting material parameters from USD shader inputs
  // static void setShaderInputIfPresent(mini::DisneyMaterial::SP mat,
  //                                     pxr::UsdShadeShader &shader,
  //                                     const char *inputName,
  //                                     const char *paramName)
  // {
  //   pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
  //   pxr::GfVec3f colorVal;
  //   if (input && input.Get(&colorVal)) {
  //     printf("[import_USD] Setting %s: %f %f %f\n",
  //            paramName,
  //            colorVal[0],
  //            colorVal[1],
  //            colorVal[2]);
  //     // mat->setParameter(tsd::core::Token(paramName),
  //     //                   tsd::math::float3(colorVal[0], colorVal[1], colorVal[2]));
  //   }
  // }

  // static void setShaderInputIfPresent(mini::DisneyMaterial::SP mat,
  //                                     pxr::UsdShadeShader &shader,
  //                                     const char *inputName,
  //                                     const char *paramName,
  //                                     float)
  // {
  //   pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
  //   float floatVal;
  //   if (input && input.Get(&floatVal)) {
  //     printf("[import_USD] Setting %s: %f\n", paramName, floatVal);
  //     // mat->setParameter(tsd::core::Token(paramName), floatVal);
  //   }
  // }

  



  //   mini::Material::SP
  //   import_usd_preview_surface_material(USDScene &scene,
  //                                       const pxr::UsdShadeMaterial &usdMat)
  //   {
  //     // Find the UsdPreviewSurface shader
  //     pxr::UsdShadeShader surfaceShader;
  //     pxr::TfToken outputName("surface");
  //     pxr::UsdShadeOutput surfaceOutput = usdMat.GetOutput(outputName);

  //     PING;
  //     if (surfaceOutput && surfaceOutput.HasConnectedSource()) {
  //       printf("[import_USD] Surface output has connected source\n");
  //       pxr::UsdShadeConnectableAPI source;
  //       pxr::TfToken sourceName;
  //       pxr::UsdShadeAttributeType sourceType;
  //       surfaceOutput.GetConnectedSource(&source, &sourceName, &sourceType);
  // #if 1
  //       surfaceShader = pxr::UsdShadeShader(source);
  //       pxr::TfToken subIdentifier;
  //       surfaceShader.GetSourceAssetSubIdentifier(&subIdentifier, pxr::TfToken("mdl"));
  //       if (subIdentifier == pxr::TfToken("OmniPBR")) {
  //         // mat = materials::importOmniPBRMaterial(
  //         //                                        scene, usdMaterial, shader, basePath, textureCache);
  //         PING;
  //         // break;
  //       } else {
  //         printf("Don't know how to process %s\n", subIdentifier.GetText());
  //       }
  // #else
  //       surfaceShader = pxr::UsdShadeShader(source.GetPrim());
  // #endif
  //     }
 
  //     PING;
  //     if (!surfaceShader)
  //       return mini::DisneyMaterial::create();//scene.defaultMaterial();

  //     PING;
  //     auto mat = mini::DisneyMaterial::create();
  //     // auto mat = scene.createObject<Material>(tokens::material::physicallyBased);

  //     PING;
  //     setShaderInputIfPresent(mat, surfaceShader, "diffuseColor", "baseColor");
  //     setShaderInputIfPresent(mat, surfaceShader, "emissiveColor", "emissive");
  //     setShaderInputIfPresent(mat, surfaceShader, "metallic", "metallic", 0.0f);
  //     setShaderInputIfPresent(mat, surfaceShader, "roughness", "roughness", 0.0f);
  //     setShaderInputIfPresent(mat, surfaceShader, "clearcoat", "clearcoat", 0.0f);
  //     setShaderInputIfPresent(mat, surfaceShader, "clearcoatRoughness", "clearcoatRoughness", 0.0f);
  //     setShaderInputIfPresent(mat, surfaceShader, "opacity", "opacity", 0.0f);
  //     setShaderInputIfPresent(mat, surfaceShader, "ior", "ior", 0.0f);

  //     PING;
  //     // Set name
  //     std::string matName = usdMat.GetPrim().GetPath().GetString();
  //     if (matName.empty())
  //       matName = "USDPreviewSurface";
  //     // mat->setName(matName.c_str());
  //     printf("[import_USD] Created material: %s\n", matName.c_str());

  //     return mat;
  //   }
  


  // Helper to get the bound material for a prim (USD or default)
  mini::Material::SP get_bound_material(USDScene &scene,
                                        const pxr::UsdPrim &prim)
  {
    // MaterialRef mat = scene.defaultMaterial();
    mini::Material::SP mat = mini::DisneyMaterial::create();
    pxr::UsdShadeMaterialBindingAPI binding(prim);
    pxr::UsdShadeMaterial usdMat = binding.ComputeBoundMaterial();
    std::string materialPath = usdMat.GetPath().GetString();
    
    return scene.getMaterial(materialPath);
    // //

    
    // // 
    // std::cout << "MATERIAL *USE* " << materialPath << std::endl;

    // PRINT((bool)usdMat);
    // if (usdMat)
    //   mat = import_usd_preview_surface_material(scene, usdMat, basePath);
    // return mat;
  }



  
  // Helper to check if a GfMatrix4d is identity
  bool is_identity(const pxr::GfMatrix4d &m)
  {
    static const pxr::GfMatrix4d IDENTITY(1.0);
    return m == IDENTITY;
  }

  void import_usd_dome_light(USDScene &scene,
                             const pxr::UsdPrim &prim,
                             const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }

  void import_usd_volume(USDScene &scene,
                         const pxr::UsdPrim &prim,
                         const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }
  

  void import_usd_disk_light(USDScene &scene, const pxr::UsdPrim &prim)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }

  void import_usd_sphere_light(USDScene &scene, const pxr::UsdPrim &prim)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }


  void import_usd_rect_light(USDScene &scene, const pxr::UsdPrim &prim)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }

  void import_usd_distant_light(USDScene &scene, const pxr::UsdPrim &prim)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }


  void import_usd_cone(USDScene &scene,
                       const pxr::UsdPrim &prim,
                       const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }

  void import_usd_sphere(USDScene &scene,
                         const pxr::UsdPrim &prim,
                         const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }

  void import_usd_points(USDScene &scene,
                         const pxr::UsdPrim &prim,
                         const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }


  static std::vector<uint32_t> generate_triangle_indices(
                                                         const pxr::VtArray<int> &faceVertexIndices,
                                                         const pxr::VtArray<int> &faceVertexCounts)
  {
    std::vector<uint32_t> triangleIndices;
    size_t faceVertexOffset = 0;

    for (size_t face = 0; face < faceVertexCounts.size(); ++face) {
      int vertsInFace = faceVertexCounts[face];

      // Tessellate polygon as triangle fan: (0,1,2), (0,2,3), (0,3,4), ...
      for (int v = 2; v < vertsInFace; ++v) {
        triangleIndices.push_back(faceVertexIndices[faceVertexOffset + 0]);
        triangleIndices.push_back(faceVertexIndices[faceVertexOffset + v - 1]);
        triangleIndices.push_back(faceVertexIndices[faceVertexOffset + v]);
      }

      faceVertexOffset += vertsInFace;
    }

    return triangleIndices;
  }

// Helper: Tessellate faceVarying data from polygons to triangles
// FaceVarying data has one value per face-vertex (corner)
// Returns tessellated data matching the triangle fan pattern
template <typename T>
static std::vector<T> tessellate_facevarying_data(
    const pxr::VtArray<T> &faceVaryingData,
    const pxr::VtArray<int> &faceVertexCounts)
{
  std::vector<T> triangleData;
  size_t faceVertexOffset = 0;

  for (size_t face = 0; face < faceVertexCounts.size(); ++face) {
    int vertsInFace = faceVertexCounts[face];

    // Tessellate as triangle fan: (0,1,2), (0,2,3), (0,3,4), ...
    for (int v = 2; v < vertsInFace; ++v) {
      triangleData.push_back(faceVaryingData[faceVertexOffset + 0]);
      triangleData.push_back(faceVaryingData[faceVertexOffset + v - 1]);
      triangleData.push_back(faceVaryingData[faceVertexOffset + v]);
    }

    faceVertexOffset += vertsInFace;
  }

  return triangleData;
}

// Helper: Tessellate uniform (per-face) data to per-triangle
// Uniform data has one value per face
// Returns replicated data with one value per generated triangle
template <typename T>
static std::vector<T> tessellate_uniform_data(
    const pxr::VtArray<T> &uniformData,
    const pxr::VtArray<int> &faceVertexCounts)
{
  std::vector<T> triangleData;

  for (size_t face = 0; face < faceVertexCounts.size(); ++face) {
    int vertsInFace = faceVertexCounts[face];
    int numTriangles = vertsInFace - 2;

    // Each triangle from this face gets the same uniform value
    for (int t = 0; t < numTriangles; ++t) {
      triangleData.push_back(uniformData[face]);
    }
  }

  return triangleData;
}
  
  void import_usd_mesh(USDScene &scene,
                       const pxr::UsdPrim &prim,
                       const pxr::GfMatrix4d &usdXform)
  {
#if 1
    pxr::UsdGeomMesh mesh(prim);
    
    // Get vertex positions
    pxr::VtArray<pxr::GfVec3f> points;
    mesh.GetPointsAttr().Get(&points);
    
    // Get face topology
    pxr::VtArray<int> faceVertexIndices;
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
    pxr::VtArray<int> faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    
    // Get normals and their interpolation
    pxr::VtArray<pxr::GfVec3f> normals;
    pxr::TfToken normalsInterpolation = pxr::UsdGeomTokens->vertex; // Default
    mesh.GetNormalsAttr().Get(&normals);
    if (!normals.empty()) {
      normalsInterpolation = mesh.GetNormalsInterpolation();
    }

    // Get UVs and their interpolation
    pxr::VtArray<pxr::GfVec2f> uvs;
    pxr::TfToken uvsInterpolation = pxr::UsdGeomTokens->vertex; // Default
    // USD stores UVs as primvars, typically "st" or "UVMap"
    pxr::UsdGeomPrimvarsAPI primvarsAPI(mesh);
    pxr::UsdGeomPrimvar stPrimvar = primvarsAPI.GetPrimvar(pxr::TfToken("st"));
    if (!stPrimvar) {
      stPrimvar = primvarsAPI.GetPrimvar(pxr::TfToken("UVMap"));
    }
    if (stPrimvar) {
      // USD primvars can be indexed - need to use ComputeFlattened to expand
      // indices
      stPrimvar.ComputeFlattened(&uvs);
      if (!uvs.empty()) {
        uvsInterpolation = stPrimvar.GetInterpolation();
      }
    }

    std::string primName = prim.GetPath().GetString();
    if (primName.empty())
      primName = "<unnamed_mesh>";

    printf("[import_USD] Mesh '%s': %zu points, %zu faces, %zu normals (interpolation: %s), %zu UVs (interpolation: %s)\n",
              prim.GetName().GetString().c_str(),
              points.size(),
              faceVertexCounts.size(),
              normals.size(),
              normalsInterpolation.GetText(),
              uvs.size(),
              uvsInterpolation.GetText());

    // Convert vertex positions to float3
    std::vector<vec3f> positions;
    positions.reserve(points.size());
    for (const auto &p : points) {
      positions.push_back(vec3f(p[0], p[1], p[2]));
    }
    
    // Generate triangle indices from polygon faces
    std::vector<uint32_t> indices =
      generate_triangle_indices(faceVertexIndices, faceVertexCounts);

    printf("[import_USD] Mesh '%s': Generated %zu triangle indices (%zu triangles)\n",
           prim.GetName().GetString().c_str(),
           indices.size(),
           indices.size() / 3);
    
    // Create ANARI indexed triangle geometry
    // auto meshObj = scene.createObject<Geometry>(tokens::geometry::triangle);
    mini::Mesh::SP meshObj = mini::Mesh::create();
    
    // Set vertex positions
    // auto vertexPositionArray =
    //   scene.createArray(ANARI_FLOAT32_VEC3, positions.size());
    // vertexPositionArray->setData(positions.data(), positions.size());
    // meshObj->setParameterObject("vertex.position", *vertexPositionArray);
    meshObj->vertices = positions;
    
    // Set triangle indices
    // auto indexArray = scene.createArray(ANARI_UINT32_VEC3, indices.size() / 3);
    // indexArray->setData((uint3 *)indices.data(), indices.size() / 3);
    // meshObj->setParameterObject("primitive.index", *indexArray);
    meshObj->indices = std::vector<vec3i>(indices.size()/3);
    std::copy(indices.begin(),indices.end(),(int*)meshObj->indices.data());

    // Handle normals based on USD interpolation
    if (!normals.empty()) {
      if (normalsInterpolation == pxr::UsdGeomTokens->vertex) {
        // Vertex interpolation: normals are per-vertex, shared by all triangles
        // No tessellation needed - just convert to float3
        std::vector<vec3f> normalData;
        normalData.reserve(normals.size());
        for (const auto &n : normals) {
          normalData.push_back(vec3f(n[0], n[1], n[2]));
        }

        // auto normalsArray =
        //   scene.createArray(ANARI_FLOAT32_VEC3, normalData.size());
        // normalsArray->setData(normalData.data(), normalData.size());
        // meshObj->setParameterObject("vertex.normal", *normalsArray);
        meshObj->normals = normalData;

        printf("[import_USD] Mesh '%s': Set %zu normals on vertex.normal\n",
               prim.GetName().GetString().c_str(),
               normalData.size());
      
      } else if (normalsInterpolation == pxr::UsdGeomTokens->faceVarying) {
        // FaceVarying interpolation: normals are per face-vertex (corner)
        // Need to tessellate from polygon corners to triangle corners
        auto tessellatedNormals =
          tessellate_facevarying_data(normals, faceVertexCounts);

        std::vector<vec3f> normalData;
        normalData.reserve(tessellatedNormals.size());
        for (const auto &n : tessellatedNormals) {
          normalData.push_back(vec3f(n[0], n[1], n[2]));
        }

        // auto normalsArray =
        //   scene.createArray(ANARI_VEC3F2_VEC3, normalData.size());
        // normalsArray->setData(normalData.data(), normalData.size());
        // meshObj->setParameterObject("faceVarying.normal", *normalsArray);
        meshObj->normals = normalData;
        printf("[import_USD] Mesh '%s': Set %zu normals on faceVarying.normal\n",
               prim.GetName().GetString().c_str(),
               normalData.size());
        
      } else if (normalsInterpolation == pxr::UsdGeomTokens->uniform) {
        // Uniform interpolation: one normal per face
        // Need to replicate for each triangle generated from that face
        auto tessellatedNormals =
          tessellate_uniform_data(normals, faceVertexCounts);
        
        std::vector<vec3f> normalData;
        normalData.reserve(tessellatedNormals.size());
        for (const auto &n : tessellatedNormals) {
          normalData.push_back(vec3f(n[0], n[1], n[2]));
        }
        
        // auto normalsArray =
        //   scene.createArray(ANARI_FLOAT32_VEC3, normalData.size());
        // normalsArray->setData(normalData.data(), normalData.size());
        // meshObj->setParameterObject("primitive.normal", *normalsArray);
        meshObj->normals = normalData;
        
        printf("[import_USD] Mesh '%s': Set %zu normals on primitive.normal\n",
               prim.GetName().GetString().c_str(),
               normalData.size());
      }
    }
    
    // Handle UVs based on USD interpolation
    if (!uvs.empty()) {
      if (uvsInterpolation == pxr::UsdGeomTokens->vertex) {
        // Vertex interpolation: UVs are per-vertex, shared by all triangles
        // No tessellation needed - just convert to float2
        std::vector<vec2f> uvData;
        uvData.reserve(uvs.size());
        for (const auto &uv : uvs) {
          // USD is bottom-up, ANARI is top-down
          uvData.push_back(vec2f(uv[0], //1.0f -
                                 uv[1]));
        }
        
        // auto uvsArray = scene.createArray(ANARI_FLOAT32_VEC2, uvData.size());
        // uvsArray->setData(uvData.data(), uvData.size());
        // meshObj->setParameterObject("vertex.attribute0", *uvsArray);
        meshObj->texcoords = uvData;
        
        printf("[import_USD] Mesh '%s': Set %zu UVs on vertex.attribute0\n",
               prim.GetName().GetString().c_str(),
               uvData.size());
      
      } else if (uvsInterpolation == pxr::UsdGeomTokens->faceVarying) {
        // FaceVarying interpolation: UVs are per face-vertex (corner)
        // Need to tessellate from polygon corners to triangle corners
        auto tessellatedUVs = tessellate_facevarying_data(uvs, faceVertexCounts);
        
        std::vector<vec2f> uvData;
        uvData.reserve(tessellatedUVs.size());
        for (const auto &uv : tessellatedUVs) {
          // USD is bottom-up, ANARI is top-down
          uvData.push_back(vec2f(uv[0], //1.0f -
                                 uv[1]));
        }

        // auto uvsArray = scene.createArray(ANARI_FLOAT32_VEC2, uvData.size());
        // uvsArray->setData(uvData.data(), uvData.size());
        // meshObj->setParameterObject("faceVarying.attribute0", *uvsArray);
        meshObj->texcoords = uvData;

        printf("[import_USD] Mesh '%s': Set %zu UVs on faceVarying.attribute0\n",
               prim.GetName().GetString().c_str(),
               uvData.size());

      } else if (uvsInterpolation == pxr::UsdGeomTokens->uniform) {
        // Uniform interpolation: one UV per face
        // Need to replicate for each triangle generated from that face
        auto tessellatedUVs = tessellate_uniform_data(uvs, faceVertexCounts);

        std::vector<vec2f> uvData;
        uvData.reserve(tessellatedUVs.size());
        for (const auto &uv : tessellatedUVs) {
          // USD is bottom-up, ANARI is top-down
          uvData.push_back(vec2f(uv[0], //1.0f -
                                 uv[1]));
        }

        // auto uvsArray = scene.createArray(ANARI_FLOAT32_VEC2, uvData.size());
        // uvsArray->setData(uvData.data(), uvData.size());
        // meshObj->setParameterObject("primitive.attribute0", *uvsArray);
        meshObj->texcoords = uvData;

        printf("[import_USD] Mesh '%s': Set %zu UVs on primitive.attribute0\n",
               prim.GetName().GetString().c_str(),
               uvData.size());
      }
    }

#else    
    pxr::UsdGeomMesh mesh(prim);
    pxr::VtArray<pxr::GfVec3f> points;
    mesh.GetPointsAttr().Get(&points);

    // Update scene bounding box with transformed points
    for (size_t i = 0; i < points.size(); ++i) {
      pxr::GfVec3f p = points[i];
      pxr::GfVec4d p4(p[0], p[1], p[2], 1.0);
      pxr::GfVec4d wp4 = usdXform * p4;
      vec3f wp{float(wp4[0]), float(wp4[1]), float(wp4[2])};
      scene.bounds.extend(wp);
    }

    pxr::VtArray<int> faceVertexIndices;
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
    pxr::VtArray<int> faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    pxr::VtArray<pxr::GfVec3f> normals;
    mesh.GetNormalsAttr().Get(&normals);

    // Try to get UV coordinates from primvars
    pxr::UsdGeomPrimvarsAPI primvarsAPI(prim);
    pxr::VtArray<pxr::GfVec2f> uvs;
    pxr::VtArray<int> uvIndices;
    pxr::TfToken uvInterpolation;
    bool hasUVs = false;
    bool hasUVIndices = false;

    // Try common UV primvar names
    const char *uvPrimvarNames[] = {"st", "uv", "UVMap"};
    for (const char *uvName : uvPrimvarNames) {
      pxr::UsdGeomPrimvar uvPrimvar =
        primvarsAPI.GetPrimvar(pxr::TfToken(uvName));
      if (uvPrimvar && uvPrimvar.HasValue()) {
        if (uvPrimvar.Get(&uvs)) {
          uvInterpolation = uvPrimvar.GetInterpolation();
          hasUVs = true;
          // Check if this primvar has indices
          if (uvPrimvar.GetIndices(&uvIndices) && !uvIndices.empty()) {
            hasUVIndices = true;
            printf("[import_USD] Mesh '%s': Found UV primvar '%s' with %zu values, %zu indices, interpolation: %s\n",
                   prim.GetName().GetString().c_str(),
                   uvName,
                   uvs.size(),
                   uvIndices.size(),
                   uvInterpolation.GetText());
          } else {
            printf("[import_USD] Mesh '%s': Found UV primvar '%s' with %zu values, interpolation: %s\n",
                   prim.GetName().GetString().c_str(),
                   uvName,
                   uvs.size(),
                   uvInterpolation.GetText());
          }
          break;
        }
      }
    }
    
    printf("[import_USD] Mesh '%s': %zu points, %zu faces, %zu normals, %zu UVs\n",
           prim.GetName().GetString().c_str(),
           points.size(),
           faceVertexCounts.size(),
           normals.size(),
           uvs.size());
    
    std::vector<vec3f> outVertices;
    std::vector<vec3f> outNormals;
    std::vector<vec2f> outUVs;
    size_t index = 0;
    size_t uvIndex = 0;
    
    for (size_t face = 0; face < faceVertexCounts.size(); ++face) {
      int vertsInFace = faceVertexCounts[face];
      for (int v = 2; v < vertsInFace; ++v) {
        int idx0 = faceVertexIndices[index];
        int idx1 = faceVertexIndices[index + v - 1];
        int idx2 = faceVertexIndices[index + v];

        // Vertices
        outVertices.push_back(vec3f(points[idx0][0], points[idx0][1], points[idx0][2]));
        outVertices.push_back(vec3f(points[idx1][0], points[idx1][1], points[idx1][2]));
        outVertices.push_back(vec3f(points[idx2][0], points[idx2][1], points[idx2][2]));

        // // Normals
        // if (normals.size() == points.size()) {
        //   outNormals.push_back(vec3f(normals[idx0][0], normals[idx0][1], normals[idx0][2]));
        //   outNormals.push_back(vec3f(normals[idx1][0], normals[idx1][1], normals[idx1][2]));
        //   outNormals.push_back(vec3f(normals[idx2][0], normals[idx2][1], normals[idx2][2]));
        // }
        //   } else if (uvInterpolation == pxr::UsdGeomTokens->faceVarying) {

        // UVs - handle different interpolation modes
        if (hasUVs && !uvs.empty()) {
          if (hasUVIndices) {
            // Indexed UVs: use the indices array to look up UV values
            // The indices correspond to face-vertex ordering
            if (uvIndex + v < uvIndices.size()) {
              int uvIdx0 = uvIndices[uvIndex];
              int uvIdx1 = uvIndices[uvIndex + v - 1];
              int uvIdx2 = uvIndices[uvIndex + v];
              if (uvIdx0 < (int)uvs.size() && uvIdx1 < (int)uvs.size()
                  && uvIdx2 < (int)uvs.size()) {
                // Flip V coordinate: USD has V=0 at bottom, ANARI has V=0 at top
                outUVs.push_back(vec2f(uvs[uvIdx0][0],
                                       // 1.0f -
                                       uvs[uvIdx0][1]));
                outUVs.push_back(vec2f(uvs[uvIdx1][0],
                                       // 1.0f -
                                       uvs[uvIdx1][1]));
                outUVs.push_back(vec2f(uvs[uvIdx2][0],
                                       // 1.0f -
                                       uvs[uvIdx2][1]));
              }
            }
          } else if (uvInterpolation == pxr::UsdGeomTokens->faceVarying) {
            PING; PRINT(uvs.size());
            // FaceVarying: one UV per face-vertex (most common)
            if (uvIndex + v < uvs.size()) {
              PING;
              // Flip V coordinate: USD has V=0 at bottom, ANARI has V=0 at top
              outUVs.push_back(vec2f(uvs[uvIndex][0], 1.0f - uvs[uvIndex][1]));
              outUVs.push_back(vec2f(uvs[uvIndex + v - 1][0], 1.0f - uvs[uvIndex + v - 1][1]));
              outUVs.push_back(vec2f(uvs[uvIndex + v][0], 1.0f - uvs[uvIndex + v][1]));
            }
          } else if (uvInterpolation == pxr::UsdGeomTokens->vertex) {
            // Vertex: one UV per vertex (indexed like positions)
            if (idx0 < (int)uvs.size() && idx1 < (int)uvs.size()
                && idx2 < (int)uvs.size()) {
              // Flip V coordinate: USD has V=0 at bottom, ANARI has V=0 at top
              outUVs.push_back(vec2f(uvs[idx0][0], 1.0f - uvs[idx0][1]));
              outUVs.push_back(vec2f(uvs[idx1][0], 1.0f - uvs[idx1][1]));
              outUVs.push_back(vec2f(uvs[idx2][0], 1.0f - uvs[idx2][1]));
            }
          } else if (uvInterpolation == pxr::UsdGeomTokens->uniform) {
            // Uniform: one UV per face
            if (face < uvs.size()) {
              // Flip V coordinate: USD has V=0 at bottom, ANARI has V=0 at top
              vec2f faceUV(uvs[face][0], 1.0f - uvs[face][1]);
              outUVs.push_back(faceUV);
              outUVs.push_back(faceUV);
              outUVs.push_back(faceUV);
            }
          }
        }
      }

      // Advance indices
      index += vertsInFace;
      if (hasUVs
          && (hasUVIndices
              || uvInterpolation == pxr::UsdGeomTokens->faceVarying)) {
        uvIndex += vertsInFace;
      }
    }

    mini::Mesh::SP meshObj = mini::Mesh::create();
    // auto meshObj = scene.createObject<Geometry>(tokens::geometry::triangle);
    // auto vertexPositionArray =
    //   scene.createArray(ANARI_FLOAT32_VEC3, outVertices.size());
    // vertexPositionArray->setData(outVertices.data(), outVertices.size());
    // meshObj->setParameterObject("vertex.position", *vertexPositionArray);
    meshObj->vertices = outVertices;
    for (int i=0;i<meshObj->vertices.size()/3;i++)
      meshObj->indices.push_back(3*i+vec3i(0,1,2));
    if (!outNormals.empty()) {
      // auto normalsArray =
      //   scene.createArray(ANARI_FLOAT32_VEC3, outNormals.size());
      // normalsArray->setData(outNormals.data(), outNormals.size());
      // meshObj->setParameterObject("vertex.normal", *normalsArray);
      meshObj->normals = outNormals;
    }

    if (!outUVs.empty()) {
      // auto uvArray = scene.createArray(ANARI_FLOAT32_VEC2, outUVs.size());
      // uvArray->setData(outUVs.data(), outUVsize());
      // meshObj->setParameterObject("vertex.attribute0", *uvArray);
      // logStatus(
      //           "[import_USD] Mesh '%s': Set %zu UV coordinates on vertex.attribute0\n",
      //           prim.GetName().GetString().c_str(),
      //           outUVs.size());
      meshObj->texcoords = outUVs;
    }

    std::string primName = prim.GetPath().GetString();
    if (primName.empty())
      primName = "<unnamed_mesh>";
    // meshObj->setName(primName.c_str());
#endif
    // Material binding
    meshObj->material = get_bound_material(scene,prim);
    // MaterialRef mat = get_bound_material(scene, prim, basePath);
    // auto surface = scene.createSurface(primName.c_str(), meshObj, mat);
    // printf("[import_USD] Assigned material to mesh '%s': %s\n",
    //        primName.c_str(),
    //        mat->name().c_str());
    // scene.insertChildObjectNode(parent, surface);
    scene.push(meshObj);
  }

  void import_usd_cylinder(USDScene &scene,
                           const pxr::UsdPrim &prim,
                           const pxr::GfMatrix4d &usdXform)
  {
    std::cout << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
  }
  
  static
  void import_usd_prim_recursive(USDScene &scene,
                                 const pxr::UsdPrim &prim,
                                 pxr::UsdGeomXformCache &xformCache,
                                 const pxr::GfMatrix4d &parentWorldXform = pxr::GfMatrix4d(1.0))
  {
    if (prim.IsInstance()) {
      pxr::UsdPrim prototype = prim.GetPrototype();
      if (prototype) {
        bool resetsXformStack = false;
        pxr::GfMatrix4d usdLocalXform =
          xformCache.GetLocalTransformation(prim, &resetsXformStack);
        pxr::GfMatrix4d thisWorldXform =
          resetsXformStack ? usdLocalXform : parentWorldXform * usdLocalXform;
        // tsd::math::mat4 tsdXform = to_tsd_mat4(usdLocalXform);
        affine3f miniXform = to_mini(usdLocalXform);
        std::string primName = prim.GetName().GetString();
        if (primName.empty())
          primName = "<unnamed_instance>";
        // auto xformNode =
        //   scene.insertChildTransformNode(parent, tsdXform, primName.c_str());
        // Recursively import the prototype under this transform node
        scene.pushTransform(miniXform);
        import_usd_prim_recursive(scene,
                                  prototype,
                                  // xformNode,
                                  xformCache,
                                  thisWorldXform);
        scene.popTransform();
      } else {
        printf("[import_USD] Instance has no prototype: %s\n",
               prim.GetName().GetString().c_str());
      }
      return;
    }

    // Only declare these in the main body (non-instance case)
    bool resetsXformStack = false;
    pxr::GfMatrix4d usdLocalXform =
      xformCache.GetLocalTransformation(prim, &resetsXformStack);
    pxr::GfMatrix4d thisWorldXform =
      resetsXformStack ? usdLocalXform : parentWorldXform * usdLocalXform;

    // Determine if this prim is a geometry or light
    bool isGeometry = prim.IsA<pxr::UsdGeomMesh>()
      || prim.IsA<pxr::UsdGeomPoints>() || prim.IsA<pxr::UsdGeomSphere>()
      || prim.IsA<pxr::UsdGeomCone>() || prim.IsA<pxr::UsdGeomCylinder>();
    bool isVolume = prim.IsA<pxr::UsdVolVolume>();
    bool isLight = prim.IsA<pxr::UsdLuxDistantLight>()
      || prim.IsA<pxr::UsdLuxRectLight>() || prim.IsA<pxr::UsdLuxSphereLight>()
      || prim.IsA<pxr::UsdLuxDiskLight>() || prim.IsA<pxr::UsdLuxDomeLight>();
    bool isDomeLight = prim.IsA<pxr::UsdLuxDomeLight>();
    bool isXform = prim.IsA<pxr::UsdGeomXform>() || prim.IsA<pxr::UsdGeomScope>();

    // Count children
    size_t numChildren = 0;
    for (const auto &child : prim.GetChildren())
      ++numChildren;

    // Only create a transform node if:
    // - The local transform is not identity
    // - The prim is geometry, light (not dome), or volume
    //   For the domelight, the rationale is the domelight can encode the
    //   transformation in
    //     its orientation axes and at least VisRTX and Barney do not correctly
    //     support transforming the HDRI lights.
    // - The prim resets the xform stack
    bool createNode = !is_identity(usdLocalXform) || isGeometry || isLight
      || isVolume || resetsXformStack;
    createNode = createNode && !isDomeLight;

    // tsd::math::mat4 tsdXform = to_tsd_mat4(usdLocalXform);
    affine3f miniXform = to_mini(usdLocalXform);;
    std::string primName = prim.GetName().GetString();
    if (primName.empty())
      primName = "<unnamed_xform>";

    // if (createNode) {
    //   thisNode =
    //     scene.insertChildTransformNode(parent, tsdXform, primName.c_str());
    // }

    // Import geometry for this prim (if any)
    if (prim.IsA<pxr::UsdGeomMesh>()) {
      import_usd_mesh(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdGeomPoints>()) {
      import_usd_points(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdGeomSphere>()) {
      import_usd_sphere(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdGeomCone>()) {
      import_usd_cone(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdGeomCylinder>()) {
      import_usd_cylinder(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdLuxDistantLight>()) {
      import_usd_distant_light(scene, prim);
    } else if (prim.IsA<pxr::UsdLuxRectLight>()) {
      import_usd_rect_light(scene, prim);
    } else if (prim.IsA<pxr::UsdLuxSphereLight>()) {
      import_usd_sphere_light(scene, prim);
    } else if (prim.IsA<pxr::UsdLuxDiskLight>()) {
      import_usd_disk_light(scene, prim);
    } else if (prim.IsA<pxr::UsdLuxDomeLight>()) {
      import_usd_dome_light(scene, prim, thisWorldXform);
    } else if (prim.IsA<pxr::UsdVolVolume>()) {
      import_usd_volume(scene, prim, thisWorldXform);
    }
    // Recurse into children
    for (const auto &child : prim.GetChildren()) {
      import_usd_prim_recursive(scene,
                                child,
                                xformCache,
                                thisWorldXform);
    }
  }




  bool getShaderFloatInput(const pxr::UsdShadeShader &shader,
                           const char *inputName,
                           float &outValue)
  {
    pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
    if (!input) {
      return false;
    }

    // Check if there's a connected source
    if (input.HasConnectedSource()) {
      pxr::UsdShadeConnectableAPI source;
      pxr::TfToken sourceName;
      pxr::UsdShadeAttributeType sourceType;
      if (input.GetConnectedSource(&source, &sourceName, &sourceType)) {
        // Check if this is a connection to a material interface input
        pxr::UsdPrim sourcePrim = source.GetPrim();
        if (sourcePrim.IsA<pxr::UsdShadeMaterial>()) {
          // This is a material interface connection - get the value from the material's input
          pxr::UsdShadeMaterial mat(sourcePrim);
          pxr::UsdShadeInput matInput = mat.GetInput(sourceName);
          if (matInput && matInput.Get(&outValue)) {
            return true;
          }
        } else {
          // This is a connection to another shader's output
          pxr::UsdShadeShader sourceShader(sourcePrim);
          if (sourceShader) {
            pxr::UsdShadeOutput output = sourceShader.GetOutput(sourceName);
            if (output) {
              pxr::UsdAttribute attr = output.GetAttr();
              if (attr && attr.Get(&outValue)) {
                return true;
              }
            }
          }
        }
      }
    }

    // Fall back to direct value
    if (input.Get(&outValue)) {
      return true;
    }
    return false;
  }


  bool getShaderBoolInput(const pxr::UsdShadeShader &shader,
                          const char *inputName,
                          bool &outValue)
  {
    pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
    if (!input) {
      return false;
    }

    // Check if there's a connected source
    if (input.HasConnectedSource()) {
      pxr::UsdShadeConnectableAPI source;
      pxr::TfToken sourceName;
      pxr::UsdShadeAttributeType sourceType;
      if (input.GetConnectedSource(&source, &sourceName, &sourceType)) {
        // Check if this is a connection to a material interface input
        pxr::UsdPrim sourcePrim = source.GetPrim();
        if (sourcePrim.IsA<pxr::UsdShadeMaterial>()) {
          // This is a material interface connection - get the value from the material's input
          pxr::UsdShadeMaterial mat(sourcePrim);
          pxr::UsdShadeInput matInput = mat.GetInput(sourceName);
          if (matInput && matInput.Get(&outValue)) {
            return true;
          }
        } else {
          // This is a connection to another shader's output
          pxr::UsdShadeShader sourceShader(sourcePrim);
          if (sourceShader) {
            pxr::UsdShadeOutput output = sourceShader.GetOutput(sourceName);
            if (output) {
              pxr::UsdAttribute attr = output.GetAttr();
              if (attr && attr.Get(&outValue)) {
                return true;
              }
            }
          }
        }
      }
    }

    // Fall back to direct value
    if (input.Get(&outValue)) {
      return true;
    }
    return false;
  }

  bool getShaderColorInput(const pxr::UsdShadeShader &shader,
                           const char *inputName,
                           pxr::GfVec3f &outValue)
  {
    pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
    if (!input) {
      return false;
    }

    // Check if there's a connected source
    if (input.HasConnectedSource()) {
      pxr::UsdShadeConnectableAPI source;
      pxr::TfToken sourceName;
      pxr::UsdShadeAttributeType sourceType;
      if (input.GetConnectedSource(&source, &sourceName, &sourceType)) {
        // Check if this is a connection to a material interface input
        pxr::UsdPrim sourcePrim = source.GetPrim();
        if (sourcePrim.IsA<pxr::UsdShadeMaterial>()) {
          // This is a material interface connection - get the value from the material's input
          pxr::UsdShadeMaterial mat(sourcePrim);
          pxr::UsdShadeInput matInput = mat.GetInput(sourceName);
          if (matInput && matInput.Get(&outValue)) {
            return true;
          }
        } else {
          // This is a connection to another shader's output
          pxr::UsdShadeShader sourceShader(sourcePrim);
          if (sourceShader) {
            pxr::UsdShadeOutput output = sourceShader.GetOutput(sourceName);
            if (output) {
              pxr::UsdAttribute attr = output.GetAttr();
              if (attr && attr.Get(&outValue)) {
                return true;
              }
            }
          }
        }
      }
    }

    // Fall back to direct value
    if (input.Get(&outValue)) {
      return true;
    }
    return false;
  }

  bool getShaderTextureInput(const pxr::UsdShadeShader &shader,
                             const char *inputName,
                             std::string &outFilePath)
  {
    pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(inputName));
    if (!input) {
      return false;
    }

    // Check if there's a connected texture reader
    if (input.HasConnectedSource()) {
      pxr::UsdShadeConnectableAPI source;
      pxr::TfToken sourceName;
      pxr::UsdShadeAttributeType sourceType;
      input.GetConnectedSource(&source, &sourceName, &sourceType);

      // Check if this is a connection to a material interface input
      pxr::UsdPrim sourcePrim = source.GetPrim();
      if (sourcePrim.IsA<pxr::UsdShadeMaterial>()) {
        // This is a material interface connection - get the value from the material's input
        pxr::UsdShadeMaterial mat(sourcePrim);
        pxr::UsdShadeInput matInput = mat.GetInput(sourceName);
        if (matInput) {
          pxr::SdfAssetPath assetPath;
          if (matInput.Get(&assetPath)) {
            outFilePath = assetPath.GetResolvedPath();
            if (outFilePath.empty()) {
              outFilePath = assetPath.GetAssetPath();
            }
            return !outFilePath.empty();
          }
        }
      } else {
        // Check if this is a texture reader shader
        pxr::UsdShadeShader textureShader(sourcePrim);
        if (textureShader) {
          // Look for file input on the texture reader
          pxr::UsdShadeInput fileInput = textureShader.GetInput(pxr::TfToken("file"));
          if (fileInput) {
            pxr::SdfAssetPath assetPath;
            if (fileInput.Get(&assetPath)) {
              outFilePath = assetPath.GetResolvedPath();
              if (outFilePath.empty()) {
                outFilePath = assetPath.GetAssetPath();
              }
              return !outFilePath.empty();
            }
          }
        }
      }
    }

    // Try direct asset path input
    pxr::SdfAssetPath assetPath;
    if (input.Get(&assetPath)) {
      outFilePath = assetPath.GetResolvedPath();
      if (outFilePath.empty()) {
        outFilePath = assetPath.GetAssetPath();
      }
      return !outFilePath.empty();
    }

    return false;
  }
  
  
  mini::Material::SP
  createOmniPBRMaterial(USDScene &scene,
                        const pxr::UsdShadeMaterial &usdMaterial,
                        const pxr::UsdShadeShader &usdShader)
  {
    std::cout << "======================================================= " << std::endl;
    std::cout << "creating new USD material" << std::endl;                                                                                    
    mini::ANARIMaterial::SP mat = mini::ANARIMaterial::create();
    mat->baseColor = mini::common::randomColor(scene.materials.size());
    
    std::string diffuseTexPath;
    if (getShaderTextureInput(usdShader, "diffuse_texture", diffuseTexPath)) {
      mat->baseColor = vec3f(1.f);
      mat->baseColor_texture = scene.getTexture(diffuseTexPath);
    }

    // Handle emissive with intensity
    pxr::GfVec3f emissiveColor(0, 0, 0);
    float emissiveIntensity = 1.0f;
    bool enableEmission = false;

    getShaderColorInput(usdShader, "emissive_color", emissiveColor);
    getShaderFloatInput(usdShader, "emissive_intensity", emissiveIntensity);
    getShaderBoolInput(usdShader, "enable_emission", enableEmission);

    if (enableEmission) {
      // Scale emissive color by intensity
      vec3f finalEmissive(emissiveColor[0] * emissiveIntensity,
                          emissiveColor[1] * emissiveIntensity,
                          emissiveColor[2] * emissiveIntensity
                          );
      mat->emissive = finalEmissive;
    }

    // Metallic - try texture first, then constant
    std::string metallicTexPath;
    if (getShaderTextureInput(usdShader, "metallic_texture", metallicTexPath)) {
      mini::Texture::SP sampler = scene.getTexture(metallicTexPath);
      mat->metallic_texture = sampler;
    } else {
      float metallic = 0.0f;
      if (getShaderFloatInput(usdShader, "metallic_constant", metallic)) {
        mat->metallic = metallic;
      } else {
        mat->metallic = 0.f;
      }
    }

    // Roughness - try texture first, then constant
    std::string roughnessTexPath;
    if (getShaderTextureInput(usdShader, "reflectionroughness_texture",
                              roughnessTexPath)) {
      mini::Texture::SP sampler = scene.getTexture(roughnessTexPath);
      mat->roughness_texture = sampler;
    } else {
      float roughness = 0.5f;  // Default to mid-range roughness
      if (getShaderFloatInput(usdShader, "reflection_roughness_constant",
                              roughness)) {
        mat->roughness = roughness;
      } else {
        mat->roughness = 0.5f;
      }
    }

    // Normal map
    std::string normalTexPath;
    if (getShaderTextureInput(usdShader, "normalmap_texture", normalTexPath)) {
      mini::Texture::SP sampler = scene.getTexture(normalTexPath);
      mat->normal_texture = sampler;
    }

    // Ambient Occlusion map
    std::string aoTexPath;
    if (getShaderTextureInput(usdShader, "ao_texture", aoTexPath)) {
      mini::Texture::SP sampler = scene.getTexture(aoTexPath);
      mat->occlusion_texture = sampler;
    }

    // Opacity - try texture first, then constant
    std::string opacityTexPath;
    bool enableOpacity = false;
    getShaderBoolInput(usdShader, "enable_opacity", enableOpacity);

    // if (enableOpacity) {
    if (getShaderTextureInput(usdShader, "opacity_texture", opacityTexPath)) {
      mini::Texture::SP sampler = scene.getTexture(opacityTexPath);
      mat->opacity_texture = sampler;
    } else {
      float opacityConstant = 1.0f;
      if (getShaderFloatInput(usdShader, "opacity_constant", opacityConstant)) {
        mat->opacity = opacityConstant;
      }
    }

#if 0
    // Set alpha mode based on opacity threshold
    float opacityThreshold = 0.0f;
    if (getShaderFloatInput(usdShader, "opacity_threshold", opacityThreshold)) {
      if (opacityThreshold > 0.0f) {
        mat->setParameter("alphaMode", "mask");
        mat->setParameter("alphaCutoff", opacityThreshold);
      } else {
        mat->setParameter("alphaMode", "blend");
      }
    } else {
      // Default to blend mode when opacity is enabled
      mat->setParameter("alphaMode", "blend");
    }
#endif

    // IOR (index of refraction)
    float ior = 1.5f;
    if (getShaderFloatInput(usdShader, "ior_constant", ior)) {
      mat->ior = ior;
    }

    // Specular level
    float specularLevel = 0.5f;
    if (getShaderFloatInput(usdShader, "specular_level", specularLevel)) {
      mat->specular = specularLevel;
    }
    return mat;
  }


  void createMaterials(USDScene &scene,
                       const std::string &fileName)
  {
    // auto filepath = fileName.c_str();
    // std::string basePath = pathOf(filepath);
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(scene.fileName.c_str());
    // pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(fileName.c_str());
    for (pxr::UsdPrim const &prim : stage->Traverse()) {
      if (prim.IsA<pxr::UsdShadeMaterial>()) {
        std::string materialPath = prim.GetPath().GetString();
        printf("[import_USD2] Found material: %s\n", materialPath.c_str());
        std::cout << "MATERIAL *DEF* " << materialPath << std::endl;
        auto usdMaterial = pxr::UsdShadeMaterial(prim);
        auto surfaceOutputs = usdMaterial.GetSurfaceOutputs();
        auto surfaceOutput = usdMaterial.GetSurfaceOutput(pxr::TfToken("mdl"));

        // MaterialRef mat;

        for (auto &connectionSourceInfo : surfaceOutput.GetConnectedSources()) {
          UsdShadeShader shader(connectionSourceInfo.source);
          TfToken subIdentifier;
          shader.GetSourceAssetSubIdentifier(&subIdentifier, TfToken("mdl"));

          if (subIdentifier == TfToken("OmniPBR")) {
            printf("OmniPBR material...\n");
            scene.materials[materialPath]
              = createOmniPBRMaterial(scene, usdMaterial, shader);
            // mat = materials::importOmniPBRMaterial(
            //                                        scene, usdMaterial, shader, basePath, textureCache);
            break;
          } else {
            printf("Don't know how to process %s\n", subIdentifier.GetText());
          }
        }
          
      }
    }
  }



  
  void   USDContent::executeLoad(DataRank &dataGroup, bool verbose)
  {
    USDScene scene(fileName);
    // auto filepath = fileName.c_str();
    // PING;
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(scene.fileName.c_str());
    createMaterials(scene,fileName);
    if (!stage) {
      printf("[import_USD] failed to open stage '%s'", fileName.c_str());
      return;
    }
    printf("[import_USD] Opened USD stage: %s\n", fileName.c_str());
    auto defaultPrim = stage->GetDefaultPrim();
    if (defaultPrim) {
      printf("[import_USD] Default prim: %s\n",
             defaultPrim.GetPath().GetString().c_str());
    } else {
      printf("[import_USD] No default prim set.\n");
    }
    size_t primCount = 0;
    for (auto _ : stage->Traverse())
      ++primCount;
    printf(
           "[import_USD] Number of prims in stage: %zu\n", primCount);
    // auto usd_root = scene.insertChildNode(
    //                                       location ? location : scene.defaultLayer()->root(), filepath);

    pxr::UsdGeomXformCache xformCache(pxr::UsdTimeCode::Default());

    // std::string basePath = pathOf(filepath);

    // Traverse all prims in the USD file, but only import top-level prims
    for (pxr::UsdPrim const &prim : stage->Traverse()) {
      // if (prim.IsPrototype()) continue;
      if (prim.GetParent() && prim.GetParent().IsPseudoRoot()) {
        import_usd_prim_recursive(scene,
                                  prim,
                                  // usd_root,
                                  xformCache);
      }
    }
    vec3f sceneMin = scene.bounds.lower;
    vec3f sceneMax = scene.bounds.upper;
    printf(
           "[import_USD] Scene bounds: min=(%f, %f, %f) max=(%f, %f, %f)\n",
           sceneMin.x,
           sceneMin.y,
           sceneMin.z,
           sceneMax.x,
           sceneMax.y,
           sceneMax.z);


    mini::Object::SP miniObj = mini::Object::create(scene.meshes);
    mini::Scene::SP miniScene = mini::Scene::create({mini::Instance::create(miniObj)});


    dataGroup.minis.push_back(miniScene);
  }

} // namespace tsd::io
