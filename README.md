# PotreeLoaderCpp
This project provides functionality to iterate through point cloud octree data generated for the Potree renderer


## Overview
"[PotreeConverter](https://github.com/potree/PotreeConverter) generates an octree LOD structure for streaming and real-time rendering of massive point clouds."<br>
These octree structures can be loaded and viewed using Potree which is mainly programmed using JavaScript.<br>
This project aims to make the Potree file and data structure accessible using a simple header-only C++ library.<br>

Features:
- Header-only C++ library (no external dependencies except stl)
- Read Potree Octree hierarchy and metadata like point attributes, offset, scale, ...
- (Conditionally) Traverse octree nodes containing points per cube, bounding boxes, spacing, ...
- Load node points from octree data on disk using the data loader


## How to use
- Create an Octree object using metadata file as argument
- Initiallize data loader
- Get scale, offset and the index of the position coordinates in data
- Traverse nodes and load their respective point data
- Extract your desired point attributes

```
// A PointCloudItem with XYZ coordinates
struct PointCloudItem {
	double x, y, z;
};

/*----------- Directory path to PoTree converted data -------------------------*/
auto current_path = fs::current_path();
string file_searchpath = (current_path / "sample_data" / "sparse_junction").string();

/*----------- Initialize Octree class with PoTree metadata --------------------*/
auto octreeFiles = octree_files::SearchOctreeFiles(file_searchpath);
string metafile(octreeFiles["metadata"]);
Octree octree(metafile);

/*----------- Extract data with Octree Loader Class ---------------------------*/
OctreeLoader loader(&octree);
auto nodeData = loader.CreateMaxNodeData();

const int bytesPerPoint = octree.geometry.pointAttributes.bytes;
const auto scale        = octree.geometry.pointAttributes.posScale;
const auto offset       = octree.geometry.pointAttributes.posOffset;
const int posIndex     = loader.pcloud_byte_offsets.xyz;

std::vector<PointCloudItem> cloudpoints;
cloudpoints.reserve(octree.points);

/*----------- Extract data with Octree Loader Class ----------------------------*/
// Traverse Octree Nodes
octree.geometry.nodes[0]->traverse(
	[maxLevel, &loader, &nodeData, bytesPerPoint, scale, offset, posIndex, &cloudpoints](OctreeGeometryNode* node, int level) {

        // Load raw node data
        auto& data = loader.LoadNodeData(node, nodeData);
	size_t pointCountInBuffer = node->byteSize / bytesPerPoint;
	uint8_t* pBuffer = data.data();

        // Extract coordinates, apply scale plus offset and push to cloudpoints
	for (int i = 0; i < pointCountInBuffer; ++i)
	{
	     const int offsetToPointStart = i * bytesPerPoint;

             double x = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + posIndex)), scale.x, offset.x);
             double y = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + posIndex + sizeof(int32_t))), scale.y, offset.y);
             double z = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + posIndex + 2 * sizeof(int32_t))), scale.z, offset.z);

             cloudpoints.emplace_back(x, y, z);
	}
	});
```


## How to build sample
For an example how to additionally load point color data, build the sample source.

- Open the provided Visual Studio solution with VS2017 or newer (VS2022 is recommended)
- Select your target configuration and hit build
- Run ``PoTreeLoader.exe``
  Running the program with the debugger in Debug mode is recommended. Set breakpoints to see how the code operates.


## Credits
Due to the nature of working with a predefined data structure, some of the code is a direct translation of the Potree project source

- Markus Schütz and contributors of [Potree](https://github.com/potree/potree) and [PotreeConverter](https://github.com/potree/PotreeConverter)
