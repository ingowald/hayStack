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
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>

namespace hs {

  // ============================================================================
  // Lambda2 criterion computation functions
  // ============================================================================
  
  // Compute gradient in X direction (one-sided differences at boundaries)
  static void computeGradX(const std::vector<float>& u, std::vector<float>& uGrad, 
                           int nx, int ny, int nz, float dx)
  {
    for (int z = 0; z < nz; ++z) {
      size_t off = z * nx * ny;
      for (int row = 0; row < ny; ++row) {
        // One-sided forward difference at lower boundary
        uGrad[off + row*nx] = (u[off + row*nx + 1] - u[off + row*nx]) / dx;
        // One-sided backward difference at upper boundary
        uGrad[off + row*nx + nx - 1] = (u[off + row*nx + nx - 1] - u[off + row*nx + nx - 2]) / dx;
        // Interior points (central difference)
        for (int col = 1; col < nx - 1; ++col) {
          uGrad[off + row*nx + col] = 0.5f * (u[off + row*nx + col + 1] - u[off + row*nx + col - 1]) / dx;
        }
      }
    }
  }
  
  // Compute gradient in Y direction (one-sided differences at boundaries)
  static void computeGradY(const std::vector<float>& v, std::vector<float>& vGrad,
                           int nx, int ny, int nz, float dy)
  {
    for (int z = 0; z < nz; ++z) {
      size_t off = z * nx * ny;
      for (int col = 0; col < nx; ++col) {
        // One-sided forward difference at lower boundary
        vGrad[0*nx + col + off] = (v[1*nx + col + off] - v[0*nx + col + off]) / dy;
        // One-sided backward difference at upper boundary
        vGrad[(ny-1)*nx + col + off] = (v[(ny-1)*nx + col + off] - v[(ny-2)*nx + col + off]) / dy;
        // Interior points (central difference)
        for (int row = 1; row < ny - 1; ++row) {
          vGrad[row*nx + col + off] = 0.5f * (v[(row+1)*nx + col + off] - v[(row-1)*nx + col + off]) / dy;
        }
      }
    }
  }
  
  // Compute gradient in Z direction (non-uniform spacing)
  static void computeGradZ(const std::vector<float>& w, std::vector<float>& wGrad,
                           int nx, int ny, int nz, const std::vector<float>& z)
  {
    size_t off = nx * ny;
    for (int row = 0; row < ny; ++row) {
      for (int col = 0; col < nx; ++col) {
        // Boundaries (forward/backward difference)
        wGrad[(nz-1)*off + row*nx + col] = (w[(nz-1)*off + row*nx + col] - w[(nz-2)*off + row*nx + col]) / (z[nz-1] - z[nz-2]);
        wGrad[0*off + row*nx + col] = (w[1*off + row*nx + col] - w[0*off + row*nx + col]) / (z[1] - z[0]);
        // Interior points (central difference)
        for (int zi = 1; zi < nz - 1; ++zi) {
          wGrad[zi*off + row*nx + col] = 0.5f * (w[(zi+1)*off + row*nx + col] - w[(zi-1)*off + row*nx + col]) / (z[zi+1] - z[zi-1]);
        }
      }
    }
  }
  
  // Compute all 9 velocity gradients
  static void computeVelocityGradients(
    const std::vector<float>& vel1, const std::vector<float>& vel2, const std::vector<float>& vel3,
    int nx, int ny, int nz, float dx, float dy, const std::vector<float>& z,
    std::vector<float>& dux, std::vector<float>& duy, std::vector<float>& duz,
    std::vector<float>& dvx, std::vector<float>& dvy, std::vector<float>& dvz,
    std::vector<float>& dwx, std::vector<float>& dwy, std::vector<float>& dwz)
  {
    computeGradX(vel1, dux, nx, ny, nz, dx);
    computeGradX(vel2, dvx, nx, ny, nz, dx);
    computeGradX(vel3, dwx, nx, ny, nz, dx);
    
    computeGradY(vel1, duy, nx, ny, nz, dy);
    computeGradY(vel2, dvy, nx, ny, nz, dy);
    computeGradY(vel3, dwy, nx, ny, nz, dy);
    
    computeGradZ(vel1, duz, nx, ny, nz, z);
    computeGradZ(vel2, dvz, nx, ny, nz, z);
    computeGradZ(vel3, dwz, nx, ny, nz, z);
  }
  
