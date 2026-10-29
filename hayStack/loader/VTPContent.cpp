// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/VTPContent.h"
#include "hayStack/TriangleMesh.h"

#if HS_VTK
#include <vtkVersion.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkPolyDataReader.h>
#include <vtkCellArray.h>
#endif

#include <cstring>
#include <fstream>
#include <limits>

namespace hs {
  namespace loader {
#if HS_VTK
    namespace vtp {
      void readPolyData(OnePartition &dataGroup,
                        const ResourceSpecifier &data,
                        vtkPolyData *poly)
      {
        const std::string requestedScalar=data.get("scalar","");
        vtkPointData *pointData = poly->GetPointData();        
        assert(pointData);
        vtkCellData  *cellData  = poly->GetCellData();
        assert(cellData);

        int numPointArrays = pointData->GetNumberOfArrays();
        bool scalarsOnVertices = (numPointArrays > 0);
        vtkDataArray *scalarArray = 0;
        bool havePerCellData;
        if (scalarsOnVertices) {
          for(int i=0;i<numPointArrays;++i) {
            std::string name = pointData->GetArrayName(i);
            if (requestedScalar != "" && name != requestedScalar) {
              std::cout << "#hs.vtp:  - found (and skipped) scalar " << name
                        << std::endl;
              continue;
            }
            scalarArray = pointData->GetArray(i);
            break;
          }
        } else {
          int numCellArrays  = cellData->GetNumberOfArrays();
          throw std::runtime_error("cell centric vtk data not yet supported");
        }
        if (requestedScalar != "" && scalarArray==nullptr) 
          throw std::runtime_error
            ("requested scalar '"+requestedScalar+"', not found");

        TriangleMesh mesh;
        int numPoints = poly->GetNumberOfPoints();
        for (int i=0;i<numPoints;i++) {
        }
        
        // now, parse the actual polys...
        for (int polyIdx=0; polyIdx<poly->GetNumberOfPolys(); polyIdx++) {
          vtkCell   *cell    = poly->GetCell(polyIdx);
          vtkIdList *cellIDs = cell->GetPointIds();
          int numIDs = cellIDs->GetNumberOfIds();
          for (int i=2;i<numIDs;i++) {

            int i0 = cellIDs->GetId(0);
            int i1 = cellIDs->GetId(i-1);
            int i2 = cellIDs->GetId(i);
            
            vec3d p0,p1,p2;
            poly->GetPoint(i0,&p0.x);
            poly->GetPoint(i1,&p1.x);
            poly->GetPoint(i2,&p2.x);
            
            
          }
        }
      }
    }
    
    void VTPContent::executeLoad(OnePartition &dataGroup)
    {
      if (endsWith(data.where,".vtp")) {
        std::cout << "#hs::vtp: file ends in .vtp, using vtk polydata reader"
                  << std::endl;
        vtkNew<vtkXMLPolyDataReader> reader;
        reader->SetFileName(data.where.c_str());
        reader->Update();
        vtkPolyData *poly = reader->GetOutput ();
        if (!poly)
          throw std::runtime_error("could not find polydata in vtp file");
        vtp::readPolyData(dataGroup,data,poly);
      } else {
        std::cout << "#hs::vtp: not a '.vtp' file, using vtk generic reader"
                  << std::endl;
        vtkNew<vtkGenericDataObjectReader> reader;
        reader->SetFileName(data.where.c_str());
        reader->Update();
        vtkPolyData *poly = reader->GetPolyDataOutput();
        if (!poly)
          throw std::runtime_error("could not find polydata in vtk reader result");
        vtp::readPolyData(dataGroup,data,poly);
      }
    }
    
    VTPContent::VTPContent(const ResourceSpecifier &data)
      : data(data),
        fileSize(getFileSize(data.where))
    {}
    
    std::string VTPContent::toString()
    { return "VTPContent: "+data.where; }
    
    size_t VTPContent::projectedSize()
    { return fileSize; }
    
#endif

    
    void VTPContent::create(DataLoader *loader,
                       const ResourceSpecifier &data)
    {
#if HS_VTK
      loader->addContent(new VTPContent(data));
#else
      throw std::runtime_error
        ("hs::VTPContent requires VTK support, but vtk isn't "
         "compiled into this version of haystack");
#endif
    }
    
  }
}
