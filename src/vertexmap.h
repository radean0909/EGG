#ifndef VERTEXMAP_H
#define VERTEXMAP_H

#include <stdio.h>
#include <iostream>
#include <vector>

#include "extents2d.h"
#include "dcel.h"

#include "cereal/cereal.hpp"
#include "cereal/types/common.hpp"

namespace gen {

enum class VertexType : char { 
    edge = 0x00, 
    interior = 0x01
};

class VertexMap {

public:
    VertexMap();
    VertexMap(dcel::DCEL *V, Extents2d extents);

    unsigned int size();
    void getNeighbours(dcel::Vertex v, std::vector<dcel::Vertex> &nbs);
    void getNeighbourIndices(dcel::Vertex v, std::vector<int> &nbs);
    int getVertexIndex(dcel::Vertex &v);
    bool isVertex(dcel::Vertex &v);
    bool isEdge(dcel::Vertex &v);
    bool isInterior(dcel::Vertex &v);

    std::vector<dcel::Vertex> vertices;
    std::vector<dcel::Vertex> edge;
    std::vector<dcel::Vertex> interior;

    template <class Archive>
	inline void serialize(Archive & archive) {
		archive(vertices, edge, interior, _vertexIdToMapIndex, _vertexTypes);
	}

private:
	bool _isBoundaryVertex(dcel::Vertex &v);
	bool _isInRange(int id);
    VertexType _getVertexType(dcel::Vertex &v);

	dcel::DCEL *_dcel;
	Extents2d _extents;
	std::vector<int> _vertexIdToMapIndex;
	std::vector<VertexType> _vertexTypes;

};

}

#endif