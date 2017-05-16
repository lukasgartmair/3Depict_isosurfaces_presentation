#ifndef OPENVDB_INCLUDES_H
#define OPENVDB_INCLUDES_H

#ifdef __LITTLE_ENDIAN__
	#undef __LITTLE_ENDIAN__
#endif

#include <openvdb/openvdb.h>
#include <openvdb/math/Maps.h>
#include <openvdb/Grid.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/io/Stream.h>
#include <openvdb/math/Coord.h>
#include <openvdb/metadata/MetaMap.h>

#include "OpenVDB_TestSuite/vdb_functions.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

#endif
