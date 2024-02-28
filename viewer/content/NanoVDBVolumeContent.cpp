// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
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

#include "NanoVDBVolumeContent.h"
//#include <fstream>
//#include <umesh/UMesh.h>
//#include <umesh/extractIsoSurface.h>
//#include <miniScene/Scene.h>

#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/GridHandle.h>
#include <nanovdb/util/IO.h>

//namespace umesh {
//	UMesh::SP tetrahedralize(UMesh::SP in,
//		int ownedTets,
//		int ownedPyrs,
//		int ownedWedges,
//		int ownedHexes);
//}
namespace hs {

	NanoVDBVolumeContent::NanoVDBVolumeContent(const std::string& fileName,
		int thisPartID,
		const box3i& cellRange,
		vec3i fullVolumeDims,
		BNTexelFormat texelFormat,
		int numChannels)
		: fileName(fileName),
		thisPartID(thisPartID),
		cellRange(cellRange),
		fullVolumeDims(fullVolumeDims),
		texelFormat(texelFormat),
		numChannels(numChannels)
	{}

	void NVDBsplitKDTree(std::vector<box3i>& regions,
		box3i cellRange,
		int numParts)
	{
		if (numParts == 1) {
			regions.push_back(cellRange);
			return;
		}

		vec3i size = cellRange.size();
		int dim = arg_max(size);
		if (size[dim] < 2) {
			regions.push_back(cellRange);
			return;
		}

		int nRight = numParts / 2;
		int nLeft = numParts - nRight;

		box3i lBox = cellRange, rBox = cellRange;
		lBox.upper[dim]
			= rBox.lower[dim]
			= cellRange.lower[dim] + (cellRange.size()[dim] * nLeft) / numParts;
		NVDBsplitKDTree(regions, lBox, nLeft);
		NVDBsplitKDTree(regions, rBox, nRight);
	}

	void NanoVDBVolumeContent::create(DataLoader* loader,
		const ResourceSpecifier& dataURL)
	{
		nanovdb::GridHandle<> nanogrid = nanovdb::io::readGrid<>(dataURL.where);
		std::cout << "Loaded NanoVDB grid[" << nanogrid.gridMetaData()->shortGridName() << "]...\n";

		auto type = nanogrid.gridMetaData()->gridType();

		BNTexelFormat texelFormat;
		int numChannels = 1;
		switch (type) {
		case nanovdb::GridType::Float:
			texelFormat = BN_TEXEL_FORMAT_NANOVDB_FLOAT;
			numChannels = 1;
			break;
		default:
			throw std::runtime_error("NanoVDBVolumeContent: invalid type '" + std::string(nanovdb::toStr(type)) + "'");
		}

		auto vdb_dims = nanogrid.gridMetaData()->worldBBox().dim();
		auto vdb_spacing = nanogrid.gridMetaData()->voxelSize();
		vec3i dims;
		dims.x = (int)(vdb_dims[0] / vdb_spacing[0]);
		dims.y = (int)(vdb_dims[1] / vdb_spacing[1]);
		dims.z = (int)(vdb_dims[2] / vdb_spacing[2]);

		box3i initRegion = { vec3i(0), dims - 1 };
		std::string extractString = dataURL.get("extract");
		if (!extractString.empty()) {
			vec3i lower, size;
			int n = sscanf(extractString.c_str(), "%i,%i,%i,%i,%i,%i",
				&lower.x, &lower.y, &lower.z,
				&size.x, &size.y, &size.z);
			if (n != 6)
				throw std::runtime_error("NanoVDBVolumeContent:: could not parse 'extract' value from '"
					+ extractString
					+ "' (should be 'f,f,f,f,f,f' format)");
			initRegion.lower = lower;
			initRegion.upper = lower + size - 1;
		}

		std::vector<box3i> regions;
		NVDBsplitKDTree(regions, initRegion, dataURL.numParts);
		if (regions.size() < dataURL.numParts)
			throw std::runtime_error("input data too small to split into indicated number of parts");

		if (loader->myRank() == 0) {
			std::cout << "RAW Volume: input data file of " << dims << " voxels will be read in the following bricks:" << std::endl;
			for (int i = 0; i < regions.size(); i++)
				std::cout << " #" << i << " : " << regions[i] << std::endl;
		}

		for (int i = 0; i < dataURL.numParts; i++) {
			loader->addContent(new NanoVDBVolumeContent(dataURL.where, i,
				regions[i],
				dims, texelFormat,
				numChannels));
		}
	}

	size_t NanoVDBVolumeContent::projectedSize()
	{
		vec3i numVoxels = cellRange.size() + 1;
		return numVoxels.x * size_t(numVoxels.y) * numVoxels.z * numChannels * sizeOf(texelFormat);
	}

	void NanoVDBVolumeContent::executeLoad(DataGroup& dataGroup, bool verbose)
	{
		vec3i numVoxels = (cellRange.size() + 1);
		vec3f gridOrigin(cellRange.lower);

		nanovdb::GridHandle<> nanogrid = nanovdb::io::readGrid<>(fileName);
		std::cout << "Loaded NanoVDB grid[" << nanogrid.gridMetaData()->shortGridName() << "]...\n";

		auto vdb_spacing = nanogrid.gridMetaData()->voxelSize();
		vec3f gridSpacing(1.0f, 1.0f, 1.0f);

		std::vector<uint8_t> rawData(nanogrid.size());
		memcpy(rawData.data(), nanogrid.data(), rawData.size());

		std::vector<uint8_t> rawDataRGB;

		dataGroup.structuredVolumes.push_back
		(std::make_shared<StructuredVolume>(numVoxels, texelFormat, rawData, rawDataRGB,
			gridOrigin, gridSpacing));
	}

	std::string NanoVDBVolumeContent::toString()
	{
		std::stringstream ss;
		ss << "NanoVDBVolumeContent{#" << thisPartID << ",fileName=" << fileName << ",cellRange=" << cellRange << "}";
		return ss.str();
	}


}
