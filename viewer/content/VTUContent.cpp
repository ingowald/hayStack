#include "VTUContent.h"

#include <vtkSmartPointer.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkCellTypes.h>
#include <vtkCellIterator.h>
#include <vtkIdList.h>

namespace hs {

  VTUContent::VTUContent(const std::string &fileName)
    : fileName(fileName),
      fileSize(getFileSize(fileName))
  {}

  void VTUContent::create(DataLoader *loader,
                           const std::string &dataURL)
  {
    loader->addContent(new VTUContent(dataURL));
  }

  std::string VTUContent::toString()
  {
    return "VTU{fileName="+fileName+", proj size "
      +prettyNumber(projectedSize())+"B}";
  }

  size_t VTUContent::projectedSize()
  { return 2 * fileSize; }

  void VTUContent::executeLoad(DataRank &dataRank, bool verbose)
  {
    std::cout << "#hs.vtu: loading " << fileName << std::endl;

    auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    reader->SetFileName(fileName.c_str());
    reader->Update();

    vtkUnstructuredGrid *grid = reader->GetOutput();
    if (!grid) {
      std::cerr << "#hs.vtu: could not read " << fileName << std::endl;
      return;
    }

    const int numPoints = grid->GetNumberOfPoints();
    const int numCells  = grid->GetNumberOfCells();
    std::cout << "#hs.vtu: " << numPoints << " points, "
              << numCells << " cells" << std::endl;

    umesh::UMesh::SP mesh = std::make_shared<umesh::UMesh>();

    // ---- vertices ----
    mesh->vertices.resize(numPoints);
    for (int i = 0; i < numPoints; i++) {
      double pt[3];
      grid->GetPoint(i, pt);
      mesh->vertices[i] = umesh::vec3f((float)pt[0],(float)pt[1],(float)pt[2]);
    }

    // ---- scalars ----
    // Try per-vertex (point data) first, then per-cell (cell data)
    vtkPointData *pointData = grid->GetPointData();
    vtkCellData  *cellData  = grid->GetCellData();
    vtkDataArray *scalarArray = nullptr;
    bool perVertex = false;

    if (pointData && pointData->GetNumberOfArrays() > 0) {
      // prefer the first scalar (1-component) array
      for (int a = 0; a < pointData->GetNumberOfArrays(); a++) {
        vtkDataArray *arr = pointData->GetArray(a);
        if (arr && arr->GetNumberOfComponents() == 1) {
          scalarArray = arr;
          perVertex = true;
          std::cout << "#hs.vtu: using per-vertex scalar '"
                    << arr->GetName() << "'" << std::endl;
          break;
        }
      }
    }
    if (!scalarArray && cellData && cellData->GetNumberOfArrays() > 0) {
      for (int a = 0; a < cellData->GetNumberOfArrays(); a++) {
        vtkDataArray *arr = cellData->GetArray(a);
        if (arr && arr->GetNumberOfComponents() == 1) {
          scalarArray = arr;
          perVertex = false;
          std::cout << "#hs.vtu: using per-cell scalar '"
                    << arr->GetName() << "'" << std::endl;
          break;
        }
      }
    }

    // ---- cells ----
    int numPolyhedra = 0;
    int numStandard  = 0;

    // Per-cell scalars need to be converted to per-vertex for UMesh.
    // We'll accumulate and average if needed.
    std::vector<float> perVertexValues(numPoints, 0.f);
    std::vector<float> perVertexCounts(numPoints, 0.f);
    bool needCellToVertexConversion = (scalarArray && !perVertex);

    // Use vtkCellIterator which properly handles VTK 9.x's internal
    // representation for polyhedra (the legacy GetFaces/GetFaceStream
    // APIs don't work reliably in VTK 9.1).
    vtkIdType cellId = 0;
    auto iter = vtkSmartPointer<vtkCellIterator>::Take(grid->NewCellIterator());
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
         iter->GoToNextCell(), cellId++) {
      int cellType = iter->GetCellType();
      vtkIdList *ptIds = iter->GetPointIds();
      vtkIdType npts = ptIds->GetNumberOfIds();

      if (cellType == VTK_TETRA && npts == 4) {
        mesh->tets.push_back({(int)ptIds->GetId(0),(int)ptIds->GetId(1),
                              (int)ptIds->GetId(2),(int)ptIds->GetId(3)});
        numStandard++;
      } else if (cellType == VTK_PYRAMID && npts == 5) {
        mesh->pyrs.push_back({(int)ptIds->GetId(0),(int)ptIds->GetId(1),
                              (int)ptIds->GetId(2),(int)ptIds->GetId(3),
                              (int)ptIds->GetId(4)});
        numStandard++;
      } else if (cellType == VTK_WEDGE && npts == 6) {
        mesh->wedges.push_back({(int)ptIds->GetId(0),(int)ptIds->GetId(1),
                                (int)ptIds->GetId(2),(int)ptIds->GetId(3),
                                (int)ptIds->GetId(4),(int)ptIds->GetId(5)});
        numStandard++;
      } else if (cellType == VTK_HEXAHEDRON && npts == 8) {
        mesh->hexes.push_back({(int)ptIds->GetId(0),(int)ptIds->GetId(1),
                                (int)ptIds->GetId(2),(int)ptIds->GetId(3),
                                (int)ptIds->GetId(4),(int)ptIds->GetId(5),
                                (int)ptIds->GetId(6),(int)ptIds->GetId(7)});
        numStandard++;
      } else if (cellType == VTK_POLYHEDRON) {
        numPolyhedra++;
      } else {
        static bool warned = false;
        if (!warned) {
          std::cerr << "#hs.vtu: skipping unsupported cell type "
                    << cellType << " (further warnings suppressed)"
                    << std::endl;
          warned = true;
        }
        continue;
      }

      // Accumulate per-cell scalar to vertices if needed
      if (needCellToVertexConversion) {
        float cellVal = (float)scalarArray->GetTuple1(cellId);
        for (vtkIdType p = 0; p < npts; p++) {
          perVertexValues[ptIds->GetId(p)] += cellVal;
          perVertexCounts[ptIds->GetId(p)] += 1.f;
        }
      }
    }