  // Compute lambda2 criterion from velocity gradients
  static void computeLambda2(const std::vector<float>& dux, const std::vector<float>& duy, const std::vector<float>& duz,
                             const std::vector<float>& dvx, const std::vector<float>& dvy, const std::vector<float>& dvz,
                             const std::vector<float>& dwx, const std::vector<float>& dwy, const std::vector<float>& dwz,
                             std::vector<float>& result)
  {
    size_t len = dux.size();
    for (size_t i = 0; i < len; ++i) {
      // Strain rate tensor S = 0.5*(J + J^T)
      float s11 = dux[i];
      float s12 = 0.5f * (duy[i] + dvx[i]);
      float s13 = 0.5f * (duz[i] + dwx[i]);
      float s22 = dvy[i];
      float s23 = 0.5f * (dvz[i] + dwy[i]);
      float s33 = dwz[i];
      
      // Antisymmetric part Omega = 0.5*(J - J^T)
      float o12 = 0.5f * (duy[i] - dvx[i]);
      float o13 = 0.5f * (duz[i] - dwx[i]);
      float o23 = 0.5f * (dvz[i] - dwy[i]);
      
      // S^2 + Omega^2
      float m11 = s11*s11 + s12*s12 + s13*s13 - o12*o12 - o13*o13;
      float m12 = s11*s12 + s12*s22 + s13*s23 + o12*(s11 - s22) + o13*o23;
      float m13 = s11*s13 + s12*s23 + s13*s33 + o13*(s11 - s33) - o12*o23;
      float m22 = s12*s12 + s22*s22 + s23*s23 - o12*o12 - o23*o23;
      float m23 = s12*s13 + s22*s23 + s23*s33 + o23*(s22 - s33) + o12*o13;
      float m33 = s13*s13 + s23*s23 + s33*s33 - o13*o13 - o23*o23;
      
      // Compute eigenvalues of 3x3 symmetric matrix
      float p1 = m12*m12 + m13*m13 + m23*m23;
      float q = (m11 + m22 + m33) / 3.0f;
      float p2 = (m11 - q)*(m11 - q) + (m22 - q)*(m22 - q) + (m33 - q)*(m33 - q) + 2.0f*p1;
      float p = std::sqrt(p2 / 6.0f);
      
      float b11 = (m11 - q) / p;
      float b12 = m12 / p;
      float b13 = m13 / p;
      float b22 = (m22 - q) / p;
      float b23 = m23 / p;
      float b33 = (m33 - q) / p;
      
      float r = (b11*(b22*b33 - b23*b23) - b12*(b12*b33 - b23*b13) + b13*(b12*b23 - b22*b13)) / 2.0f;
      r = std::max(-1.0f, std::min(1.0f, r));
      
      float phi = std::acos(r) / 3.0f;
      float eig1 = q + 2.0f*p*std::cos(phi);
      float eig3 = q + 2.0f*p*std::cos(phi + (2.0f*M_PI/3.0f));
      float eig2 = 3.0f*q - eig1 - eig3; // middle eigenvalue
      
      result[i] = -std::min(eig2, 0.0f);
    }
  }
  
