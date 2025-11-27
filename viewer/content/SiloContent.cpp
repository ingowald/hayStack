// ======================================================================== //
// Copyright 2022-2024 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "SiloContent.h"
#include "umesh/UMesh.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace hs {

  SiloContent::SiloContent(const std::string &fileName)
    : fileName(fileName),
      fileSize(getFileSize(fileName)),
      partID(-1)
  {
  }

  SiloContent::SiloContent(const ResourceSpecifier &resource, int partID)
    : fileName(resource.where),
      fileSize(getFileSize(resource.where)),
      partID(partID),
      requestedVar(resource.get("var", ""))
  {
  }

  SiloContent::~SiloContent()
  {
  }

  void SiloContent::create(DataLoader *loader,
                           const std::string &dataURL)
  {
    loader->addContent(new SiloContent(dataURL));
  }

  void SiloContent::create(DataLoader *loader,
                           const ResourceSpecifier &resource)
  {
    // Open file to check how many domains it has
    DBfile *dbfile = DBOpen(resource.where.c_str(), DB_UNKNOWN, DB_READ);
    if (!dbfile) {
      std::cerr << "#hs.silo: Warning - failed to open file to check domain count: " 
                << resource.where << std::endl;
      loader->addContent(new SiloContent(resource, 0));
      return;
    }
    
    DBtoc *toc = DBGetToc(dbfile);
    int numDomains = 0;
    
    // Check for multimeshes to determine number of domains
    if (toc && toc->nmultimesh > 0) {
      DBmultimesh *mm = DBGetMultimesh(dbfile, toc->multimesh_names[0]);
      if (mm) {
        numDomains = mm->nblocks;
        DBFreeMultimesh(mm);
      }
    }
    
    DBClose(dbfile);
    
    if (numDomains == 0) {
      // Single domain or couldn't determine - load as single
      loader->addContent(new SiloContent(resource, -1));
    } else if (resource.numParts > 1) {
      // Distribute domains across multiple data ranks
      int domainsPerRank = (numDomains + resource.numParts - 1) / resource.numParts;
      for (int rank = 0; rank < resource.numParts; rank++) {
        int startDomain = rank * domainsPerRank;
        int endDomain = std::min(startDomain + domainsPerRank, numDomains);
        
        // Create one SiloContent per domain for this rank
        for (int d = startDomain; d < endDomain; d++) {
          loader->addContent(new SiloContent(resource, d));
        }
      }
    } else {
      // Load all domains into single rank
      for (int i = 0; i < numDomains; i++) {
        loader->addContent(new SiloContent(resource, i));
      }
    }
  }
    
  std::string SiloContent::toString() 
  {
    std::string result = "Silo{fileName="+fileName;
    if (partID >= 0)
      result += ", part=" + std::to_string(partID);
    result += ", proj size " + prettyNumber(projectedSize())+"B}";
    return result;
  }
  
  size_t SiloContent::projectedSize() 
  { 
    return 2 * fileSize; 
  }
  
  void SiloContent::executeLoad(DataRank &dataRank, bool verbose) 
  {
    if (verbose)
      std::cout << "#hs.silo: Loading Silo file: " << fileName 
                << " (part " << partID << ")" << std::endl;
    
    // Open the Silo file
    DBfile *dbfile = DBOpen(fileName.c_str(), DB_UNKNOWN, DB_READ);
    if (!dbfile) {
      throw std::runtime_error("Failed to open Silo file: " + fileName);
    }

    // Get the table of contents
    DBtoc *toc = DBGetToc(dbfile);
    if (!toc) {
      DBClose(dbfile);
      throw std::runtime_error("Failed to get TOC from Silo file: " + fileName);
    }

    if (verbose) {
      std::cout << "#hs.silo: Found " << toc->nqmesh << " quadmeshes, "
                << toc->nucdmesh << " ucd meshes, "
                << toc->nptmesh << " point meshes, "
                << toc->nmultimesh << " multimeshes" << std::endl;
    }

    // Check if we have multimeshes (multi-domain datasets)
    if (toc->nmultimesh > 0 && partID >= 0) {
      // Load specific domain from multimesh
      for (int i = 0; i < toc->nmultimesh; i++) {
        DBmultimesh *mm = DBGetMultimesh(dbfile, toc->multimesh_names[i]);
        if (mm && mm->nblocks > 0 && partID < mm->nblocks) {
          if (verbose)
            std::cout << "#hs.silo: Loading domain " << partID 
                      << " from multimesh " << toc->multimesh_names[i] << std::endl;
          
          // Get the mesh name for this domain
          char *block_name = mm->meshnames[partID];
          int mesh_type = mm->meshtypes[partID];
          
          if (verbose)
            std::cout << "#hs.silo:   Block name: " << block_name 
                      << ", type: " << mesh_type << std::endl;
          
          // Handle cross-file references
          // Block names might be like "../p0/4320.silo:rectilinear_grid" (cross-file)
          // or "domain_00000/rectilinear_grid" (same file)
          std::string block_path(block_name);
          size_t colon_pos = block_path.find(':');
          
          DBfile *mesh_file = dbfile;
          bool opened_new_file = false;
          std::string mesh_name = block_path;
          
          if (colon_pos != std::string::npos) {
            // Cross-file reference: "path/to/file.silo:objectname"
            std::string ref_file_path = block_path.substr(0, colon_pos);
            mesh_name = block_path.substr(colon_pos + 1);
            
            // Make the path absolute relative to the root file
            std::string root_dir = fileName.substr(0, fileName.find_last_of('/') + 1);
            std::string full_path = root_dir + ref_file_path;
            
            if (verbose)
              std::cout << "#hs.silo:   Opening referenced file: " << full_path << std::endl;
            
            mesh_file = DBOpen(full_path.c_str(), DB_UNKNOWN, DB_READ);
            if (mesh_file) {
              opened_new_file = true;
            } else {
              if (verbose)
                std::cerr << "#hs.silo: Warning - failed to open referenced file: " 
                          << full_path << std::endl;
              mesh_file = dbfile; // Fall back to current file
            }
          } else {
            // Same-file reference - might need directory navigation
            size_t slash_pos = block_path.find_last_of('/');
            if (slash_pos != std::string::npos) {
              std::string dir_path = block_path.substr(0, slash_pos);
              mesh_name = block_path.substr(slash_pos + 1);
              
              if (DBSetDir(mesh_file, dir_path.c_str()) != 0) {
                if (verbose)
                  std::cerr << "#hs.silo: Warning - failed to change to directory: " 
                            << dir_path << std::endl;
              } else if (verbose) {
                std::cout << "#hs.silo:   Changed to directory: " << dir_path << std::endl;
              }
            }
          }
          
          // Read the mesh from the appropriate file
          if (mesh_type == DB_QUADMESH || mesh_type == DB_QUAD_RECT || mesh_type == DB_QUAD_CURV) {
            readQuadMesh(mesh_file, mesh_name.c_str(), dataRank, verbose);
          } else if (mesh_type == DB_UCDMESH) {
            readUcdMesh(mesh_file, mesh_name.c_str(), dataRank, verbose);
          } else if (mesh_type == DB_POINTMESH) {
            readPointMesh(mesh_file, mesh_name.c_str(), dataRank, verbose);
          } else {
            if (verbose)
              std::cout << "#hs.silo: Unknown mesh type: " << mesh_type << std::endl;
          }
          
          // Clean up
          if (opened_new_file) {
            DBClose(mesh_file);
          } else if (colon_pos == std::string::npos) {
            // Return to root directory if we changed it
            DBSetDir(dbfile, "/");
          }
        }
        if (mm) DBFreeMultimesh(mm);
      }
    } else {
      // Load all meshes (single domain or loading everything)
      // Read all quadmeshes (structured/rectilinear grids)
      for (int i = 0; i < toc->nqmesh; i++) {
        if (verbose)
          std::cout << "#hs.silo: Reading quadmesh: " << toc->qmesh_names[i] << std::endl;
        readQuadMesh(dbfile, toc->qmesh_names[i], dataRank, verbose);
      }

      // Read all UCD meshes (unstructured)
      for (int i = 0; i < toc->nucdmesh; i++) {
        if (verbose)
          std::cout << "#hs.silo: Reading ucdmesh: " << toc->ucdmesh_names[i] << std::endl;
        readUcdMesh(dbfile, toc->ucdmesh_names[i], dataRank, verbose);
      }

      // Read all point meshes
      for (int i = 0; i < toc->nptmesh; i++) {
        if (verbose)
          std::cout << "#hs.silo: Reading pointmesh: " << toc->ptmesh_names[i] << std::endl;
        readPointMesh(dbfile, toc->ptmesh_names[i], dataRank, verbose);
      }
    }

    DBClose(dbfile);
    
    if (verbose)
      std::cout << "#hs.silo: Finished loading Silo file" << std::endl;
  }

  void SiloContent::readQuadMesh(DBfile *dbfile, const char *meshName,
                                  DataRank &dataRank, bool verbose)
  {
    DBquadmesh *qm = DBGetQuadmesh(dbfile, meshName);
    if (!qm) {
      std::cerr << "#hs.silo: Warning - failed to read quadmesh: " << meshName << std::endl;
      return;
    }

    int ndims = qm->ndims;
    int *dims = qm->dims;
    
    if (verbose) {
      std::cout << "#hs.silo:   Quadmesh dimensions: ";
      for (int i = 0; i < ndims; i++)
        std::cout << dims[i] << " ";
      std::cout << std::endl;
    }

  // Check for ghost zones
  int min_index[3] = {0, 0, 0};
  int max_index[3] = {0, 0, 0};
  
  // Initialize based on actual number of dimensions
  for (int i = 0; i < ndims && i < 3; i++) {
    max_index[i] = dims[i] - 1;
  }
  
  bool hasGhostZones = false;
    
    if (qm->min_index && qm->max_index) {
      for (int i = 0; i < ndims; i++) {
        min_index[i] = qm->min_index[i];
        max_index[i] = qm->max_index[i];
        if (min_index[i] != 0 || max_index[i] != dims[i]-1) {
          hasGhostZones = true;
        }
      }
      
      if (hasGhostZones && verbose) {
        std::cout << "#hs.silo:   Ghost zones detected - real range: ["
                  << min_index[0] << ":" << max_index[0] << ", "
                  << min_index[1] << ":" << max_index[1] << ", "
                  << min_index[2] << ":" << max_index[2] << "]" << std::endl;
      }
    }

    // For 3D structured volumes
    if (ndims == 3) {
      // Compute dimensions of the REAL (non-ghost) portion
      vec3i volumeDims = vec3i(
        max_index[0] - min_index[0] + 1,
        max_index[1] - min_index[1] + 1,
        max_index[2] - min_index[2] + 1
      );
      
      vec3f gridOrigin = vec3f(0.f);
      vec3f gridSpacing = vec3f(1.f);
      
      // Determine if rectilinear or curvilinear
      if (qm->coordtype == DB_COLLINEAR) {
        // Rectilinear grid - compute bounds
        // Use the coordinates of the REAL (non-ghost) portion
        if (qm->datatype == DB_FLOAT) {
          float *x_coords = (float*)qm->coords[0];
          float *y_coords = (float*)qm->coords[1];
          float *z_coords = qm->coords[2] ? (float*)qm->coords[2] : nullptr;
          
          if (!x_coords || !y_coords || !z_coords) {
            if (verbose)
              std::cerr << "#hs.silo:   Error: NULL coordinate arrays" << std::endl;
            DBFreeQuadmesh(qm);
            return;
          }
          
          // Origin is at the start of the real zone region
          gridOrigin = vec3f(x_coords[min_index[0]], 
                            y_coords[min_index[1]], 
                            z_coords[min_index[2]]);
          
          // Spacing based on the real zones
          gridSpacing = vec3f(
            (x_coords[max_index[0]] - x_coords[min_index[0]]) / (max_index[0] - min_index[0]),
            (y_coords[max_index[1]] - y_coords[min_index[1]]) / (max_index[1] - min_index[1]),
            (z_coords[max_index[2]] - z_coords[min_index[2]]) / (max_index[2] - min_index[2])
          );
        } else if (qm->datatype == DB_DOUBLE) {
          double *x_coords = (double*)qm->coords[0];
          double *y_coords = (double*)qm->coords[1];
          double *z_coords = qm->coords[2] ? (double*)qm->coords[2] : nullptr;
          
          if (!x_coords || !y_coords || !z_coords) {
            if (verbose)
              std::cerr << "#hs.silo:   Error: NULL coordinate arrays" << std::endl;
            DBFreeQuadmesh(qm);
            return;
          }
          
          // Origin is at the start of the real zone region
          gridOrigin = vec3f(x_coords[min_index[0]], 
                            y_coords[min_index[1]], 
                            z_coords[min_index[2]]);
          
          // Spacing based on the real zones
          gridSpacing = vec3f(
            (x_coords[max_index[0]] - x_coords[min_index[0]]) / (max_index[0] - min_index[0]),
            (y_coords[max_index[1]] - y_coords[min_index[1]]) / (max_index[1] - min_index[1]),
            (z_coords[max_index[2]] - z_coords[min_index[2]]) / (max_index[2] - min_index[2])
          );
        }
        
        if (verbose) {
          std::cout << "#hs.silo:   Rectilinear grid - origin: " << gridOrigin
                    << ", spacing: " << gridSpacing << std::endl;
        }
      } else {
        // Curvilinear - would need more complex handling
        if (verbose)
          std::cout << "#hs.silo:   Curvilinear/Noncollinear grid detected (advanced)" << std::endl;
      }

      // Look for associated scalar variables
      std::vector<float> scalars;
      DBtoc *mesh_toc = DBGetToc(dbfile);
      if (!mesh_toc) {
        if (verbose)
          std::cerr << "#hs.silo: Warning - failed to get TOC from mesh file" << std::endl;
        DBFreeQuadmesh(qm);
        return;
      }
      
      for (int i = 0; i < mesh_toc->nqvar; i++) {
        const char *var_name = mesh_toc->qvar_names[i];
        
        // If a specific variable is requested, skip others
        if (!requestedVar.empty() && requestedVar != var_name) {
          continue;
        }
        
        DBquadvar *qv = DBGetQuadvar(dbfile, var_name);
        if (qv) {
          // Check if this variable matches the mesh (either by name or by dimensions)
          bool matches_mesh = false;
          if (qv->meshname) {
            matches_mesh = (strcmp(qv->meshname, meshName) == 0);
          }
          
          // Also check by dimensions if meshname doesn't match
          if (!matches_mesh && qv->ndims == ndims) {
            matches_mesh = true;
            for (int d = 0; d < ndims; d++) {
              if (qv->dims[d] != dims[d] && qv->dims[d] != dims[d]-1) {
                matches_mesh = false;
                break;
              }
            }
          }
          
          if (matches_mesh && scalars.empty()) {  // Only load first matching variable
            int nels = dims[0] * dims[1] * dims[2];
            
            // Check if zone-centered or node-centered
            int *var_dims = qv->dims;
            bool isNodeCentered = (var_dims[0] == dims[0] && 
                                    var_dims[1] == dims[1] && 
                                    var_dims[2] == dims[2]);
            
            if (isNodeCentered || scalars.empty()) {  // Accept zone-centered too if needed
              // Only process scalar variables (nvals should be 1)
              if (qv->nvals != 1) {
                if (verbose)
                  std::cout << "#hs.silo:   Skipping vector variable: " << var_name 
                            << " (nvals=" << qv->nvals << ")" << std::endl;
              } else {
                // Allocate and copy scalar data
                int var_nels = var_dims[0] * var_dims[1] * var_dims[2];
                
                // Check that we're reading the right amount of data
                if (qv->nels != var_nels) {
                  if (verbose)
                    std::cerr << "#hs.silo:   Warning: variable nels (" << qv->nels 
                              << ") doesn't match computed size (" << var_nels << ")" << std::endl;
                }
                
                // For variables with ghost zones, check if the variable itself
                // specifies min/max indices. If not, load the entire variable.
                int var_min[3] = {0, 0, 0};
                int var_max[3] = {var_dims[0]-1, var_dims[1]-1, var_dims[2]-1};
                
                // Check if variable has its own ghost zone specification
                if (qv->min_index && qv->max_index) {
                  for (int d = 0; d < 3 && d < qv->ndims; d++) {
                    var_min[d] = qv->min_index[d];
                    var_max[d] = qv->max_index[d];
                  }
                  if (verbose) {
                    std::cout << "#hs.silo:   Variable has ghost zones - extracting real range: ["
                              << var_min[0] << ":" << var_max[0] << ", "
                              << var_min[1] << ":" << var_max[1] << ", "
                              << var_min[2] << ":" << var_max[2] << "]" << std::endl;
                  }
                }
                
                // Calculate size of data portion to extract
                int real_dims[3] = {
                  var_max[0] - var_min[0] + 1,
                  var_max[1] - var_min[1] + 1,
                  var_max[2] - var_min[2] + 1
                };
                int real_nels = real_dims[0] * real_dims[1] * real_dims[2];
                scalars.resize(real_nels);
                
                // Extract the data portion
                if (qv->datatype == DB_FLOAT) {
                  if (!qv->vals || !qv->vals[0]) {
                    if (verbose)
                      std::cerr << "#hs.silo:   Error: NULL variable data" << std::endl;
                    DBFreeQuadvar(qv);
                    continue;
                  }
                  float *data = (float*)qv->vals[0];
                  if (data) {
                    int idx = 0;
                    for (int k = var_min[2]; k <= var_max[2]; k++) {
                      for (int j = var_min[1]; j <= var_max[1]; j++) {
                        for (int i = var_min[0]; i <= var_max[0]; i++) {
                          int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
                          // Bounds check to prevent crash
                          if (src_idx >= 0 && src_idx < var_nels) {
                            scalars[idx++] = data[src_idx];
                          } else {
                            if (verbose)
                              std::cerr << "#hs.silo:   Warning: src_idx " << src_idx 
                                        << " out of bounds [0," << var_nels << ") at ("
                                        << i << "," << j << "," << k << ")" << std::endl;
                            scalars[idx++] = 0.0f; // Fill with zero
                          }
                        }
                      }
                    }
                  }
                } else if (qv->datatype == DB_DOUBLE) {
                  if (!qv->vals || !qv->vals[0]) {
                    if (verbose)
                      std::cerr << "#hs.silo:   Error: NULL variable data" << std::endl;
                    DBFreeQuadvar(qv);
                    continue;
                  }
                  double *data = (double*)qv->vals[0];
                  if (data) {
                    int idx = 0;
                    for (int k = var_min[2]; k <= var_max[2]; k++) {
                      for (int j = var_min[1]; j <= var_max[1]; j++) {
                        for (int i = var_min[0]; i <= var_max[0]; i++) {
                          int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
                          // Bounds check to prevent crash
                          if (src_idx >= 0 && src_idx < var_nels) {
                            scalars[idx++] = (float)data[src_idx];
                          } else {
                            if (verbose)
                              std::cerr << "#hs.silo:   Warning: src_idx " << src_idx 
                                        << " out of bounds [0," << var_nels << ") at ("
                                        << i << "," << j << "," << k << ")" << std::endl;
                            scalars[idx++] = 0.0f; // Fill with zero
                          }
                        }
                      }
                    }
                  }
                } else {
                  if (verbose)
                    std::cerr << "#hs.silo:   Unsupported data type: " << qv->datatype << std::endl;
                }
                
                if (verbose) {
                  float min_val = scalars[0], max_val = scalars[0];
                  for (float v : scalars) {
                    if (v < min_val) min_val = v;
                    if (v > max_val) max_val = v;
                  }
                  std::cout << "#hs.silo:   Loaded scalar variable: " << var_name 
                            << " (" << var_nels << " elements, range: [" << min_val << ":" << max_val << "])" << std::endl;
                }
                
                // If we found the requested variable, stop looking
                if (!requestedVar.empty()) {
                  break;
                }
              }
            }
          }
        }
        if (qv) DBFreeQuadvar(qv);
      }

      if (verbose && mesh_toc->nqvar > 0) {
        std::cout << "#hs.silo:   Found " << mesh_toc->nqvar << " quad variables" << std::endl;
      }

      if (!scalars.empty()) {
        // Determine actual volume dimensions from scalar count
        // Could be zone-centered (dims-1) or node-centered (dims)
        vec3i actualDims = volumeDims;
        size_t expected_node = volumeDims.x * volumeDims.y * volumeDims.z;
        size_t expected_zone = (volumeDims.x-1) * (volumeDims.y-1) * (volumeDims.z-1);
        
        if (scalars.size() == expected_zone) {
          // Zone-centered data
          actualDims = vec3i(volumeDims.x-1, volumeDims.y-1, volumeDims.z-1);
          if (verbose)
            std::cout << "#hs.silo:   Zone-centered data, adjusted dims to " << actualDims << std::endl;
        } else if (scalars.size() != expected_node) {
          if (verbose)
            std::cerr << "#hs.silo:   Warning: scalar count " << scalars.size() 
                      << " doesn't match expected node (" << expected_node 
                      << ") or zone (" << expected_zone << ") count" << std::endl;
        }
        
        // Convert scalars to raw data
        std::vector<uint8_t> rawData(scalars.size() * sizeof(float));
        memcpy(rawData.data(), scalars.data(), rawData.size());
        
        std::vector<uint8_t> rawDataRGB; // Empty for now
        
        StructuredVolume::SP vol = std::make_shared<StructuredVolume>(
          actualDims, "float", rawData, rawDataRGB, gridOrigin, gridSpacing);
        
        dataRank.structuredVolumes.push_back(vol);
        if (verbose)
          std::cout << "#hs.silo:   Added structured volume with " << scalars.size() << " scalars" << std::endl;
      } else {
        // No scalars found - create a dummy volume so we can at least see the grid
        if (verbose)
          std::cout << "#hs.silo:   No scalars found, creating volume with dummy data" << std::endl;
        
        int nels = dims[0] * dims[1] * dims[2];
        scalars.resize(nels);
        // Create some test data based on position
        for (int k = 0; k < dims[2]; k++) {
          for (int j = 0; j < dims[1]; j++) {
            for (int i = 0; i < dims[0]; i++) {
              int idx = i + dims[0] * (j + dims[1] * k);
              // Simple gradient for visualization
              scalars[idx] = (float)i / dims[0];
            }
          }
        }
        
        std::vector<uint8_t> rawData(scalars.size() * sizeof(float));
        memcpy(rawData.data(), scalars.data(), rawData.size());
        
        std::vector<uint8_t> rawDataRGB;
        
        StructuredVolume::SP vol = std::make_shared<StructuredVolume>(
          volumeDims, "float", rawData, rawDataRGB, gridOrigin, gridSpacing);
        
        dataRank.structuredVolumes.push_back(vol);
        if (verbose)
          std::cout << "#hs.silo:   Added structured volume with dummy data" << std::endl;
      }
    }

    DBFreeQuadmesh(qm);
  }

  void SiloContent::readUcdMesh(DBfile *dbfile, const char *meshName,
                                 DataRank &dataRank, bool verbose)
  {
    DBucdmesh *um = DBGetUcdmesh(dbfile, meshName);
    if (!um) {
      std::cerr << "#hs.silo: Warning - failed to read ucdmesh: " << meshName << std::endl;
      return;
    }

    int nnodes = um->nnodes;
    int nzones = um->zones ? um->zones->nzones : 0;
    
    if (verbose) {
      std::cout << "#hs.silo:   UCD mesh: " << nnodes << " nodes, " 
                << nzones << " zones" << std::endl;
    }

    // Create UMesh
    umesh::UMesh::SP mesh = std::make_shared<umesh::UMesh>();
    
    // Read vertices
    mesh->vertices.resize(nnodes);
    if (um->datatype == DB_FLOAT) {
      float *x = (float*)um->coords[0];
      float *y = (float*)um->coords[1];
      float *z = um->ndims >= 3 ? (float*)um->coords[2] : nullptr;
      
      for (int i = 0; i < nnodes; i++) {
        mesh->vertices[i].x = x[i];
        mesh->vertices[i].y = y[i];
        mesh->vertices[i].z = z ? z[i] : 0.f;
      }
    } else if (um->datatype == DB_DOUBLE) {
      double *x = (double*)um->coords[0];
      double *y = (double*)um->coords[1];
      double *z = um->ndims >= 3 ? (double*)um->coords[2] : nullptr;
      
      for (int i = 0; i < nnodes; i++) {
        mesh->vertices[i].x = (float)x[i];
        mesh->vertices[i].y = (float)y[i];
        mesh->vertices[i].z = z ? (float)z[i] : 0.f;
      }
    }

    // Read connectivity (zonelist)
    if (um->zones && nzones > 0) {
      DBzonelist *zl = um->zones;
      int origin = zl->origin;
      int *nodelist = zl->nodelist;
      
      for (int i = 0; i < zl->nshapes; i++) {
        int shapetype = zl->shapetype[i];
        int shapesize = zl->shapesize[i];
        int shapecnt = zl->shapecnt[i];
        
        for (int j = 0; j < shapecnt; j++) {
          // Map Silo zone types to UMesh element types
          if (shapetype == DB_ZONETYPE_TET) {
            // Tetrahedron - 4 nodes
            umesh::vec4i tet;
            tet.x = nodelist[0] - origin;
            tet.y = nodelist[1] - origin;
            tet.z = nodelist[2] - origin;
            tet.w = nodelist[3] - origin;
            mesh->tets.push_back(tet);
            nodelist += 4;
          } 
          else if (shapetype == DB_ZONETYPE_HEX) {
            // Hexahedron - 8 nodes
            umesh::Hex hex(nodelist[0] - origin,
                           nodelist[1] - origin,
                           nodelist[2] - origin,
                           nodelist[3] - origin,
                           nodelist[4] - origin,
                           nodelist[5] - origin,
                           nodelist[6] - origin,
                           nodelist[7] - origin);
            mesh->hexes.push_back(hex);
            nodelist += 8;
          }
          else if (shapetype == DB_ZONETYPE_PRISM) {
            // Wedge/Prism - 6 nodes
            umesh::Wedge wedge(nodelist[0] - origin,
                               nodelist[1] - origin,
                               nodelist[2] - origin,
                               nodelist[3] - origin,
                               nodelist[4] - origin,
                               nodelist[5] - origin);
            mesh->wedges.push_back(wedge);
            nodelist += 6;
          }
          else if (shapetype == DB_ZONETYPE_PYRAMID) {
            // Pyramid - 5 nodes
            umesh::Pyr pyr(nodelist[0] - origin,
                           nodelist[1] - origin,
                           nodelist[2] - origin,
                           nodelist[3] - origin,
                           nodelist[4] - origin);
            mesh->pyrs.push_back(pyr);
            nodelist += 5;
          }
          else {
            // Skip unknown element types
            nodelist += shapesize;
          }
        }
      }
      
      if (verbose) {
        std::cout << "#hs.silo:   Loaded " << mesh->tets.size() << " tets, "
                  << mesh->hexes.size() << " hexes, "
                  << mesh->wedges.size() << " wedges, "
                  << mesh->pyrs.size() << " pyramids" << std::endl;
      }
    }

    // Look for associated variables
    DBtoc *toc = DBGetToc(dbfile);
    for (int i = 0; i < toc->nucdvar; i++) {
      DBucdvar *uv = DBGetUcdvar(dbfile, toc->ucdvar_names[i]);
      if (uv && uv->meshname && strcmp(uv->meshname, meshName) == 0) {
        // This variable is associated with this mesh
        if (uv->centering == DB_NODECENT && uv->nvals == nnodes) {
          // Node-centered scalar variable
          if (!mesh->perVertex)
            mesh->perVertex = std::make_shared<umesh::Attribute>();
          
          mesh->perVertex->values.resize(nnodes);
          
          if (uv->datatype == DB_FLOAT) {
            float *data = (float*)uv->vals[0];
            for (int j = 0; j < nnodes; j++)
              mesh->perVertex->values[j] = data[j];
          } else if (uv->datatype == DB_DOUBLE) {
            double *data = (double*)uv->vals[0];
            for (int j = 0; j < nnodes; j++)
              mesh->perVertex->values[j] = (float)data[j];
          }
          
          if (verbose)
            std::cout << "#hs.silo:   Loaded node-centered variable: " << toc->ucdvar_names[i] << std::endl;
        }
      }
      if (uv) DBFreeUcdvar(uv);
    }

    // Add to data rank if we have valid geometry
    if (mesh->vertices.size() > 0 && 
        (mesh->tets.size() > 0 || mesh->hexes.size() > 0 || 
         mesh->wedges.size() > 0 || mesh->pyrs.size() > 0)) {
      dataRank.unsts.push_back({mesh, box3f()});
      if (verbose)
        std::cout << "#hs.silo:   Added unstructured mesh" << std::endl;
    }

    DBFreeUcdmesh(um);
  }

  void SiloContent::readPointMesh(DBfile *dbfile, const char *meshName,
                                   DataRank &dataRank, bool verbose)
  {
    DBpointmesh *pm = DBGetPointmesh(dbfile, meshName);
    if (!pm) {
      std::cerr << "#hs.silo: Warning - failed to read pointmesh: " << meshName << std::endl;
      return;
    }

    int npts = pm->nels;
    
    if (verbose) {
      std::cout << "#hs.silo:   Point mesh: " << npts << " points" << std::endl;
    }

    // Create a sphere set for point mesh
    SphereSet::SP spheres = std::make_shared<SphereSet>();
    spheres->origins.resize(npts);
    spheres->radii.resize(npts);
    
    // Read positions
    if (pm->datatype == DB_FLOAT) {
      float *x = (float*)pm->coords[0];
      float *y = (float*)pm->coords[1];
      float *z = pm->ndims >= 3 ? (float*)pm->coords[2] : nullptr;
      
      for (int i = 0; i < npts; i++) {
        spheres->origins[i].x = x[i];
        spheres->origins[i].y = y[i];
        spheres->origins[i].z = z ? z[i] : 0.f;
        spheres->radii[i] = 0.01f; // Default radius
      }
    } else if (pm->datatype == DB_DOUBLE) {
      double *x = (double*)pm->coords[0];
      double *y = (double*)pm->coords[1];
      double *z = pm->ndims >= 3 ? (double*)pm->coords[2] : nullptr;
      
      for (int i = 0; i < npts; i++) {
        spheres->origins[i].x = (float)x[i];
        spheres->origins[i].y = (float)y[i];
        spheres->origins[i].z = z ? (float)z[i] : 0.f;
        spheres->radii[i] = 0.01f; // Default radius
      }
    }

    // Look for associated scalar variables (could be used for coloring)
    // Note: DBtoc doesn't have pvar_names, point variables are typically 
    // accessed differently in Silo. For now, we'll just create spheres with default colors.
    
    // Set default colors based on position (for visualization)
    spheres->colors.resize(npts);
    for (int i = 0; i < npts; i++) {
      spheres->colors[i] = vec3f(0.5f, 0.5f, 1.0f); // Default blue color
    }

    dataRank.sphereSets.push_back(spheres);
    
    DBFreePointmesh(pm);
    
    if (verbose)
      std::cout << "#hs.silo:   Added point mesh as sphere set" << std::endl;
  }

}