    std::cout << "#hs.vtu: " << numStandard << " standard cells, "
              << numPolyhedra << " polyhedra" << std::endl;

    // ---- read polyhedra face streams (requires VTK >= 9.6) ----
    if (numPolyhedra > 0) {
      auto faceList = vtkSmartPointer<vtkIdList>::New();
      for (vtkIdType cid = 0; cid < numCells; cid++) {
        if (grid->GetCellType(cid) != VTK_POLYHEDRON) continue;

        grid->GetFaceStream(cid, faceList);
        vtkIdType streamLen = faceList->GetNumberOfIds();
        if (streamLen == 0) {
          std::cerr << "#hs.vtu: WARNING - empty face stream for cell "
                    << cid << std::endl;
          continue;
        }

        mesh->polyOffsets.push_back((int)mesh->polyFaceStream.size());
        // face stream format: [nfaces, npts0, pt0, pt1, ..., npts1, ...]
        for (vtkIdType i = 0; i < streamLen; i++)
          mesh->polyFaceStream.push_back((int)faceList->GetId(i));

        // cell-to-vertex conversion for this polyhedron
        if (needCellToVertexConversion) {
          vtkIdType npts;
          const vtkIdType *pts;
          grid->GetCellPoints(cid, npts, pts);
          float cellVal = (float)scalarArray->GetTuple1(cid);
          for (vtkIdType p = 0; p < npts; p++) {
            perVertexValues[pts[p]] += cellVal;
            perVertexCounts[pts[p]] += 1.f;
          }
        }
      }
      std::cout << "#hs.vtu: loaded " << mesh->polyOffsets.size()
                << " polyhedra (faceStream size="
                << mesh->polyFaceStream.size() << ")" << std::endl;
    }

    // ---- build per-vertex attribute ----
    if (scalarArray) {
      auto attr = std::make_shared<umesh::Attribute>();
      attr->values.resize(numPoints);

      if (perVertex) {
        for (int i = 0; i < numPoints; i++)
          attr->values[i] = (float)scalarArray->GetTuple1(i);
      } else {
        // cell-to-vertex averaging
        for (int i = 0; i < numPoints; i++) {
          if (perVertexCounts[i] > 0.f)
            attr->values[i] = perVertexValues[i] / perVertexCounts[i];
          else
            attr->values[i] = 0.f;
        }
      }
      mesh->perVertex = attr;
    } else {
      std::cerr << "#hs.vtu: WARNING - no scalar field found, "
                << "creating zero-valued attribute" << std::endl;
      auto attr = std::make_shared<umesh::Attribute>();
      attr->values.resize(numPoints, 0.f);
      mesh->perVertex = attr;
    }

    // Compute bounds explicitly (including polyhedra vertices),
    // then call finalize for attribute value ranges.
    if (mesh->perVertex) mesh->perVertex->finalize();
    mesh->bounds = umesh::box3f();
    for (auto &v : mesh->vertices)
      mesh->bounds.extend(v);
    if (mesh->bounds.empty()) {
      std::cerr << "#hs.vtu: WARNING - mesh has empty bounds" << std::endl;
    }

    std::cout << "#hs.vtu: bounds ["
              << mesh->bounds.lower.x << " " << mesh->bounds.lower.y << " " << mesh->bounds.lower.z
              << "] -> ["
              << mesh->bounds.upper.x << " " << mesh->bounds.upper.y << " " << mesh->bounds.upper.z
              << "]" << std::endl;

    dataRank.unsts.push_back({mesh, box3f()});
    std::cout << "#hs.vtu: loaded successfully" << std::endl;
  }

}