  // Mask boundary faces with zeros to hide artifacts from boundary conditions
  // The mask_faces array indicates which faces to mask: [minX, maxX, minY, maxY, minZ, maxZ]
  static void maskBoundaryFaces(std::vector<float>& data, int nx, int ny, int nz, 
                                 bool mask_faces[6])
  {
    const int mask_depth = 10;  // Number of cells to mask on each face
    
    // Mask X faces if indicated
    if (mask_faces[0] || mask_faces[1]) {
      for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
          // Lower X face (i = 0 to mask_depth-1)
          if (mask_faces[0]) {
            for (int i = 0; i < std::min(mask_depth, nx); ++i) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
          // Upper X face (i = nx-mask_depth to nx-1)
          if (mask_faces[1]) {
            for (int i = std::max(0, nx - mask_depth); i < nx; ++i) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
        }
      }
    }
    
    // Mask Y faces if indicated
    if (mask_faces[2] || mask_faces[3]) {
      for (int k = 0; k < nz; ++k) {
        for (int i = 0; i < nx; ++i) {
          // Lower Y face (j = 0 to mask_depth-1)
          if (mask_faces[2]) {
            for (int j = 0; j < std::min(mask_depth, ny); ++j) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
          // Upper Y face (j = ny-mask_depth to ny-1)
          if (mask_faces[3]) {
            for (int j = std::max(0, ny - mask_depth); j < ny; ++j) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
        }
      }
    }
    
    // Mask Z faces if indicated
    if (mask_faces[4] || mask_faces[5]) {
      for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
          // Lower Z face (k = 0 to mask_depth-1)
          if (mask_faces[4]) {
            for (int k = 0; k < std::min(mask_depth, nz); ++k) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
          // Upper Z face (k = nz-mask_depth to nz-1)
          if (mask_faces[5]) {
            for (int k = std::max(0, nz - mask_depth); k < nz; ++k) {
              data[i + j*nx + k*nx*ny] = 0.0f;
            }
          }
        }
      }
    }
  }

  // Helper function to load a specific scalar variable from a quad mesh
  static bool loadQuadVar(DBfile *dbfile, 
                          const char *varName,
                          const char *meshName,
                          int ndims,
                          int *dims,
                          int *min_index,
                          int *max_index,
                          std::vector<float> &scalars,
                          bool verbose)
  {
    DBquadvar *qv = DBGetQuadvar(dbfile, varName);
    if (!qv) {
      if (verbose)
        std::cerr << "#hs.silo: Warning - failed to read variable: " << varName << std::endl;
      return false;
    }

    // Check if this variable matches the mesh dimensions
    bool matches_mesh = false;
    if (qv->meshname) {
      matches_mesh = (strcmp(qv->meshname, meshName) == 0);
    }
    
    if (!matches_mesh && qv->ndims == ndims) {
      matches_mesh = true;
      for (int d = 0; d < ndims; d++) {
        if (qv->dims[d] != dims[d] && qv->dims[d] != dims[d]-1) {
          matches_mesh = false;
          break;
        }
      }
    }
    
    if (!matches_mesh) {
      if (verbose)
        std::cerr << "#hs.silo: Variable " << varName << " doesn't match mesh" << std::endl;
      DBFreeQuadvar(qv);
      return false;
    }
    
    if (qv->nvals != 1) {
      if (verbose)
        std::cout << "#hs.silo: Skipping vector variable: " << varName 
                  << " (nvals=" << qv->nvals << ")" << std::endl;
      DBFreeQuadvar(qv);
      return false;
    }
    
    // Determine the real data range based on centering
    int *var_dims = qv->dims;
    int var_min[3], var_max[3];
    for (int d = 0; d < 3; d++) {
      if (var_dims[d] == dims[d]) {
        // Node-centered
        var_min[d] = min_index[d];
        var_max[d] = max_index[d];
      } else if (var_dims[d] == dims[d] - 1) {
        // Zone-centered
        var_min[d] = min_index[d];
        var_max[d] = max_index[d];
      } else {
        var_min[d] = 0;
        var_max[d] = var_dims[d] - 1;
      }
    }
    
    // Calculate dimensions of real data
    int real_dims[3] = {
      var_max[0] - var_min[0] + 1,
      var_max[1] - var_min[1] + 1,
      var_max[2] - var_min[2] + 1
    };
    int real_nels = real_dims[0] * real_dims[1] * real_dims[2];
    scalars.resize(real_nels);
    
    // Extract the data
    int total_var_els = var_dims[0] * var_dims[1] * var_dims[2];
    
    if (qv->datatype == DB_FLOAT) {
      float *data = (float*)qv->vals[0];
      if (data) {
        int idx = 0;
        for (int k = var_min[2]; k <= var_max[2]; k++) {
          for (int j = var_min[1]; j <= var_max[1]; j++) {
            for (int i = var_min[0]; i <= var_max[0]; i++) {
              int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
              if (src_idx >= 0 && src_idx < total_var_els) {
                scalars[idx++] = data[src_idx];
              }
            }
          }
        }
      }
    } else if (qv->datatype == DB_DOUBLE) {
      double *data = (double*)qv->vals[0];
      if (data) {
        int idx = 0;
        for (int k = var_min[2]; k <= var_max[2]; k++) {
          for (int j = var_min[1]; j <= var_max[1]; j++) {
            for (int i = var_min[0]; i <= var_max[0]; i++) {
              int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
              if (src_idx >= 0 && src_idx < total_var_els) {
                scalars[idx++] = (float)data[src_idx];
              }
            }
          }
        }
      }
    } else {
      if (verbose)
        std::cerr << "#hs.silo: Unsupported data type: " << qv->datatype << std::endl;
      DBFreeQuadvar(qv);
      return false;
    }
    
    DBFreeQuadvar(qv);
    
    if (verbose) {
      float min_val = scalars[0], max_val = scalars[0];
      for (float v : scalars) {
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
      }
      std::cout << "#hs.silo: Loaded variable: " << varName 
                << " (" << real_nels << " elements, range: [" 
                << min_val << ":" << max_val << "])" << std::endl;
    }
    
    return true;
  }

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
        std::cout << "#hs.silo: Found " << numDomains << " domains in multimesh '" 
                  << toc->multimesh_names[0] << "'" << std::endl;
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
    int max_index[3] = {dims[0]-1, dims[1]-1, dims[2]-1};
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
          float *z_coords = (float*)qm->coords[2];
          
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
          double *z_coords = (double*)qm->coords[2];
          
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
          std::cout << "#hs.silo:   Domain " << partID << " grid spacing calculation:" << std::endl;
          std::cout << "#hs.silo:     min_index = (" << min_index[0] << ", " << min_index[1] << ", " << min_index[2] << ")" << std::endl;
          std::cout << "#hs.silo:     max_index = (" << max_index[0] << ", " << max_index[1] << ", " << max_index[2] << ")" << std::endl;
          std::cout << "#hs.silo:     Index deltas = (" 
                    << (max_index[0] - min_index[0]) << ", "
                    << (max_index[1] - min_index[1]) << ", "
                    << (max_index[2] - min_index[2]) << ")" << std::endl;
          if (qm->datatype == DB_FLOAT) {
            float *x_coords = (float*)qm->coords[0];
            float *y_coords = (float*)qm->coords[1];
            float *z_coords = (float*)qm->coords[2];
            std::cout << "#hs.silo:     Coord deltas (float) = ("
                      << (x_coords[max_index[0]] - x_coords[min_index[0]]) << ", "
                      << (y_coords[max_index[1]] - y_coords[min_index[1]]) << ", "
                      << (z_coords[max_index[2]] - z_coords[min_index[2]]) << ")" << std::endl;
          } else if (qm->datatype == DB_DOUBLE) {
            double *x_coords = (double*)qm->coords[0];
            double *y_coords = (double*)qm->coords[1];
            double *z_coords = (double*)qm->coords[2];
            std::cout << "#hs.silo:     Coord deltas (double) = ("
                      << (x_coords[max_index[0]] - x_coords[min_index[0]]) << ", "
                      << (y_coords[max_index[1]] - y_coords[min_index[1]]) << ", "
                      << (z_coords[max_index[2]] - z_coords[min_index[2]]) << ")" << std::endl;
          }
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
      
      // Check if this is a derived field (lambda2 or vel_mag)
      if (requestedVar == "lambda2" || requestedVar == "vel_mag") {
        if (verbose)
          std::cout << "#hs.silo: Computing derived field: " << requestedVar << std::endl;
        
        // Load the three velocity fields
        std::vector<float> vel1_f, vel2_f, vel3_f;
        bool success = true;
        
        success &= loadQuadVar(dbfile, "vel1", meshName, ndims, dims, min_index, max_index, vel1_f, verbose);
        success &= loadQuadVar(dbfile, "vel2", meshName, ndims, dims, min_index, max_index, vel2_f, verbose);
        success &= loadQuadVar(dbfile, "vel3", meshName, ndims, dims, min_index, max_index, vel3_f, verbose);
        
        if (success && vel1_f.size() == vel2_f.size() && vel2_f.size() == vel3_f.size()) {
          
          if (requestedVar == "vel_mag") {
            // Compute velocity magnitude: sqrt(vel1^2 + vel2^2 + vel3^2)
            scalars.resize(vel1_f.size());
            for (size_t i = 0; i < vel1_f.size(); i++) {
              scalars[i] = std::sqrt(vel1_f[i]*vel1_f[i] + 
                                     vel2_f[i]*vel2_f[i] + 
                                     vel3_f[i]*vel3_f[i]);
            }
            
            if (verbose) {
              float min_val = scalars[0], max_val = scalars[0];
              for (float v : scalars) {
                if (v < min_val) min_val = v;
                if (v > max_val) max_val = v;
              }
              std::cout << "#hs.silo: Velocity magnitude computed (range: [" 
                        << min_val << ":" << max_val << "])" << std::endl;
            }
          } 
          else if (requestedVar == "lambda2") {
            // Extract coordinate arrays for gradient computation
            std::vector<float> z_coords;
            
            // Grid dimensions
            int nx_actual = max_index[0] - min_index[0] + 1;
            int ny_actual = max_index[1] - min_index[1] + 1;
            int nz_actual = max_index[2] - min_index[2] + 1;
            
            // Extract z coordinates for non-uniform spacing
            int z_count = max_index[2] - min_index[2] + 1;
            z_coords.resize(z_count);
            
            if (qm->coordtype == DB_COLLINEAR) {
              if (qm->datatype == DB_FLOAT) {
                float *z = (float*)qm->coords[2];
                for (int k = 0; k < z_count; k++) z_coords[k] = z[min_index[2] + k];
              } else if (qm->datatype == DB_DOUBLE) {
                double *z = (double*)qm->coords[2];
                for (int k = 0; k < z_count; k++) z_coords[k] = (float)z[min_index[2] + k];
              }
            } else {
              // For non-collinear grids, create uniform spacing
              for (int k = 0; k < z_count; k++) {
                z_coords[k] = gridOrigin.z + k * gridSpacing.z;
              }
            }
            
            // Get dx, dy from grid spacing
            float dx = gridSpacing.x;
            float dy = gridSpacing.y;
            
            size_t len = vel1_f.size();
            
            // Allocate gradient arrays
            std::vector<float> dux(len), duy(len), duz(len);
            std::vector<float> dvx(len), dvy(len), dvz(len);
            std::vector<float> dwx(len), dwy(len), dwz(len);
            
            if (verbose)
              std::cout << "#hs.silo: Computing velocity gradients (dx=" << dx << ", dy=" << dy << ")..." << std::endl;
            
            // Compute all 9 velocity gradients
            computeVelocityGradients(vel1_f, vel2_f, vel3_f,
                                     nx_actual, ny_actual, nz_actual,
                                     dx, dy, z_coords,
                                     dux, duy, duz,
                                     dvx, dvy, dvz,
                                     dwx, dwy, dwz);
            
            if (verbose)
              std::cout << "#hs.silo: Computing lambda2..." << std::endl;
            
            // Compute lambda2
            scalars.resize(len);
            computeLambda2(dux, duy, duz, dvx, dvy, dvz, dwx, dwy, dwz, scalars);
            
            // Determine which faces are on the global domain boundary
            // Faces without ghost zones (min_index == 0 or max_index == dims-1) are boundary faces
            bool mask_faces[6] = {false, false, false, false, false, false};
            // [0] = minX, [1] = maxX, [2] = minY, [3] = maxY, [4] = minZ, [5] = maxZ
            
            mask_faces[0] = (min_index[0] == 0);                // Lower X boundary
            mask_faces[1] = (max_index[0] == dims[0] - 1);      // Upper X boundary
            mask_faces[2] = (min_index[1] == 0);                // Lower Y boundary
            mask_faces[3] = (max_index[1] == dims[1] - 1);      // Upper Y boundary
            mask_faces[4] = (min_index[2] == 0);                // Lower Z boundary
            mask_faces[5] = (max_index[2] == dims[2] - 1);      // Upper Z boundary
            
            // Apply masking to boundary faces only
            bool has_boundary_faces = mask_faces[0] || mask_faces[1] || mask_faces[2] || 
                                       mask_faces[3] || mask_faces[4] || mask_faces[5];
            if (has_boundary_faces) {
              if (verbose)
                std::cout << "#hs.silo: Masking global boundary faces (10 cells deep)..." << std::endl;
              maskBoundaryFaces(scalars, nx_actual, ny_actual, nz_actual, mask_faces);
            }
            
            if (verbose) {
              float min_val = scalars[0], max_val = scalars[0];
              for (float v : scalars) {
                if (v < min_val) min_val = v;
                if (v > max_val) max_val = v;
              }
              std::cout << "#hs.silo: Lambda2 computed successfully (range: [" 
                        << min_val << ":" << max_val << "])" << std::endl;
            }
          }
        } else {
          if (verbose)
            std::cerr << "#hs.silo: Failed to load velocity fields for derived field computation" << std::endl;
        }
      }
      
      // Load regular scalar variables if lambda2 wasn't computed
      if (scalars.empty()) {  // Only load if we haven't already computed a derived field
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
                  // Extract only the real (non-ghost) portion of the variable data
                  // Visit reads all data but marks ghosts; we extract only real data for StructuredVolume
                  
                  // Determine the real data range based on centering
                  int var_min[3], var_max[3];
                  for (int d = 0; d < 3; d++) {
                    if (var_dims[d] == dims[d]) {
                      // Node-centered: use mesh min/max_index directly
                      var_min[d] = min_index[d];
                      var_max[d] = max_index[d];
                    } else if (var_dims[d] == dims[d] - 1) {
                      // Zone-centered: zone indices already match
                      var_min[d] = min_index[d];
                      var_max[d] = max_index[d];
                    } else {
                      // Unknown centering, use full range (no ghosts to extract)
                      var_min[d] = 0;
                      var_max[d] = var_dims[d] - 1;
                    }
                  }
                  
                  // Calculate dimensions of real data
                  int real_dims[3] = {
                    var_max[0] - var_min[0] + 1,
                    var_max[1] - var_min[1] + 1,
                    var_max[2] - var_min[2] + 1
                  };
                  int real_nels = real_dims[0] * real_dims[1] * real_dims[2];
                  scalars.resize(real_nels);
                  
                  // Extract only the real (non-ghost) portion
                  int total_var_els = var_dims[0] * var_dims[1] * var_dims[2];
                  
                  if (qv->datatype == DB_FLOAT) {
                    float *data = (float*)qv->vals[0];
                    if (data) {
                      int idx = 0;
                      for (int k = var_min[2]; k <= var_max[2]; k++) {
                        for (int j = var_min[1]; j <= var_max[1]; j++) {
                          for (int i = var_min[0]; i <= var_max[0]; i++) {
                            int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
                            if (src_idx < 0 || src_idx >= total_var_els) {
                              if (verbose) {
                                std::cerr << "#hs.silo: ERROR: Out of bounds access at domain " << partID 
                                          << ": src_idx=" << src_idx << " >= " << total_var_els 
                                          << " (i=" << i << ",j=" << j << ",k=" << k << ")" 
                                          << " var_dims=(" << var_dims[0] << "," << var_dims[1] << "," << var_dims[2] << ")"
                                          << " var_range=[(" << var_min[0] << "," << var_min[1] << "," << var_min[2] << ") to ("
                                          << var_max[0] << "," << var_max[1] << "," << var_max[2] << ")]" << std::endl;
                              }
                              continue;  // Skip this element
                            }
                            scalars[idx++] = data[src_idx];
                          }
                        }
                      }
                    }
                  } else if (qv->datatype == DB_DOUBLE) {
                    double *data = (double*)qv->vals[0];
                    if (data) {
                      int idx = 0;
                      for (int k = var_min[2]; k <= var_max[2]; k++) {
                        for (int j = var_min[1]; j <= var_max[1]; j++) {
                          for (int i = var_min[0]; i <= var_max[0]; i++) {
                            int src_idx = i + var_dims[0] * (j + var_dims[1] * k);
                            if (src_idx < 0 || src_idx >= total_var_els) {
                              if (verbose) {
                                std::cerr << "#hs.silo: ERROR: Out of bounds access at domain " << partID 
                                          << ": src_idx=" << src_idx << " >= " << total_var_els 
                                          << " (i=" << i << ",j=" << j << ",k=" << k << ")" 
                                          << " var_dims=(" << var_dims[0] << "," << var_dims[1] << "," << var_dims[2] << ")"
                                          << " var_range=[(" << var_min[0] << "," << var_min[1] << "," << var_min[2] << ") to ("
                                          << var_max[0] << "," << var_max[1] << "," << var_max[2] << ")]" << std::endl;
                              }
                              continue;  // Skip this element
                            }
                            scalars[idx++] = (float)data[src_idx];
                          }
                        }
                      }
                    }
                  } else {
                    if (verbose)
                      std::cerr << "#hs.silo:   Unsupported data type: " << qv->datatype << std::endl;
                  }
                  
                  if (verbose && !scalars.empty()) {
                    float min_val = scalars[0], max_val = scalars[0];
                    for (float v : scalars) {
                      if (v < min_val) min_val = v;
                      if (v > max_val) max_val = v;
                    }
                    std::cout << "#hs.silo:   Loaded scalar variable: " << var_name 
                              << " (" << real_nels << " real elements from " << qv->nels << " total"
                              << ", range: [" << min_val << ":" << max_val << "])" << std::endl;
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
        }  // end for loop over variables

        if (verbose && mesh_toc->nqvar > 0) {
          std::cout << "#hs.silo:   Found " << mesh_toc->nqvar << " quad variables" << std::endl;
        }
      }  // end if (scalars.empty())

      if (!scalars.empty()) {
        // Use the real (non-ghost) volume dimensions
        // volumeDims was already calculated from the real region (min_index to max_index)
        vec3i actualDims = volumeDims;
        size_t actual_count = scalars.size();
        
        // Check if data is zone-centered (one less in each dimension)
        size_t expected_node = volumeDims.x * volumeDims.y * volumeDims.z;
        size_t expected_zone = (volumeDims.x-1) * (volumeDims.y-1) * (volumeDims.z-1);
        
        if (actual_count == expected_zone) {
          // Zone-centered data
          actualDims = vec3i(volumeDims.x-1, volumeDims.y-1, volumeDims.z-1);
          if (verbose)
            std::cout << "#hs.silo:   Zone-centered data, dims: " << actualDims << std::endl;
        } else if (actual_count == expected_node) {
          // Node-centered data
          if (verbose)
            std::cout << "#hs.silo:   Node-centered data, dims: " << actualDims << std::endl;
        } else {
          if (verbose)
            std::cerr << "#hs.silo:   Warning: scalar count " << actual_count 
                      << " doesn't match expected node (" << expected_node 
                      << ") or zone (" << expected_zone << ") count. Using volumeDims anyway." << std::endl;
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
