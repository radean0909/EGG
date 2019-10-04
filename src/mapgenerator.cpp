#include "mapgenerator.h"
#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/common.hpp"

gen::MapGenerator::MapGenerator() : 
                                _fontData(gen::resources::getFontDataResource()),
                                _cityLabelFontFace("Times New Roman"),
                                _townLabelFontFace("Times New Roman"),
                                _areaLabelFontFace("Times New Roman") {
    Extents2d defaultExtents(0, 0, _defaultExtentsWidth, _defaultExtentsHeight);
    _extents = defaultExtents;
    _resolution = _defaultResolution;
    double width = _extents.maxx - _extents.minx;
    double height = _extents.maxy - _extents.miny;
    double aspectratio = width / height;
    _imgwidth = (int)(aspectratio*_defaultImageHeight + 0.5);
    _imgheight = _defaultImageHeight;
}

gen::MapGenerator::MapGenerator(Extents2d extents, double resolution, 
                                int imgwidth, int imgheight) : 
                                _extents(extents), _resolution(resolution),
                                _imgwidth(imgwidth), _imgheight(imgheight),
                                _fontData(gen::resources::getFontDataResource()),
                                _cityLabelFontFace("Times New Roman"),
                                _townLabelFontFace("Times New Roman"),
                                _areaLabelFontFace("Times New Roman") {
}

gen::MapGenerator::MapGenerator(Extents2d extents, double resolution) :
                                _extents(extents), _resolution(resolution),
                                _fontData(gen::resources::getFontDataResource()),
                                _cityLabelFontFace("Times New Roman"),
                                _townLabelFontFace("Times New Roman"),
                                _areaLabelFontFace("Times New Roman") {
    double width = _extents.maxx - _extents.minx;
    double height = _extents.maxy - _extents.miny;
    double aspectratio = width / height;
    _imgwidth = (int)(aspectratio*_defaultImageHeight + 0.5);
    _imgheight = _defaultImageHeight;
}

void gen::MapGenerator::initialize() {
    if (_voronoi.vertices.size() == 0) {
        _initializeVoronoiData();
    }
    _initializeMapData();
    _isInitialized = true;
}

void gen::MapGenerator::normalize() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    _heightMap.normalize();
}

void gen::MapGenerator::round() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    _heightMap.round();
}

void gen::MapGenerator::relax() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    _heightMap.relax();
}

void gen::MapGenerator::setSeaLevel(double level) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    _heightMap.setLevel(level);
}

void gen::MapGenerator::setSeaLevelToMedian() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    _heightMap.setLevelToMedian();
}

void gen::MapGenerator::addHill(double px, double py, double r, double height, bool multiply = false) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    /*
        Create a rounded hill where height falls off smoothly
    */

    // coefficients for the Wyvill kernel function
    double coef1 = (4.0 / 9.0)*(1.0 / (r*r*r*r*r*r));
    double coef2 = (17.0 / 9.0)*(1.0 / (r*r*r*r));
    double coef3 = (22.0 / 9.0)*(1.0 / (r*r));
    double rsq = r*r;

    dcel::Point v;
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        v = _vertexMap.vertices[i].position;
        double dx = v.x - px;
        double dy = v.y - py;
        double dsq = dx*dx + dy*dy;
        if (dsq < rsq) {
            double kernel = 1.0 - coef1*dsq*dsq*dsq + coef2*dsq*dsq - coef3*dsq;
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval + height*kernel);
            } else {
                _heightMap.set(i, hval * height*kernel);
            }
        } else {
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval + 0);
            } else {
                _heightMap.set(i, hval * 0);
            }
        }
    }
}

void gen::MapGenerator::addCone(double px, double py, double radius, double height, bool multiply = false) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    /*
        Create a cone where height falls off linearly
    */

    double invradius = 1.0 / radius;
    double rsq = radius * radius;
    dcel::Point v;
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        v = _vertexMap.vertices[i].position;
        double dx = v.x - px;
        double dy = v.y - py;
        double dsq = dx*dx + dy*dy;
        if (dsq < rsq) {
            double dist = sqrt(dsq);
            double kernel = 1.0 - dist * invradius;
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval + height*kernel);
            } else {
                _heightMap.set(i, hval * height*kernel);
            }
        } else {
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval + 0);
            } else {
                _heightMap.set(i, hval * 0);
            }
        }
    }
}

void gen::MapGenerator::addSlope(double px, double py, double dirx, double diry, 
                                 double radius, double height, bool multiply = false) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    /*
        Add a slope that runs parallel to the line with direction (dirx, diry)
        and position (px, py). (dirx, diry) must be a unit vector. The slope
        varies in height linearly from 0 to height from the left side of the 
        direction vector to the right side of the direction vector.
    */

    dcel::Point v;
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        v = _vertexMap.vertices[i].position;
        
        double dx = px - v.x;
        double dy = py - v.y;
        double dot = dx*dirx + dy*diry;
        double distx = dx - dot*dirx;
        double disty = dy - dot*diry;
        double dist = sqrt(distx*distx + disty*disty);
        dist = fmin(dist, radius);

        double cross = dx*diry - dy*dirx;
        double min, max;
        if (cross < 0) {
            min = 0.5*height;
            max = 0;
        } else {
            min = 0.5*height;
            max = height;
        }

        double fieldval = min + (dist / radius)*(max - min);
        double hval = _heightMap(i);
        if (!multiply) {
                _heightMap.set(i, hval + fieldval);
            } else {
                _heightMap.set(i, hval * fieldval);
            }
    }
}


void gen::MapGenerator::addPit(double px, double py, double radius, double height, bool multiply = false) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    /*
        Create a conal pit where height rises off linearly
    */

    double invradius = 1.0 / radius;
    double rsq = radius * radius;
    dcel::Point v;
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        v = _vertexMap.vertices[i].position;
        double dx = v.x - px;
        double dy = v.y - py;
        double dsq = dx*dx + dy*dy;
        if (dsq < rsq) {
            double dist = sqrt(dsq);
            double kernel = 1.0 - dist * invradius;
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval - height*kernel);
            } else {
                _heightMap.set(i, hval * -height*kernel);
            }
        }
    }
}

void gen::MapGenerator::addDepression(double px, double py, double r, double height, bool multiply = false) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    /*
        Create a rounded depression where height falls off smoothly
    */

    // coefficients for the Wyvill kernel function
    double coef1 = (4.0 / 9.0)*(1.0 / (r*r*r*r*r*r));
    double coef2 = (17.0 / 9.0)*(1.0 / (r*r*r*r));
    double coef3 = (22.0 / 9.0)*(1.0 / (r*r));
    double rsq = r*r;

    dcel::Point v;
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        v = _vertexMap.vertices[i].position;
        double dx = v.x - px;
        double dy = v.y - py;
        double dsq = dx*dx + dy*dy;
        if (dsq < rsq) {
            double kernel = 1.0 - coef1*dsq*dsq*dsq + coef2*dsq*dsq - coef3*dsq;
            double hval = _heightMap(i);
            if (!multiply) {
                _heightMap.set(i, hval - height*kernel);
            } else {
                _heightMap.set(i, hval *    height*kernel);
            }
        }
    }
}

void gen::MapGenerator::multiply(double min, double max, double amount) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    for (unsigned int i =0; i <_heightMap.size(); i++) {
        double h = _heightMap(i);
        if (h > min && h < max) {
            _heightMap.set(i, h * amount);
        }
    }
}

void gen::MapGenerator::addNoise(double freq, double strength, bool multiply) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    FastNoise noise;
    noise.SetNoiseType(FastNoise::Simplex);
    noise.SetSeed(rand()%10000);
    noise.SetFrequency(freq);
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        dcel::Point p = _vertexMap.vertices[i].position;
        double h = _heightMap(i);
        double n = noise.GetNoise(p.x, p.y);
        if (!multiply) {
            _heightMap.set(i, h + strength*n);
        } else {
            _heightMap.set(i, h * strength*n);
        }
    }
}

double gen::MapGenerator::randomDouble(double min, double max) {
    return min + (double)rand() / ((double)RAND_MAX / (max - min));
}

void gen::MapGenerator::makeContinent() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    FastNoise noiseGen;
    noiseGen.SetNoiseType(FastNoise::Simplex);
    noiseGen.SetSeed(rand() % 1000000);
    noiseGen.SetFrequency(randomDouble(.1, 1));

    double minDist = randomDouble(1., 2.);
    double scalar = randomDouble(1., 4.);

    for (unsigned int i = 0; i <_heightMap.size(); i++) {
        double h = _heightMap(i);
        dcel::Point p = _vertexMap.vertices[i].position;

        double dist = abs(_extents.minx - p.x);

        if (abs(_extents.maxx - p.x) < dist) {
            dist = abs(_extents.maxx - p.x);
        }
        if (abs(_extents.maxy - p.y) < dist) {
            dist =abs(_extents.maxy - p.y);
        }
        if (abs(_extents.miny - p.y) < dist) {
            dist = abs(_extents.miny - p.y);
        }
        double noise = scalar * noiseGen.GetNoise(p.x, p.y);
        if (dist <= minDist + noise) {
            _heightMap.set(i, h - (1/pow(minDist + noise, 2)));
        } else {
            _heightMap.set(i, h - (1/pow(dist,2)));
        }
    }
}


void gen::MapGenerator::erode()  {
    erode(_defaultErodeAmount);
}

void gen::MapGenerator::erode(double amount) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<double> erosionMap(vmp, 0.0);
    _calculateErosionMap(erosionMap);
    gen::config::print("Erosion Map Created...");
    for (unsigned int i = 0; i < _heightMap.size(); i++) {
        double currlevel = _heightMap(i);
        double newlevel = currlevel - amount * erosionMap(i);
        _heightMap.set(i, newlevel);
    }

    _isHeightMapEroded = true;
}

void gen::MapGenerator::generateBiomes() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<double> precipitationMap(vmp, 0.0);
    _calculatePrecipitationMap(precipitationMap);
     _precipitationMap = precipitationMap;
     _isPrecipitationCalculated = true;

    NodeMap<double> temperatureMap(vmp, 0.0);
    _calculateTemperatureMap(temperatureMap);
    _temperatureMap = temperatureMap;
    _isTemperatureCalculated = true;

    NodeMap<double> biomeMap(vmp);
    _calculateBiomeMap(biomeMap);
    _biomeMap = biomeMap;

}

void gen::MapGenerator::addCity(std::string cityName, std::string territoryName) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    if (!_isHeightMapEroded) {
        erode(0.0);
    }

    CityLocation loc = _getCityLocation();

    City city;
    city.cityName = cityName;
    city.territoryName = territoryName;
    city.position = loc.position;
    city.faceid = loc.faceid;
    _updateCityMovementCost(city);

    _cities.push_back(city);
}

void gen::MapGenerator::addTown(std::string townName) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    if (!_isHeightMapEroded) {
        erode(0.0);
    }

    CityLocation loc = _getCityLocation();

    Town town;
    town.townName = townName;
    town.position = loc.position;
    town.faceid = loc.faceid;
    _towns.push_back(town);
}

void gen::MapGenerator::outputVoronoiDiagram(std::string filename) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    // cereal version
    std::ofstream file(filename, std::ios::binary);
    {
        cereal::BinaryOutputArchive oarchive(file);
        dcel::DCEL voronoi = _voronoi;
        oarchive(voronoi);
    }
}

void gen::MapGenerator::outputHeightMap(std::string filename) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    // Cereal version
    std::ofstream file(filename, std::ios::binary);
    {   
        cereal::BinaryOutputArchive oarchive(file);
        NodeMap<double> heightMap = _heightMap;
        oarchive(_heightMap);
    } // to ensure contents are flushed
}

void gen::MapGenerator::outputInstructionFile(std::string filename) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }
    // Cereal version
    std::ofstream file(filename);
    {   
        cereal::JSONOutputArchive oarchive(file);
        std::vector<gen::MapInstruction> instructions = _instructions;
        oarchive(CEREAL_NVP(instructions));
    } // to ensure contents are flushed
}

void gen::MapGenerator::addInstruction(gen::MapInstruction instruction) {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    _instructions.push_back(instruction);
}

std::vector<char> gen::MapGenerator::getDrawData() {
    if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    if (!_isHeightMapEroded) {
        erode(0.0);
    }

    std::vector<std::vector<double> > contourData;
    if (_isContourEnabled) { 
        _getContourDrawData(contourData);
        _contourData = contourData;
    }

    std::vector<std::vector<double> > riverData;
    if (_isRiversEnabled) {
        _getRiverDrawData(riverData);
        _riverData = riverData;
    }

    std::vector<double> slopeData;
    if (_isSlopesEnabled) {
        _getSlopeDrawData(slopeData);
    }

    std::vector<double> cityData;
    if (_isCitiesEnabled) {
        _getCityDrawData(cityData);
    }

    std::vector<double> townData;
    if (_isTownsEnabled) {
        _getTownDrawData(townData);
    }

    std::vector<std::vector<double> > territoryData;
    _getTerritoryDrawData(territoryData);
    _borderData = territoryData;
    if (!_isBordersEnabled) {
        territoryData.clear();
    }

    std::vector<jsoncons::json> labelData;
    if (_isLabelsEnabled) {
        _getLabelDrawData(labelData);
    }

    std::vector<jsoncons::json> faceVertices;
    _getBiomeDrawData(faceVertices);

    jsoncons::json output;
    output["image_width"] = _imgwidth;
    output["image_height"] = _imgheight;
    output["draw_scale"] = _drawScale;
    output["contour"] = contourData;
    output["river"] = riverData;
    output["slope"] = slopeData;
    output["city"] = cityData;
    output["town"] = townData;
    output["territory"] = territoryData;
    output["label"] = labelData;
    output["biomes"] = faceVertices;

    std::string strout = output.as<std::string>();
    std::vector<char> charvect(strout.begin(), strout.end());
    charvect.push_back('\0');

    return charvect;
}

Extents2d gen::MapGenerator::getExtents() {
    return _extents;
}

void gen::MapGenerator::readHeightMapFile(std::string filename) {
    // Cereal version
    {
        std::ifstream is(filename, std::ios::binary);
        gen::config::print("\tReading heightmap file...");
        cereal::BinaryInputArchive iarchive(is);
        gen::NodeMap<double> heightMap;
        iarchive(heightMap);
        _heightMap = heightMap;
    }
    gen::config::print("\tHeightMap with " + gen::config::toString(_heightMap.size())+ " nodes loaded...");
}


void gen::MapGenerator::readVoronoiFile(std::string filename) {
    // Cereal version
    {
        std::ifstream is(filename, std::ios::binary);
        gen::config::print("\tReading voronoi file...");
        cereal::BinaryInputArchive iarchive(is);
        dcel::DCEL voronoi;
        iarchive(voronoi);

        _voronoi = voronoi;
    }
}

void gen::MapGenerator::readInstructionFile(std::string filename) {
     if (!_isInitialized) {
        throw std::runtime_error("MapGenerator must be initialized.");
    }

    {
        std::ifstream is(filename);
        gen::config::print("\tReading map instructions file...");
        cereal::JSONInputArchive iarchive(is);
        std::vector<gen::MapInstruction> instructions;
        iarchive(cereal::make_nvp("instructions", instructions));
        gen::config::print("\tInstructions size:" + gen::config::toString(instructions.size()));

        _instructions = instructions; 
    }
}


void gen::MapGenerator::setDrawScale(double scale) {
    if (scale > 0.0) {
        double origDrawScale = _drawScale;
        _drawScale = scale;
        _cityMarkerRadius *= (_drawScale / origDrawScale);
        _townMarkerRadius *= (_drawScale / origDrawScale);
    }
}
void gen::MapGenerator::setMapGlobalPosition(double scale, double offset) {
    if (scale > 0.1) {
        _mapScale = scale;
    }

    if (scale + offset <= 1) {
        _mapOffset = offset;
    } else {
        _mapOffset = 1. - _mapScale;
    }
}

void gen::MapGenerator::enableSlopes() {
    _isSlopesEnabled = true;
}

void gen::MapGenerator::enableRivers() {
    _isRiversEnabled = true;
}

void gen::MapGenerator::enableContour() {
    _isContourEnabled = true;
}

void gen::MapGenerator::enableBorders() {
    _isBordersEnabled = true;
}

void gen::MapGenerator::enableCities() {
    _isCitiesEnabled = true;
}

void gen::MapGenerator::enableTowns() {
    _isTownsEnabled = true;
}

void gen::MapGenerator::enableLabels() {
    _isLabelsEnabled = true;
}

void gen::MapGenerator::enableAreaLabels() {
    _isAreaLabelsEnabled = true;
}

void gen::MapGenerator::disableSlopes() {
    _isSlopesEnabled = false;
}

void gen::MapGenerator::disableRivers() {
    _isRiversEnabled = false;
}

void gen::MapGenerator::disableContour() {
    _isContourEnabled = false;

}

void gen::MapGenerator::disableBorders() {
    _isBordersEnabled = false;
}

void gen::MapGenerator::disableCities() {
    _isCitiesEnabled = false;

}

void gen::MapGenerator::disableTowns() {
    _isTownsEnabled = false;

}

void gen::MapGenerator::disableLabels() {
    _isLabelsEnabled = false;

}

void gen::MapGenerator::disableAreaLabels() {
    _isAreaLabelsEnabled = false;

}

void gen::MapGenerator::_initializeVoronoiData() {
    // Boundary vertices in the Voronoi diagram cannot be used as
    // grid nodes and will be excluded from the map grid. Extending the
    // bounds for the sample area will generate a Voronoi diagram that
    // extends past the map bounds, resulting in less excluded grid nodes.
    double pad = _samplePadFactor * _resolution;
    Extents2d sampleExtents(_extents.minx - pad, _extents.miny - pad,
                            _extents.maxx + pad, _extents.maxy + pad);

    //gen::config::print("\tGenerating poisson disc samples...");
    StopWatch timer;
    timer.start();
    std::vector<dcel::Point> samples;
    samples = PoissonDiscSampler::generateSamples(sampleExtents, 
                                                  _resolution, 
                                                  _poissonSamplerKValue);
    timer.stop();
    //gen::config::print("\tFinished generating " + 
                       //gen::config::toString(samples.size()) + " poisson disc "
                       //"samples in " + gen::config::toString(timer.getTime()) + 
                       //" seconds.\n");

    //gen::config::print("\tComputing Delaunay triangulation of " + 
                       //gen::config::toString(samples.size()) + " points...");
    timer.reset();
    timer.start();
    dcel::DCEL triangulation = Delaunay::triangulate(samples);
    timer.stop();
    //gen::config::print("\tFinished computing triangulation in " + 
                       //gen::config::toString(timer.getTime()) + " seconds.\n");

    //gen::config::print("\tComputing Voronoi diagram from delaunay triangulation...");
    timer.reset();
    timer.start();
    _voronoi = Voronoi::delaunayToVoronoi(triangulation);
    timer.stop();
    //gen::config::print("\tFinished computing Voronoi diagram in " + 
                       //gen::config::toString(timer.getTime()) + " seconds.\n");
}

void gen::MapGenerator::_initializeMapData() {
    //gen::config::print("\tInitializing map data...");
    StopWatch timer;
    timer.start();
    _vertexMap = VertexMap(&_voronoi, _extents);
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    _heightMap = NodeMap<double>(vmp, 0);
    _initializeNeighbourMap();
    _initializeFaceNeighbours();
    _initializeFaceVertices();
    _initializeFaceEdges();
    timer.stop();
    //gen::config::print("\tFinished initializing map data in " + 
                       //gen::config::toString(timer.getTime()) + " seconds.");
}

void gen::MapGenerator::_initializeNeighbourMap() {
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    _neighbourMap = NodeMap<std::vector<int> >(vmp);
    std::vector<int> indices;
    indices.reserve(3);
    for (unsigned int i = 0; i < _neighbourMap.size(); i++) {
        indices.clear();
        _vertexMap.getNeighbourIndices(_vertexMap.vertices[i], indices);
        _neighbourMap.set(i, indices);
    }
}

void gen::MapGenerator::_initializeFaceNeighbours() {
    _faceNeighbours.reserve(_voronoi.faces.size());
    dcel::Face f;
    dcel::HalfEdge h;
    std::vector<dcel::HalfEdge> outerComponents;
    std::vector<int> faceindices;
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        f = _voronoi.faces[i];

        outerComponents.clear();
        _voronoi.getOuterComponents(f, outerComponents);
        faceindices.clear();
        for (unsigned int nidx = 0; nidx < outerComponents.size(); nidx++) {
            h = _voronoi.twin(outerComponents[nidx]);
            int nfidx = h.incidentFace.ref;
            if (nfidx != -1) {
                faceindices.push_back(nfidx);
            }
        }

        _faceNeighbours.push_back(std::vector<int>(faceindices.size(), -1));
        for (unsigned int idx = 0; idx < faceindices.size(); idx++) {
            _faceNeighbours[i][idx] = faceindices[idx];
        }
    }
}

void gen::MapGenerator::_initializeFaceVertices() {
    _faceVertices.reserve(_voronoi.faces.size());
    dcel::Face f;
    dcel::Vertex v;
    std::vector<dcel::HalfEdge> edges;
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        f = _voronoi.faces[i];

        edges.clear();
        _voronoi.getOuterComponents(f, edges);
        _faceVertices.push_back(std::vector<int>(edges.size(), -1));
        for (unsigned int eidx = 0; eidx < edges.size(); eidx++) {
            v = _voronoi.origin(edges[eidx]);
            _faceVertices[i][eidx] = v.id.ref;
        }
    }
}

void gen::MapGenerator::_initializeFaceEdges() {
    _faceEdges.reserve(_voronoi.faces.size());
    dcel::Face f;
    std::vector<dcel::HalfEdge> edges;
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        f = _voronoi.faces[i];

        edges.clear();
        _voronoi.getOuterComponents(f, edges);
        _faceEdges.push_back(std::vector<int>(edges.size(), -1));
        for (unsigned int eidx = 0; eidx < edges.size(); eidx++) {
            _faceEdges[i][eidx] = edges[eidx].id.ref;
        }
    }
}

jsoncons::json gen::MapGenerator::_getExtentsJSON() {
    jsoncons::json extents;
    extents["minx"] = _extents.minx;
    extents["miny"] = _extents.miny;
    extents["maxx"] = _extents.maxx;
    extents["maxy"] = _extents.maxy;

    return extents;
}

void gen::MapGenerator::_outputVertices(VertexList &verts, 
                                        std::string filename) {
    std::vector<double> coordinates;
    coordinates.reserve(2*_vertexMap.edge.size());
    for (unsigned int i = 0; i < verts.size(); i++) {
        coordinates.push_back(verts[i].position.x);
        coordinates.push_back(verts[i].position.y);
    }

    jsoncons::json output;
    output["vertices"] = coordinates;
    output["extents"] = _getExtentsJSON();

    std::ofstream file(filename);
    file << output;
    file.close();
}

std::vector<double> gen::MapGenerator::_computeFaceValues(NodeMap<double> &heightMap) {
    std::vector<double> faceheights;
    faceheights.reserve(_voronoi.faces.size());

    dcel::Vertex v;
    for (unsigned int j = 0; j < _voronoi.faces.size(); j++) {

        double sum = 0.0;
        for (unsigned int i = 0; i < _faceVertices[j].size(); i++) {
            v = _voronoi.getVertex(_faceVertices[j][i]);
            if (heightMap.isNode(v)) {
                sum += heightMap(v);
            }
        }
        double avg = sum / _faceVertices[j].size();
        faceheights.push_back(avg);
    }

    return faceheights;
}

std::vector<dcel::Point> gen::MapGenerator::_computeFacePositions() {
    std::vector<dcel::Point> positions;
    positions.reserve(_voronoi.faces.size());

    dcel::Point p;
    for (unsigned int j = 0; j < _voronoi.faces.size(); j++) {
        double sumx = 0.0;
        double sumy = 0.0;
        for (unsigned int i = 0; i < _faceVertices[j].size(); i++) {
            p = _voronoi.getVertex(_faceVertices[j][i]).position;
            sumx += p.x;
            sumy += p.y;
        }
        positions.push_back(dcel::Point(sumx / _faceVertices[j].size(), 
                                        sumy / _faceVertices[j].size()));
    }

    return positions;
}

dcel::Point gen::MapGenerator::_computeFacePosition(int fidx) {
    double sumx = 0.0;
    double sumy = 0.0;
    dcel::Point p;
    for (unsigned int i = 0; i < _faceVertices[fidx].size(); i++) {
        p = _voronoi.getVertex(_faceVertices[fidx][i]).position;
        sumx += p.x;
        sumy += p.y;
    }

    return dcel::Point(sumx / _faceVertices[fidx].size(), 
                       sumy / _faceVertices[fidx].size());
}

bool gen::MapGenerator::_isEdgeInMap(dcel::HalfEdge &h) {
    dcel::Vertex v1 = _voronoi.origin(h);
    dcel::Vertex v2 = _voronoi.origin(_voronoi.twin(h));

    return _vertexMap.isVertex(v1) && _vertexMap.isVertex(v2);
}

bool gen::MapGenerator::_isContourEdge(dcel::HalfEdge &h, 
                                       std::vector<double> &faceheights, 
                                       double isolevel) {
    dcel::Face f1 = _voronoi.incidentFace(h);
    dcel::Face f2 = _voronoi.incidentFace(_voronoi.twin(h));
    double iso1 = faceheights[f1.id.ref];
    double iso2 = faceheights[f2.id.ref];
    bool hasInside = iso1 < isolevel || iso2 < isolevel;
    bool hasOutside = iso1 >= isolevel || iso2 >= isolevel;

    return hasInside && hasOutside;
}

void gen::MapGenerator::_calculateErosionMap(NodeMap<double> &erosionMap) {
    _fillDepressions();
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<double> fluxMap(vmp, 0.0);
    _calculateFluxMap(fluxMap);

    NodeMap<double> slopeMap(vmp, 0.0);
    _calculateSlopeMap(slopeMap);
    for (unsigned int i = 0; i < erosionMap.size(); i++) {
        double flux = fluxMap(i);
        double slope = slopeMap(i);
        double river = _erosionRiverFactor * sqrt(flux) * slope;
        double creep = _erosionCreepFactor * slope * slope;
        double erosion = fmin(river + creep, _maxErosionRate);
        erosionMap.set(i, erosion);
    }

    erosionMap.normalize();
}

void gen::MapGenerator::_calculatePrecipitationMap(NodeMap<double> &precipitationMap) {
    _precipitationNoiseMap.SetNoiseType(FastNoise::Simplex);
    _precipitationNoiseMap.SetSeed(rand()%1000);
    _precipitationNoiseMap.SetFrequency(0.01);

    for (unsigned int i = 0; i < precipitationMap.size(); i++) {
        dcel::Point point = _vertexMap.vertices[i].position;
        double precip = _precipitationNoiseMap.GetNoise(point.x, point.y) ;
        precipitationMap.set(i, .33 * _calculateHeightPrecipitation(i) + .66 * precip);
    }
    
}

double gen::MapGenerator::_calculateHeightPrecipitation(int i) {
    double height = _heightMap(i);

    return 1.0-height*height;
}

void gen::MapGenerator::_calculateTemperatureMap(NodeMap<double> &temperatureMap) {  
    _temperatureNoiseMap.SetNoiseType(FastNoise::Simplex);
    _temperatureNoiseMap.SetSeed(rand() % 1000);
    _temperatureNoiseMap.SetFrequency(.001 * floor((1. - _mapScale) * 10));

    double max = _heightMap.getMax() * (rand() % 3 + 7) / 10.;
    _calculateLatitudeTemperatures(temperatureMap);
}


double gen::MapGenerator::_calculateHeightTemperature(int i, double max) {
    double height = _heightMap(i);

    return 1. -  std::max(0., ((max-height) / max));
}

double gen::MapGenerator::_calculateLatitudeTemperature(int i) {
    dcel::Point point = _vertexMap.vertices[i].position;

    double invheight = 1.0 / (_extents.maxy - _extents.miny); 

    double yLoc = (point.y - _extents.miny) * invheight;
    
    double invwidth = 1.0 / (_extents.maxx - _extents.minx); 

    double xLoc = (point.x - _extents.minx) * invwidth;
    return 1.0 - (abs(.5 - yLoc) / .5);
}

void gen::MapGenerator::_calculateLatitudeTemperatures(NodeMap<double> &temperatureMap) {
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        dcel::Face f = _voronoi.faces[i];

        if (f.outerComponent.ref == -1) {
            continue;
        }

        if (!_isLandFace(i)) {
            continue;
        }
        
        dcel::HalfEdge h = _voronoi.outerComponent(f);
        dcel::Ref startRef = h.id;

        double p = 0.;
        dcel::Vertex v;
        bool outOfBoundsEdge = false;
        double count = 0.;
        do {
            v = _voronoi.origin(h);
            outOfBoundsEdge = !_isEdgeInMap(h);
            p += ((v.position.y - _extents.miny) * invheight);
            h = _voronoi.next(h);
            count++;
        } while (h.id != startRef && !outOfBoundsEdge);
        if (outOfBoundsEdge) {
            continue;
        }
        p /= count;

        // Adjust the point to fit the scale and offset
        p =(_mapScale * p)  + _mapOffset; // World Scale Point

        double dist = abs(.5 - p);
        double temperature = (1.0 - (dist/ .5));
        temperature *= .5 * _calculateVertexNoise(i, _temperatureNoiseMap) + .5;
        temperature -= _calculateHeightTemperature(i, 1.0);
        temperatureMap.set(i, std::max(0., temperature));
    }
}


double gen::MapGenerator::_calculateVertexNoise(int i, FastNoise &noiseMap) {
    dcel::Vertex ver = _vertexMap.vertices[i];
    return (noiseMap.GetNoise(ver.position.x, ver.position.y) + 1.0) / 2.0; // normalize
}

void gen::MapGenerator::_calculateBiomeMap(NodeMap<double> &biomeMap) {
    if (!_isTemperatureCalculated || !_isPrecipitationCalculated) {
        config::print("Temperature and Precipitation must be calculated");
    } else { 
        for (unsigned int i = 0; i < biomeMap.size(); i++) {
            double temp = _temperatureMap(i);
            double precip = _precipitationMap(i);
            double b = 0.0;
            b = _getLifeZone(temp, precip);

            biomeMap.set(i, b);
        }
    }
}

double gen::MapGenerator::_getLifeZone(double temp, double precip) {
    int i = int(floor(temp * 8.0));
    int j = int(floor(precip * 8.0));
    if (i < 2) { // Tundra
        return 1.0;
    } else if (i >=2 && i < 6 && j < 1) { // Grassland-Steppe
        return 2.0;
    } else if ((i >= 6 && j < 1) || (i >= 7 && j < 4)) { // Desert
        return 3.0;
    } else if ((i >= 2 && i < 7 && j >= 1 && j < 2) || (i == 7 && j >=1 && j <3 )) { // Woodland
        return 4.0;
    }  else if (i >= 2 && i < 4 && j >= 2) { // Boreal Forest
        return 5.0;
    } else if (i >= 4 && i < 6 && j >=2 && j <7) { // Seasonal Forest
        return 6.0;
    } else if ((i == 6 && j >=3 && j < 5) || (i == 7 && j >= 4 && j < 6)) { // Savannah
        return 7.0;
    } else { // Rainforest
        return 8.0;
    }

}

void gen::MapGenerator::_getBiomeDrawData(std::vector<jsoncons::json> &faceVertices) {
    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        dcel::Face f = _voronoi.faces[i];

        if (f.outerComponent.ref == -1) {
            continue;
        }

        if (!_isLandFace(i)) {
            continue;
        }
        

        dcel::HalfEdge h = _voronoi.outerComponent(f);
        dcel::Ref startRef = h.id;

        std::vector<double> vertices;
        dcel::Vertex v;
        bool outOfBoundsEdge = false;
        do {
            v = _voronoi.origin(h);
            outOfBoundsEdge = !_isEdgeInMap(h);
            vertices.push_back((v.position.x - _extents.minx) * invwidth);
            vertices.push_back((v.position.y - _extents.miny) * invheight);
            h = _voronoi.next(h);
        } while (h.id != startRef && !outOfBoundsEdge);
        if (outOfBoundsEdge) {
            continue;
        }
        double type = _biomeMap(i);
        faceVertices.push_back(_getBiomeJSON(i, vertices));
    }
}

jsoncons::json gen::MapGenerator::_getBiomeJSON(double i, std::vector<double> &vertices) {
    jsoncons::json json;
    
    double type = _biomeMap(i);
    std::string name = _toBiomeString(type);
    std::vector<double> neighbors;
    _biomeMap.getNeighbours(i, neighbors);

    std::vector<std::string> neighborTypes;

    for(unsigned int i =0; i < neighbors.size(); i++) {
        neighborTypes.push_back(_toBiomeString(neighbors[i]));
    }


    json["name"] = name;
    json["vertices"] = vertices;

    return json;
}

std::string gen::MapGenerator::_toBiomeString(double v) {
    if (v == 1.0) {
        return "Tundra";
    } else if (v == 2.0) {
        return "Grassland";
    } else if (v == 3.0) {
        return "Desert";
    } else if (v == 4.0) {
        return "Steppe/Woodland";
    } else if (v  == 5.0) {
        return "Boreal Forest";
    } else if (v == 6.0) {
        return "Seasonal Forest";
    } else if (v == 7.0) {
        return "Savannah";
    } else {        
        return "Rainforest";
    }
}

void gen::MapGenerator::_fillDepressions() {
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    double maxHeight = _heightMap.getMax();
    NodeMap<double> finalHeightMap(vmp, maxHeight);
    dcel::Vertex v;
    for (unsigned int i = 0; i < _vertexMap.edge.size(); i++) {
        v = _vertexMap.edge[i];
        finalHeightMap.set(v, _heightMap(v));
    }

    double eps = 1e-5;
    std::vector<int> *neighbours;
    for (;;) {
        bool heightUpdated = false;
        for (unsigned int i = 0; i < _heightMap.size(); i++) {
            if (_heightMap(i) == finalHeightMap(i)) {
                continue;
            }

            neighbours = _neighbourMap.getPointer(i);
            for (unsigned int nidx = 0; nidx < neighbours->size(); nidx++) {
                double nval = finalHeightMap(neighbours->at(nidx));
                if (_heightMap(i) >= nval + eps) {
                    finalHeightMap.set(i, _heightMap(i));
                    heightUpdated = true;
                    break;
                }

                double hval = nval + eps;
                if ((finalHeightMap(i) > hval) && (hval > _heightMap(i))) {
                    finalHeightMap.set(i, hval);
                    heightUpdated = true;
                }
            }
        }

        if (!heightUpdated) {
            break;
        }
    }

    _heightMap = finalHeightMap;
}

void gen::MapGenerator::_calculateFlowMap(NodeMap<int> &flowMap) {
    dcel::Vertex v, n;
    std::vector<int> *nbs;
    for (unsigned int i = 0; i < _vertexMap.interior.size(); i++) {
        v = _vertexMap.interior[i];

        nbs = _neighbourMap.getPointer(v);
        dcel::Vertex minVertex;
        double minHeight = _heightMap(v);
        for (unsigned int nidx = 0; nidx < nbs->size(); nidx++) {
            n = _vertexMap.vertices[nbs->at(nidx)];
            if (!_vertexMap.isVertex(n)) {
                continue;
            }

            if (_heightMap(n) < minHeight) {
                minHeight = _heightMap(n);
                minVertex = n;
            }
        }

        flowMap.set(v, flowMap.getNodeIndex(minVertex));
    }
}

void gen::MapGenerator::_calculateFluxMap(NodeMap<double> &fluxMap) {
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<int> flowMap(vmp, -1);
    _calculateFlowMap(flowMap);

    for (unsigned int i = 0; i < flowMap.size(); i++) {
        int next = i;
        while (next != -1) {
            fluxMap.set(next, fluxMap(next) + 1.0);
            next = flowMap(next);
        }
    }

    double maxFlux = _calculateFluxCap(fluxMap);
    for (unsigned int i = 0; i < fluxMap.size(); i++) {
        double f = fluxMap(i);
        f = fmin(maxFlux, f);
        f /= maxFlux;
        fluxMap.set(i, f);
    }

    _fluxMap = fluxMap;
    _flowMap = flowMap;
}

double gen::MapGenerator::_calculateFluxCap(NodeMap<double> &fluxMap) {
    double max = fluxMap.getMax();

    int nbins = 1000;
    std::vector<int> bins(nbins, 0);

    double step = (double)max / (double)nbins;
    double invstep = 1.0 / step;
    for (unsigned int i = 0; i < fluxMap.size(); i++) {
        double f = fluxMap(i);
        int binidx = (int)floor(f * invstep);
        if (binidx >= (int)bins.size()) {
            binidx = bins.size() - 1;
        }
        bins[binidx]++;
    }

    double acc = 0.0;
    double maxflux = 0.0;
    for (int i = 0; i < nbins; i++) {
        double pct = (double)bins[i] / (double)fluxMap.size();
        acc += pct;
        if (acc > _fluxCapPercentile) {
            maxflux = (i + 1)*step;
            break;
        }
    }

    return maxflux;
}

void gen::MapGenerator::_calculateSlopeMap(NodeMap<double> &slopeMap) {
    for (unsigned int i = 0; i < slopeMap.size(); i++) {
        slopeMap.set(i, _calculateSlope(i));
    }
}

double gen::MapGenerator::_calculateSlope(int i) {
    dcel::Vertex v = _vertexMap.vertices[i];
    if (!_vertexMap.isInterior(v)) {
        return 0.0;
    }

    double nx, ny, nz;
    _calculateVertexNormal(i, &nx, &ny, &nz);

    double slope = sqrt(nx*nx + ny*ny);

    return slope;
}

void gen::MapGenerator::performInstructions() {
    gen::config::print("Performing instructions...");

    for(unsigned int i = 0; i < _instructions.size(); i++) {
        gen::MapGenerator::_performInstruction(_instructions[i]);
    }
}

void gen::MapGenerator::_performInstruction(gen::MapInstruction& mapInstruction) {
    if (mapInstruction.FnName == "AddHill") {
        bool multiply = mapInstruction.Params[4] == 1.;
        gen::MapGenerator::addHill(mapInstruction.Params[0], mapInstruction.Params[1], 
                            mapInstruction.Params[2], mapInstruction.Params[3], multiply);
    } else if (mapInstruction.FnName == "addCone") {
        bool multiply = mapInstruction.Params[4] == 1.;
        gen::MapGenerator::addCone(mapInstruction.Params[0], mapInstruction.Params[1], 
                            mapInstruction.Params[2], mapInstruction.Params[3], multiply);
    } else if (mapInstruction.FnName == "addDepression") {
        bool multiply = mapInstruction.Params[4] == 1.;
        gen::MapGenerator::addDepression(mapInstruction.Params[0], mapInstruction.Params[1], 
                            mapInstruction.Params[2], mapInstruction.Params[3], multiply);
    } else if (mapInstruction.FnName == "addPit") {
        bool multiply = mapInstruction.Params[4] == 1.;
        gen::MapGenerator::addPit(mapInstruction.Params[0], mapInstruction.Params[1], 
                            mapInstruction.Params[2], mapInstruction.Params[3], multiply);
    } else if (mapInstruction.FnName == "addSlope") {
        bool multiply = mapInstruction.Params[6] == 1.;
        gen::MapGenerator::addSlope(mapInstruction.Params[0], mapInstruction.Params[1], 
                            mapInstruction.Params[2], mapInstruction.Params[3], mapInstruction.Params[4], 
                            mapInstruction.Params[5], multiply);
    } else if (mapInstruction.FnName == "addNoise") {
        bool multiply = mapInstruction.Params[2] == 1.;
        gen::MapGenerator::addNoise(mapInstruction.Params[0], mapInstruction.Params[1],  multiply);
    } else if (mapInstruction.FnName == "makeContinent") {
        gen::MapGenerator::makeContinent();
    } else if (mapInstruction.FnName == "erode") {
        if (mapInstruction.Params.size() > 0) {
            gen::MapGenerator::erode(mapInstruction.Params[0]);
        } else {
            gen::MapGenerator::erode();
        }
        
    }
}

void gen::MapGenerator::_getContourDrawData(std::vector<std::vector<double> > &data) {
    std::vector<VertexList> paths;
    _getContourPaths(paths);

    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int j = 0; j < paths.size(); j++) {
        std::vector<double> drawPath;
        drawPath.reserve(2*paths[j].size());
        for (unsigned int i = 0; i < paths[j].size(); i++) {
            dcel::Vertex v = paths[j][i];
            double nx = (v.position.x - _extents.minx) * invwidth;
            double ny = (v.position.y - _extents.miny) * invheight;
            drawPath.push_back(nx);
            drawPath.push_back(ny);
        }
        data.push_back(drawPath);
    }
}

void gen::MapGenerator::_getContourPaths(std::vector<VertexList> &paths) {
    std::vector<int> adjacentEdgeCounts(_vertexMap.vertices.size(), 0);
    std::vector<bool> isEdgeVisited(_voronoi.edges.size(), false);
    dcel::HalfEdge h;
    dcel::Vertex v1, v2;
    for (unsigned int i = 0; i < _voronoi.edges.size(); i++) {
        h = _voronoi.edges[i];
        if (!_isEdgeInMap(h) || isEdgeVisited[h.id.ref]) { 
            continue; 
        }

        if (!_isContourEdge(h)) { 
            continue; 
        }

        v1 = _voronoi.origin(h);
        v2 = _voronoi.origin(_voronoi.twin(h));
        int idx1 = _vertexMap.getVertexIndex(v1);
        int idx2 = _vertexMap.getVertexIndex(v2);
        adjacentEdgeCounts[idx1]++;
        adjacentEdgeCounts[idx2]++;

        isEdgeVisited[h.id.ref] = true;
        isEdgeVisited[h.twin.ref] = true;
    }

    std::vector<bool> isEndVertex(_vertexMap.vertices.size(), false);
    std::vector<bool> isContourVertex(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < adjacentEdgeCounts.size(); i++) {
        if (adjacentEdgeCounts[i] == 1) {
            isEndVertex[i] = true;
            isContourVertex[i] = true;
        } else if (adjacentEdgeCounts[i] == 2) {
            isContourVertex[i] = true;
        }
    }

    std::vector<bool> isVertexInContour(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < isEndVertex.size(); i++) {
        if (!isEndVertex[i] || isVertexInContour[i]) {
            continue;
        }

        VertexList path;
        _getContourPath(i, isContourVertex, isEndVertex, isVertexInContour, path);
        paths.push_back(path);
    }

    for (unsigned int i = 0; i < isContourVertex.size(); i++) {
        if (!isContourVertex[i] || isVertexInContour[i]) {
            continue;
        }

        VertexList path;
        _getContourPath(i, isContourVertex, isEndVertex, isVertexInContour, path);
        paths.push_back(path);
    }
}

bool gen::MapGenerator::_isLandFace(int fidx) {
    if (!_isLandFaceTableInitialized) {
        _initializeLandFaceTable();
    }

    return _isLandFaceTable[fidx];
}

void gen::MapGenerator::_initializeLandFaceTable() {
    _getLandFaces(_isLandFaceTable);
    _isLandFaceTableInitialized = true;
}

void gen::MapGenerator::_getLandFaces(std::vector<bool> &isLandFace) {
    std::vector<double> faceHeights;
    _getFaceHeights(faceHeights);

    isLandFace.clear();
    isLandFace.reserve(_voronoi.faces.size());
    for (unsigned int i = 0; i < faceHeights.size(); i++) {
        isLandFace.push_back(_isLand(faceHeights[i]));
    }

    _cleanupLandFaces(isLandFace);    
}

void gen::MapGenerator::_getFaceHeights(std::vector<double> &faceHeights) {
    std::vector<double> faceValues = _computeFaceValues(_heightMap);
    faceHeights.reserve(faceValues.size());
    for (unsigned int i = 0; i < faceValues.size(); i++) {
        faceHeights.push_back(faceValues[i]);
    }
}

bool gen::MapGenerator::_isLand(double isolevel) {
    return isolevel >= _isolevel;
}

void gen::MapGenerator::_cleanupLandFaces(std::vector<bool> &isLandFace) {
    std::vector<std::vector<int> > islands;
    std::vector<bool> isFaceProcessed(_voronoi.faces.size(), false);
    std::vector<int> connectedFaces;
    for (unsigned int i = 0; i < isLandFace.size(); i++) {
        if (isFaceProcessed[i]) {
            continue;
        }

        connectedFaces.clear();
        _getConnectedFaces(i, isLandFace, isFaceProcessed, connectedFaces);
        islands.push_back(connectedFaces);
    }

    for (unsigned int j = 0; j < islands.size(); j++) {
        if (islands[j].size() >= _minIslandFaceThreshold) {
            continue;
        }

        for (unsigned int i = 0; i < islands[j].size(); i++) {
            int fidx = islands[j][i];
            isLandFace[fidx] = !isLandFace[fidx];
        }
    }
}

void gen::MapGenerator::_getConnectedFaces(int seed, 
                                           std::vector<bool> &isLandFace,
                                           std::vector<bool> &isFaceProcessed,
                                           std::vector<int> &faces) {
    std::vector<int> queue;
    queue.push_back(seed);
    isFaceProcessed[seed] = true;

    while (!queue.empty()) {
        int fidx = queue.back();
        queue.pop_back();

        bool faceType = isLandFace[fidx];
        for (unsigned int nidx = 0; nidx < _faceNeighbours[fidx].size(); nidx++) {
            int nfidx = _faceNeighbours[fidx][nidx];
            if ((isLandFace[nfidx] == faceType) && !isFaceProcessed[nfidx]) {
                queue.push_back(nfidx);
                isFaceProcessed[nfidx] = true;
            }
        }

        faces.push_back(fidx);
    }
}


bool gen::MapGenerator::_isContourEdge(dcel::HalfEdge &h) {
    dcel::Face f1 = _voronoi.incidentFace(h);
    dcel::Face f2 = _voronoi.incidentFace(_voronoi.twin(h));
    return (_isLandFace(f1.id.ref) && !_isLandFace(f2.id.ref)) ||
           (_isLandFace(f2.id.ref) && !_isLandFace(f1.id.ref));
}

bool gen::MapGenerator::_isContourEdge(dcel::Vertex &v1, dcel::Vertex &v2) {
    std::vector<dcel::HalfEdge> edges;
    edges.reserve(3);
    _voronoi.getIncidentEdges(v1, edges);

    dcel::HalfEdge h;
    dcel::Vertex v;
    for (unsigned int i = 0; i < edges.size(); i++) {
        h = edges[i];
        v = _voronoi.origin(_voronoi.twin(h));
        if (v.id.ref == v2.id.ref) {
            return _isContourEdge(h);
        }
    }

    return false;
}

void gen::MapGenerator::_getContourPath(int seed, std::vector<bool> &isContourVertex, 
                                                  std::vector<bool> &isEndVertex, 
                                                  std::vector<bool> &isVertexInContour, 
                                                  VertexList &path) {
    dcel::Vertex v = _vertexMap.vertices[seed];
    dcel::Vertex lastVertex = v;

    std::vector<int> *nbs;
    for (;;) {
        path.push_back(v);
        isVertexInContour[_vertexMap.getVertexIndex(v)] = true;
        
        nbs = _neighbourMap.getPointer(v);
        bool isFound = false;
        for (unsigned int i = 0; i < nbs->size(); i++) {
            dcel::Vertex n = _vertexMap.vertices[nbs->at(i)];
            int nidx = _vertexMap.getVertexIndex(n);
            if (n.id.ref != lastVertex.id.ref && isContourVertex[nidx] && 
                    _isContourEdge(v, n)) {
                lastVertex = v;
                v = n;
                isFound = true;
                break;
            }
        }

        if (!isFound) {
            break;
        }

        int vidx = _vertexMap.getVertexIndex(v);
        if (isEndVertex[vidx] || isVertexInContour[vidx]) {
            path.push_back(v);
            isVertexInContour[vidx] = true;
            break;
        }
    }
}

void gen::MapGenerator::_getRiverDrawData(std::vector<std::vector<double> > &data) {
    std::vector<VertexList> riverPaths;
    _getRiverPaths(riverPaths);

    for (unsigned int i = 0; i < riverPaths.size(); i++) {
        riverPaths[i] = _smoothPath(riverPaths[i], _riverSmoothingFactor);
    }

    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int j = 0; j < riverPaths.size(); j++) {
        std::vector<double> drawPath;
        drawPath.reserve(2*riverPaths[j].size());
        for (unsigned int i = 0; i < riverPaths[j].size(); i++) {
            dcel::Vertex v = riverPaths[j][i];
            double nx = (v.position.x - _extents.minx) * invwidth;
            double ny = (v.position.y - _extents.miny) * invheight;
            drawPath.push_back(nx);
            drawPath.push_back(ny);
        }
        data.push_back(drawPath);
    }
}

void gen::MapGenerator::_getRiverPaths(std::vector<VertexList> &riverPaths) {
    VertexList riverVertices;
    _getRiverVertices(riverVertices);

    std::vector<bool> isVertexInRiver(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < riverVertices.size(); i++) {
        int idx = _vertexMap.getVertexIndex(riverVertices[i]);
        isVertexInRiver[idx] = true;
    }

    VertexList fixedVertices;
    _getFixedRiverVertices(riverVertices, fixedVertices);
    std::vector<bool> isFixedVertex(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < fixedVertices.size(); i++) {
        int vidx = _vertexMap.getVertexIndex(fixedVertices[i]);
        isFixedVertex[vidx] = true;
    }

    VertexList path;
    for (unsigned int i = 0; i < isFixedVertex.size(); i++) {
        if (!isFixedVertex[i]) {
            continue;
        }

        path.clear();
        int next = i;
        while (isVertexInRiver[next]) {
            path.push_back(_vertexMap.vertices[next]);
            next = _flowMap(next);

            if (next == -1) { break; }
            if (isFixedVertex[next]) {
                path.push_back(_vertexMap.vertices[next]);
                break;
            }
        }

        if (path.size() >= 2) {
            riverPaths.push_back(path);
        }
    }
}

void gen::MapGenerator::_getRiverVertices(VertexList &vertices) {
    std::vector<bool> isVertexAdded(_vertexMap.vertices.size(), false);

    dcel::Vertex v;
    VertexList pathVertices;
    for (unsigned int i = 0; i < _vertexMap.vertices.size(); i++) {
        v = _vertexMap.vertices[i];
        if (_fluxMap(v) < _riverFluxThreshold || _isCoastVertex(i)) {
            continue;
        }

        int next = _flowMap.getNodeIndex(v);
        pathVertices.clear();
        while (next != -1) {
            pathVertices.push_back(_vertexMap.vertices[next]);
            if (_isCoastVertex(next)) { 
                break; 
            }
            if (!_isLandVertex(next)) { 
                pathVertices.clear();
                break; 
            }
            next = _flowMap(next);
        }

        if (pathVertices.empty()) { continue; }

        for (unsigned int i = 0; i < pathVertices.size(); i++) {
            int idx = _vertexMap.getVertexIndex(pathVertices[i]);
            if (!isVertexAdded[idx]) {
                vertices.push_back(pathVertices[i]);
                isVertexAdded[idx] = true;
            }
        }
    }
}

bool gen::MapGenerator::_isLandVertex(int vidx) {
    std::vector<dcel::Face> faces;
    faces.reserve(6);
    _voronoi.getIncidentFaces(_vertexMap.vertices[vidx], faces);

    for (unsigned int i = 0; i < faces.size(); i++) {
        if (_isLandFace(faces[i].id.ref)) {
            return true;
        }
    }

    return false;
}

bool gen::MapGenerator::_isCoastVertex(int vidx) {
    std::vector<dcel::Face> faces;
    faces.reserve(6);
    _voronoi.getIncidentFaces(_vertexMap.vertices[vidx], faces);

    bool hasLand = false;
    bool hasSea = false;
    for (unsigned int i = 0; i < faces.size(); i++) {
        if (_isLandFace(faces[i].id.ref)) {
            hasLand = true;
        } else {
            hasSea = true;
        }
    }

    return hasLand && hasSea;
}

void gen::MapGenerator::_getFixedRiverVertices(VertexList &riverVertices, 
                                               VertexList &fixedVertices) {
    std::vector<bool> isVertexInRiver(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < riverVertices.size(); i++) {
        int idx = _vertexMap.getVertexIndex(riverVertices[i]);
        isVertexInRiver[idx] = true;
    }

    std::vector<bool> isEdgeProcessed(_voronoi.edges.size(), false);
    std::vector<int> adjacentEdgeCounts(_vertexMap.vertices.size(), 0);

    dcel::HalfEdge h;
    dcel::Vertex p1, p2;
    for (unsigned int i = 0; i < _voronoi.edges.size(); i++) {
        h = _voronoi.edges[i];
        if (!_isEdgeInMap(h) || isEdgeProcessed[h.id.ref]) { 
            continue; 
        }

        p1 = _voronoi.origin(h);
        p2 = _voronoi.origin(_voronoi.twin(h));
        int idx1 = _vertexMap.getVertexIndex(p1);
        int idx2 = _vertexMap.getVertexIndex(p2);

        if (!isVertexInRiver[idx1] || !isVertexInRiver[idx2]) {
            continue;
        }

        adjacentEdgeCounts[idx1]++;
        adjacentEdgeCounts[idx2]++;

        isEdgeProcessed[h.id.ref] = true;
        isEdgeProcessed[h.twin.ref] = true;
    }

    for (unsigned int i = 0; i < adjacentEdgeCounts.size(); i++) {
        if (adjacentEdgeCounts[i] == 1 || adjacentEdgeCounts[i] == 3) {
            fixedVertices.push_back(_vertexMap.vertices[i]);
        }
    }
}

gen::MapGenerator::VertexList gen::MapGenerator::_smoothPath(VertexList &path,
                                                             double factor) {
    if (path.size() < 2) {
        return path;
    }

    VertexList smoothedPath = path;
    for (unsigned int i = 1; i < path.size() - 1; i++) {
        dcel::Point v0 = path[i-1].position;
        dcel::Point v1 = path[i].position;
        dcel::Point v2 = path[i+1].position;

        v1.x = (1-factor)*v1.x + factor*0.5*(v0.x + v2.x);
        v1.y = (1-factor)*v1.y + factor*0.5*(v0.y + v2.y);

        smoothedPath[i].position = v1;
    }

    return smoothedPath;
}

void gen::MapGenerator::_getSlopeDrawData(std::vector<double> &data) {
    std::vector<Segment> slopeSegments;
    _getSlopeSegments(slopeSegments);

    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int i = 0; i < slopeSegments.size(); i++) {
        Segment s = slopeSegments[i];

        double nx1 = (s.p1.x - _extents.minx) * invwidth;
        double ny1 = (s.p1.y - _extents.miny) * invheight;
        double nx2 = (s.p2.x - _extents.minx) * invwidth;
        double ny2 = (s.p2.y - _extents.miny) * invheight;

        data.push_back(nx1);
        data.push_back(ny1);
        data.push_back(nx2);
        data.push_back(ny2);
    }
}

void gen::MapGenerator::_getSlopeSegments(std::vector<Segment> &segments) {
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<double> slopeMap(vmp, 0.0);
    _calculateHorizontalSlopeMap(slopeMap);

    NodeMap<double> nearSlopeMap(vmp, 0.0);
    _calculateVerticalSlopeMap(nearSlopeMap);

    std::vector<double> faceSlopes = _computeFaceValues(slopeMap);
    std::vector<double> nearSlopes = _computeFaceValues(nearSlopeMap);
    std::vector<dcel::Point> facePositions = _computeFacePositions();

    for (unsigned int i = 0; i < faceSlopes.size(); i++) {
        double slope = faceSlopes[i];
        if (!_isLandFace(i) || fabs(slope) < _minSlopeThreshold) {
            continue;
        }

        double factor = (fabs(slope) - _minSlope) / (_maxSlope - _minSlope);
        factor = fmin(1.0, factor);
        factor = fmax(0.0, factor);

        double angle = _minSlopeAngle + factor * (_maxSlopeAngle - _minSlopeAngle);
        angle = slope < 0 ? angle : -angle;

        double dirx = cos(angle);
        double diry = sin(angle);
        
        double minlength = _minSlopeLength*_resolution;
        double maxlength = _maxSlopeLength*_resolution;
        double minv = _minVerticalSlope;
        double maxv = _maxVerticalSlope;
        double vslope = nearSlopes[i];
        double nf = (vslope - minv) / (maxv - minv);
        nf = fmin(1.0, nf);
        nf = fmax(0.0, nf);
        double length = minlength + nf * (maxlength - minlength);

        Segment s;
        s.p1 = facePositions[i];
        s.p2 = dcel::Point(s.p1.x + dirx*length, s.p1.y + diry*length);
        segments.push_back(s);
    }
}

void gen::MapGenerator::_calculateHorizontalSlopeMap(NodeMap<double> &slopeMap) {
    for (unsigned int i = 0; i < slopeMap.size(); i++) {
        slopeMap.set(i, _calculateHorizontalSlope(i));
    }
}

double gen::MapGenerator::_calculateHorizontalSlope(int i) {
    dcel::Vertex v = _vertexMap.vertices[i];
    if (!_vertexMap.isInterior(v)) {
        return 0.0;
    }

    double nx, ny, nz;
    _calculateVertexNormal(i, &nx, &ny, &nz);

    return nx;
}

void gen::MapGenerator::_calculateVerticalSlopeMap(NodeMap<double> &slopeMap) {
    for (unsigned int i = 0; i < slopeMap.size(); i++) {
        slopeMap.set(i, _calculateVerticalSlope(i));
    }
}

double gen::MapGenerator::_calculateVerticalSlope(int i) {
    dcel::Vertex v = _vertexMap.vertices[i];
    if (!_vertexMap.isInterior(v)) {
        return 0.0;
    }

    double nx, ny, nz;
    _calculateVertexNormal(i, &nx, &ny, &nz);

    return ny;
}

void gen::MapGenerator::_calculateVertexNormal(int vidx, 
                                               double *nx, double *ny, double *nz) {
    std::vector<int> *nbs = _neighbourMap.getPointer(vidx);
    if (nbs->size() != 3) {
        return;
    }

    dcel::Point p0 = _vertexMap.vertices[nbs->at(0)].position;
    dcel::Point p1 = _vertexMap.vertices[nbs->at(1)].position;
    dcel::Point p2 = _vertexMap.vertices[ nbs->at(2)].position;

    double v0x = p1.x - p0.x;
    double v0y = p1.y - p0.y;
    double v0z = _heightMap(nbs->at(1)) - _heightMap(nbs->at(0));
    double v1x = p2.x - p0.x;
    double v1y = p2.y - p0.y;
    double v1z = _heightMap(nbs->at(2)) - _heightMap(nbs->at(0));

    double vnx = v0y*v1z - v0z*v1y;
    double vny = v0z*v1x - v0x*v1z;
    double vnz = v0x*v1y - v0y*v1x;
    double invlen = 1.0 / sqrt(vnx*vnx + vny*vny + vnz*vnz);
    *nx = vnx * invlen;
    *ny = vny * invlen;
    *nz = vnz * invlen;
}

void gen::MapGenerator::_getCityDrawData(std::vector<double> &data) {
    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int i = 0; i < _cities.size(); i++) {
        dcel::Point p = _cities[i].position;
        double nx = (p.x - _extents.minx) * invwidth;
        double ny = (p.y - _extents.miny) * invheight;

        data.push_back(nx);
        data.push_back(ny);
    }
}

void gen::MapGenerator::_getTownDrawData(std::vector<double> &data) {
    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int i = 0; i < _towns.size(); i++) {
        dcel::Point p = _towns[i].position;
        double nx = (p.x - _extents.minx) * invwidth;
        double ny = (p.y - _extents.miny) * invheight;

        data.push_back(nx);
        data.push_back(ny);
    }
}

gen::MapGenerator::CityLocation gen::MapGenerator::_getCityLocation() {
    std::shared_ptr<VertexMap> vmp = std::make_shared<VertexMap>(_vertexMap);
    NodeMap<double> cityScores(vmp, 0.0);
    _getCityScores(cityScores);

    std::vector<double> faceScores = _computeFaceValues(cityScores);
    std::vector<dcel::Point> facePositions = _computeFacePositions();
    double maxScore = -std::numeric_limits<double>::infinity();
    int cityfidx = -1;
    for (unsigned int i = 0; i < faceScores.size(); i++) {
        dcel::Point fp = facePositions[i];
        if (_extents.containsPoint(fp) && faceScores[i] > maxScore) {
            maxScore = faceScores[i];
            cityfidx = i;
        }
    }

    CityLocation loc;
    loc.position = _computeFacePosition(cityfidx);
    loc.faceid = cityfidx;

    return loc;
}

void gen::MapGenerator::_getCityScores(NodeMap<double> &cityScores) {
    NodeMap<double> fluxMap = _fluxMap;
    fluxMap.relax();

    double neginf = -1e2;
    double eps = 1e-6;
    dcel::Point p;
    for (unsigned int i = 0; i < cityScores.size(); i++) {
        double score = 0.0;
        if (!_isLandVertex(i) || _isCoastVertex(i)) {
            score += neginf;
        }

        score += _fluxScoreBonus * sqrt(fluxMap(i));

        p = _vertexMap.vertices[i].position;
        double extentsDist = fmax(0.0, _pointToEdgeDistance(p));
        score -= _nearEdgeScorePenalty * (1.0 / (extentsDist + eps));

        for (unsigned int cidx = 0; cidx < _cities.size(); cidx++) {
            dcel::Point cp = _cities[cidx].position;
            double dist = fmin(_getPointDistance(p, cp), _maxPenaltyDistance);
            double distfactor = 1 - dist / _maxPenaltyDistance;
            score -= _nearCityScorePenalty * distfactor;
        }

        for (unsigned int tidx = 0; tidx < _towns.size(); tidx++) {
            dcel::Point tp = _towns[tidx].position;
            double dist = fmin(_getPointDistance(p, tp), _maxPenaltyDistance);
            double distfactor = 1 - dist / _maxPenaltyDistance;
            score -= _nearTownScorePenalty * distfactor;
        }

        score = fmax(neginf, score);
        cityScores.set(i, score);
    }
}

double gen::MapGenerator::_getPointDistance(dcel::Point &p1, dcel::Point &p2) {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    return sqrt(dx*dx + dy*dy);
}

double gen::MapGenerator::_pointToEdgeDistance(dcel::Point p) {
    double mindist = std::numeric_limits<double>::infinity();
    mindist = fmin(mindist, p.x - _extents.minx);
    mindist = fmin(mindist, _extents.maxx - p.x);
    mindist = fmin(mindist, p.y - _extents.miny);
    mindist = fmin(mindist, _extents.maxy - p.y);

    return mindist;
}

void gen::MapGenerator::_updateCityMovementCost(City &city) {
    std::vector<double> faceHeights;
    _getFaceHeights(faceHeights);
    std::vector<double> faceFlux = _computeFaceValues(_fluxMap);
    std::vector<dcel::Point> facePositions = _computeFacePositions();
    std::vector<bool> isFaceInMap(_voronoi.faces.size(), false);
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        isFaceInMap[i] = _extents.containsPoint(facePositions[i]);
    }

    double inf = std::numeric_limits<double>::infinity();
    std::vector<double> movementCosts(_voronoi.faces.size(), inf);
    std::vector<int> parents(_voronoi.faces.size(), -1);
    int rootid = city.faceid;
    movementCosts[rootid] = 0;
    std::queue<int> queue;
    queue.push(rootid);

    while (!queue.empty()) {
        int fidx = queue.front();
        queue.pop();

        for (unsigned int idx = 0; idx < _faceNeighbours[fidx].size(); idx++) {
            int nidx = _faceNeighbours[fidx][idx];
            if (!isFaceInMap[nidx] || movementCosts[nidx] != inf) {
                continue;
            }

            double cost = 0.0;
            double hdist = _getPointDistance(facePositions[nidx], facePositions[fidx]);
            double hcost = _isLandFace(nidx) ? _landDistanceCost : _seaDistanceCost;
            cost += hcost * hdist;

            if (_isLandFace(nidx)) {
                double udist = faceHeights[nidx] - faceHeights[fidx];
                double ucost = udist > 0.0 ? _uphillCost : _downhillCost;
                cost += (udist / hdist) * (udist / hdist) * ucost;
                cost += sqrt(faceFlux[nidx]) * _fluxCost;
            }

            if ((_isLandFace(fidx) && !_isLandFace(nidx)) || 
                (_isLandFace(nidx) && !_isLandFace(fidx))) {
                cost += _landTransitionCost;
            }

            movementCosts[nidx] = movementCosts[fidx] + cost;
            parents[nidx] = fidx;
            queue.push(nidx);
        }
    }

    city.movementCosts = movementCosts;
}

void gen::MapGenerator::_getTerritoryDrawData(
                            std::vector<std::vector<double> > &data) {
    if (_cities.size() == 0) {
        return;
    }

    std::vector<VertexList> borders;
    _getTerritoryBorders(borders);

    for (unsigned int i = 0; i < borders.size(); i++) {
        borders[i] = _smoothPath(borders[i], _territoryBorderSmoothingFactor);
    }

    double invwidth = 1.0 / (_extents.maxx - _extents.minx);
    double invheight = 1.0 / (_extents.maxy - _extents.miny);
    for (unsigned int j = 0; j < borders.size(); j++) {
        std::vector<double> drawPath;
        drawPath.reserve(2*borders[j].size());
        for (unsigned int i = 0; i < borders[j].size(); i++) {
            dcel::Vertex v = borders[j][i];
            double nx = (v.position.x - _extents.minx) * invwidth;
            double ny = (v.position.y - _extents.miny) * invheight;
            drawPath.push_back(nx);
            drawPath.push_back(ny);
        }
        data.push_back(drawPath);
    }
}

void gen::MapGenerator::_getTerritoryBorders(std::vector<VertexList> &borders) {
    std::vector<int> faceTerritories(_voronoi.faces.size(), -1);
    _getFaceTerritories(faceTerritories);
    _territoryData = faceTerritories;

    _getBorderPaths(faceTerritories, borders);
}

void gen::MapGenerator::_getFaceTerritories(std::vector<int> &faceTerritories) {
    std::vector<dcel::Point> facePositions = _computeFacePositions();
    std::vector<bool> isFaceInMap(_voronoi.faces.size(), false);
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        isFaceInMap[i] = _extents.containsPoint(facePositions[i]);
    }

    for (unsigned int i = 0; i < faceTerritories.size(); i++) {
        if (!isFaceInMap[i]) {
            continue;
        }

        if (!_isLandFace(i)) {
            continue;
        }

        double mincost = std::numeric_limits<double>::infinity();
        int mincidx = -1;
        for (unsigned int j = 0; j < _cities.size(); j++) {
            if (_cities[j].movementCosts[i] < mincost) {
                mincost = _cities[j].movementCosts[i];
                mincidx = j;
            }
        }

        faceTerritories[i] = mincidx;
    }

    _cleanupFaceTerritories(faceTerritories);
}

void gen::MapGenerator::_cleanupFaceTerritories(std::vector<int> &faceTerritories) {
    for (int i = 0; i < _numTerritoryBorderSmoothingInterations; i++) {
        _smoothTerritoryBoundaries(faceTerritories);
    }

    std::vector<std::vector<int> > connectedTerritories;
    _getConnectedTerritories(faceTerritories, connectedTerritories);

    std::vector<std::vector<int> > disjointTerritories;
    _getDisjointTerritories(faceTerritories, 
                            connectedTerritories, 
                            disjointTerritories);
    _claimDisjointTerritories(disjointTerritories, faceTerritories);
}

void gen::MapGenerator::_smoothTerritoryBoundaries(
                            std::vector<int> &faceTerritories) {
    std::vector<int> tempFaceTerritories = faceTerritories;
    std::vector<int> neighbourCounts(_cities.size(), 0);
    for (unsigned int fidx = 0; fidx < faceTerritories.size(); fidx++) {
        if (faceTerritories[fidx] == -1) {
            continue;
        }

        std::fill(neighbourCounts.begin(), neighbourCounts.end(), 0);
        for (unsigned int idx = 0; idx < _faceNeighbours[fidx].size(); idx++) {
            int nidx = _faceNeighbours[fidx][idx];
            if (faceTerritories[nidx] == -1) {
                continue;
            }

            neighbourCounts[faceTerritories[nidx]]++;
        }

        int majorityTerritory = faceTerritories[fidx];
        int majorityCount = neighbourCounts[faceTerritories[fidx]];
        for (unsigned int cidx = 0; cidx < neighbourCounts.size(); cidx++) {
            if (neighbourCounts[cidx] > majorityCount) {
                majorityCount = neighbourCounts[cidx];
                majorityTerritory = cidx;
            }
        }

        tempFaceTerritories[fidx] = majorityTerritory;
    }

    for (unsigned int i = 0; i < faceTerritories.size(); i++) {
        faceTerritories[i] = tempFaceTerritories[i];
    }
}

void gen::MapGenerator::_getConnectedTerritories(
                            std::vector<int> &faceTerritories,
                            std::vector<std::vector<int> > &connected) {
    std::vector<bool> isFaceProcessed(faceTerritories.size(), false);
    for (unsigned int fidx = 0; fidx < faceTerritories.size(); fidx++) {
        if (faceTerritories[fidx] == -1 || isFaceProcessed[fidx]) {
            continue;
        }

        std::vector<int> faces;
        _getConnectedTerritory(fidx, faceTerritories, isFaceProcessed, faces);
        connected.push_back(faces);
    }
}

void gen::MapGenerator::_getConnectedTerritory(int fidx, 
                                               std::vector<int> &faceTerritories,
                                               std::vector<bool> &isFaceProcessed,
                                               std::vector<int> &connectedFaces) {
    int territoryID = faceTerritories[fidx];
    std::vector<int> queue;
    queue.push_back(fidx);
    isFaceProcessed[fidx] = true;

    while (!queue.empty()) {
        fidx = queue.back();
        queue.pop_back();

        for (unsigned int idx = 0; idx < _faceNeighbours[fidx].size(); idx++) {
            int nidx = _faceNeighbours[fidx][idx];
            if (faceTerritories[nidx] == -1) {
                continue;
            }

            if (!isFaceProcessed[nidx] && faceTerritories[nidx] == territoryID) {
                queue.push_back(nidx);
                isFaceProcessed[nidx] = true;
            }
        }

        connectedFaces.push_back(fidx);
    }
}

void gen::MapGenerator::_getDisjointTerritories(
                            std::vector<int> &faceTerritories,
                            std::vector<std::vector<int> > &connected, 
                            std::vector<std::vector<int> > &disjoint) {
    for (unsigned int i = 0; i < connected.size(); i++) {
        int cityidx = faceTerritories[connected[i][0]];
        int cityfaceidx = _cities[cityidx].faceid;

        bool containsCity = std::find(connected[i].begin(), 
                                      connected[i].end(), 
                                      cityfaceidx) != connected[i].end();
        if (!containsCity) {
            disjoint.push_back(connected[i]);
        }
    }
}

void gen::MapGenerator::_claimDisjointTerritories(
                            std::vector<std::vector<int> > &disjoint,
                            std::vector<int> &faceTerritories) {
    for (unsigned int i = 0; i < disjoint.size(); i++) {
        int cidx = _getTerritoryOwner(disjoint[i], faceTerritories);
        for (unsigned int j = 0; j < disjoint[i].size(); j++) {
            faceTerritories[disjoint[i][j]] = cidx;
        }
    }
}

int gen::MapGenerator::_getTerritoryOwner(std::vector<int> &territory,
                                          std::vector<int> &faceTerritories) {
    std::vector<bool> isFaceProcessed(faceTerritories.size(), false);
    for (unsigned int i = 0; i < territory.size(); i++) {
        isFaceProcessed[territory[i]] = true;
    }

    std::vector<int> cityNeighbourCounts(_cities.size(), 0);
    for (unsigned int i = 0; i < territory.size(); i++) {
        int fidx = territory[i];

        for (unsigned int idx = 0; idx < _faceNeighbours[fidx].size(); idx++) {
            int nidx = _faceNeighbours[fidx][idx];
            if (faceTerritories[nidx] == -1 || isFaceProcessed[nidx]) {
                continue;
            }
            cityNeighbourCounts[faceTerritories[nidx]]++;
            isFaceProcessed[nidx] = true;
        }
    }

    int majorityCount = 0;
    int majorityCity = -1;
    for (unsigned int i = 0; i < cityNeighbourCounts.size(); i++) {
        if (cityNeighbourCounts[i] > majorityCount) {
            majorityCount = cityNeighbourCounts[i];
            majorityCity = i;
        }
    }

    return majorityCity;
}

void gen::MapGenerator::_getBorderPaths(std::vector<int> &faceTerritories, 
                                        std::vector<VertexList> &borders) {
    std::vector<dcel::HalfEdge> borderEdges;
    _getBorderEdges(faceTerritories, borderEdges);

    std::vector<int> vertexBorderCounts(_vertexMap.vertices.size(), 0);
    dcel::HalfEdge h;
    dcel::Vertex v1, v2;
    for (unsigned int i = 0; i < borderEdges.size(); i++) {
        h = borderEdges[i];
        v1 = _voronoi.origin(h);
        v2 = _voronoi.origin(_voronoi.twin(h));
        int vidx1 = _vertexMap.getVertexIndex(v1);
        int vidx2 = _vertexMap.getVertexIndex(v2);

        vertexBorderCounts[vidx1]++;
        vertexBorderCounts[vidx2]++;
    }

    std::vector<bool> isEndVertex(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < vertexBorderCounts.size(); i++) {
        if (vertexBorderCounts[i] == 1 || vertexBorderCounts[i] == 3) {
            isEndVertex[i] = true;
        }
    }

    std::vector<bool> isVertexProcessed(_vertexMap.vertices.size(), false);
    for (unsigned int i = 0; i < isEndVertex.size(); i++) {
        if (!isEndVertex[i]) {
            continue;
        }

        for (unsigned int idx = 0; idx < 3; idx++) {
            VertexList path;
            _getBorderPath(i, faceTerritories, 
                              isEndVertex, 
                              isVertexProcessed,
                              path);

            if (path.size() > 0) {
                borders.push_back(path);
            }
        }
    }
}

void gen::MapGenerator::_getBorderEdges(std::vector<int> &faceTerritories, 
                                        std::vector<dcel::HalfEdge> &borderEdges) {
    std::vector<bool> isEdgeVisited(_voronoi.edges.size(), false);
    dcel::HalfEdge h;
    for (unsigned int i = 0; i < _voronoi.edges.size(); i++) {
        h = _voronoi.edges[i];
        if (!_isEdgeInMap(h) || isEdgeVisited[h.id.ref]) { 
            continue; 
        }

        if (_isBorderEdge(h, faceTerritories)) { 
            borderEdges.push_back(h);
            isEdgeVisited[h.id.ref] = true;
            isEdgeVisited[h.twin.ref] = true;
        }
    }
}

bool gen::MapGenerator::_isBorderEdge(dcel::HalfEdge &h, 
                                      std::vector<int> &faceTerritories) {
    dcel::Face f1 = _voronoi.incidentFace(h);
    dcel::Face f2 = _voronoi.incidentFace(_voronoi.twin(h));
    int city1 = faceTerritories[f1.id.ref];
    int city2 = faceTerritories[f2.id.ref];
    
    if (city1 == -1 || city2 == -1) {
        return false;
    }

    return city1 != city2;
}

bool gen::MapGenerator::_isBorderEdge(dcel::Vertex &v1, dcel::Vertex &v2, 
                                      std::vector<int> &faceTerritories) {
    std::vector<dcel::HalfEdge> edges;
    edges.reserve(3);
    _voronoi.getIncidentEdges(v1, edges);

    dcel::HalfEdge h;
    dcel::Vertex v;
    for (unsigned int i = 0; i < edges.size(); i++) {
        h = edges[i];
        v = _voronoi.origin(_voronoi.twin(h));
        if (v.id.ref == v2.id.ref) {
            return _isBorderEdge(h, faceTerritories);
        }
    }

    return false;
}

void gen::MapGenerator::_getBorderPath(int vidx, 
                                       std::vector<int> &faceTerritories, 
                                       std::vector<bool> &isEndVertex, 
                                       std::vector<bool> &isVertexProcessed,
                                       VertexList &path) {

    dcel::Vertex v = _vertexMap.vertices[vidx];
    dcel::Vertex lastVertex = v;

    std::vector<int> *nbs;
    for (;;) {
        path.push_back(v);
        isVertexProcessed[_vertexMap.getVertexIndex(v)] = true;
        
        nbs = _neighbourMap.getPointer(v);
        bool isFound = false;
        for (unsigned int i = 0; i < nbs->size(); i++) {
            dcel::Vertex n = _vertexMap.vertices[nbs->at(i)];
            int nidx = _vertexMap.getVertexIndex(n);
            if (n.id.ref != lastVertex.id.ref && 
                            _isBorderEdge(v, n, faceTerritories) && 
                            !isVertexProcessed[nidx]) {
                lastVertex = v;
                v = n;
                isFound = true;
                break;
            }
        }

        if (!isFound) {
            for (unsigned int i = 0; i < nbs->size(); i++) {
                dcel::Vertex n = _vertexMap.vertices[nbs->at(i)];;
                int nidx = _vertexMap.getVertexIndex(n);
                if (isEndVertex[nidx]) {
                    path.push_back(n);
                    isVertexProcessed[nidx] = true;
                }
            }

            if (path.size() < 2) {
                path.clear();
            }
            break;
        }

        int vidx = _vertexMap.getVertexIndex(v);
        if (isEndVertex[vidx]) {
            path.push_back(v);
            isVertexProcessed[vidx] = true;
            break;
        }
    }
}

void gen::MapGenerator::_getLabelDrawData(std::vector<jsoncons::json> &data) {
    std::vector<Label> labels;
    _initializeLabels(labels);
    if (labels.size() == 0) {
        return;
    }

    _generateLabelPlacements(labels);

    std::vector<jsoncons::json> jsondata;
    for (unsigned int i = 0; i < labels.size(); i++) {
        LabelCandidate label = labels[i].candidates[labels[i].candidateIdx];
        label.baseScore = labels[i].score;
        data.push_back(_getLabelJSON(label));
    }
}

void gen::MapGenerator::_initializeLabels(std::vector<Label> &labels) {
    std::vector<Label> markerLabels;
    std::vector<Label> areaLabels;
    _initializeMarkerLabels(markerLabels);

    if (_isAreaLabelsEnabled) {
        _initializeAreaLabels(areaLabels);
    }

    labels.insert(labels.end(), markerLabels.begin(), markerLabels.end());
    labels.insert(labels.end(), areaLabels.begin(), areaLabels.end());
}

void gen::MapGenerator::_initializeMarkerLabels(std::vector<Label> &labels) {
    _fontData.setFontFace(_cityLabelFontFace, _cityLabelFontSize);
    for (unsigned int i = 0; i < _cities.size(); i++) {
        Label cityLabel;
        _initializeCityLabel(_cities[i], cityLabel);
        labels.push_back(cityLabel);
    }

    _fontData.setFontFace(_townLabelFontFace, _townLabelFontSize);
    for (unsigned int i = 0; i < _towns.size(); i++) {
        Label townLabel;
        _initializeTownLabel(_towns[i], townLabel);
        labels.push_back(townLabel);
    }

    if (labels.size() == 0) {
        return;
    }

    _initializeMarkerLabelScores(labels);
}

void gen::MapGenerator::_initializeAreaLabels(std::vector<Label> &labels) {
    _fontData.setFontFace(_areaLabelFontFace, _areaLabelFontSize);
    for (unsigned int i = 0; i < _cities.size(); i++) {
        Label areaLabel;
        _initializeAreaLabel(_cities[i], areaLabel);
        labels.push_back(areaLabel);
    }

    if (labels.size() == 0) {
        return;
    }

    _initializeAreaLabelScores(labels);
}

void gen::MapGenerator::_initializeCityLabel(City &city, Label &label) {
    label.text = city.cityName;
    label.fontface = _fontData.getFontFace();
    label.fontsize = _fontData.getFontSize();
    label.position = city.position;

    double mapheight = _extents.maxy - _extents.miny;
    double radius = (_cityMarkerRadius / (double)_imgheight) * mapheight;
    label.candidates = _getMarkerLabelCandidates(label, radius);
}

void gen::MapGenerator::_initializeTownLabel(Town &town, Label &label) {
    label.text = town.townName;
    label.fontface = _fontData.getFontFace();
    label.fontsize = _fontData.getFontSize();
    label.position = town.position;

    double mapheight = _extents.maxy - _extents.miny;
    double radius = (_townMarkerRadius / (double)_imgheight) * mapheight;
    label.candidates = _getMarkerLabelCandidates(label, radius);
}

void gen::MapGenerator::_initializeAreaLabel(City &city, Label &label) {
    label.text = city.territoryName;
    label.fontface = _fontData.getFontFace();
    label.fontsize = _fontData.getFontSize();
    label.position = city.position;
    label.candidates = _getAreaLabelCandidates(label, city);
}

std::vector<gen::MapGenerator::LabelCandidate> 
gen::MapGenerator::_getMarkerLabelCandidates(Label label, double markerRadius) {
    std::vector<LabelOffset> offsets = _getLabelOffsets(label, markerRadius);
    std::vector<LabelCandidate> candidates;
    for (unsigned int i = 0; i < offsets.size(); i++) {
        LabelCandidate c;
        c.text = label.text;
        c.fontface = label.fontface;
        c.fontsize = label.fontsize;
        c.position = dcel::Point(label.position.x + offsets[i].offset.x, 
                                 label.position.y + offsets[i].offset.y);
        c.extents = _getTextExtents(c.text, c.position);
        c.charextents = _getCharacterExtents(c.text, c.position);
        c.orientationScore = offsets[i].score;
        candidates.push_back(c);
    }

    return candidates;
}

std::vector<gen::MapGenerator::LabelCandidate> 
gen::MapGenerator::_getAreaLabelCandidates(Label label, City &city) {
    std::vector<LabelCandidate> candidates;

    std::vector<dcel::Point> samples;
    _getAreaLabelSamples(city, samples);

    dcel::Point p(0.0, 0.0);
    Extents2d extents = _getTextExtents(label.text, p);
    std::vector<Extents2d> charextents = _getCharacterExtents(label.text, p);

    int cityid = -1;
    for (unsigned int i = 0; i < _cities.size(); i++) {
        if (_cities[i].faceid == city.faceid) {
            cityid = i;
            break;
        }
    }

    dcel::Point center(0.5*(extents.minx + extents.maxx), 
                       0.5*(extents.miny + extents.maxy));
    for (unsigned int i = 0; i < samples.size(); i++) {
        p = samples[i];
        double tx = p.x - center.x;
        double ty = p.y - center.y;

        LabelCandidate c;
        c.text = label.text;
        c.fontface = label.fontface;
        c.fontsize = label.fontsize;
        c.position = dcel::Point(p.x - center.x, p.y - center.y);
        c.extents = extents;
        c.charextents = charextents;
        c.cityid = cityid;

        c.extents.minx += tx;
        c.extents.miny += ty;
        c.extents.maxx += tx;
        c.extents.maxy += ty;
        for (unsigned int j = 0; j < c.charextents.size(); j++) {
            c.charextents[j].minx += tx;
            c.charextents[j].miny += ty;
            c.charextents[j].maxx += tx;
            c.charextents[j].maxy += ty;
        }

        candidates.push_back(c);
    }

    return candidates;
}

void gen::MapGenerator::_getAreaLabelSamples(City &city, 
                                             std::vector<dcel::Point> &samples) {
    int cityid = _territoryData[city.faceid];
    std::vector<int> territoryFaces;
    std::vector<int> territoryCounts(_cities.size(), 0);
    for (unsigned int i = 0; i < _territoryData.size(); i++) {
        if (_territoryData[i] == cityid) {
            territoryFaces.push_back(i);
        }
        if (_territoryData[i] != -1) {
            territoryCounts[_territoryData[i]]++;
        }
    }
    _shuffleVector(territoryFaces);

    int maxCount = -1;
    for (unsigned int i = 0; i < territoryCounts.size(); i++) {
        maxCount = (int)fmax(territoryCounts[i], maxCount);
    }

    int numFaces = territoryFaces.size();
    int numSamples = (int)(((double)numFaces / (double)maxCount)*_numAreaLabelSamples);
    numSamples = (int)fmin(numSamples, territoryFaces.size());
    for (int i = 0; i < numSamples; i++) {
        samples.push_back(_computeFacePosition(territoryFaces[i]));
    }
}

void gen::MapGenerator::_shuffleVector(std::vector<int> &vector) {
    int temp;
    for (int i = vector.size() - 2; i >= 0; i--) {
        int j = (rand() % (int)(i - 0 + 1));
        temp = vector[i];
        vector[i] = vector[j];
        vector[j] = temp;
    }
}

dcel::Point gen::MapGenerator::_getPixelCoordinates(dcel::Point &p) {
    double nx = (p.x - _extents.minx) / (_extents.maxx - _extents.minx);
    double ny = (p.y - _extents.miny) / (_extents.maxy - _extents.miny);
    return dcel::Point((double)_imgwidth * nx, 
                       (double)_imgheight * (1.0 - ny));
}

dcel::Point gen::MapGenerator::_getMapCoordinates(dcel::Point &p) {
    double nx = p.x / (double)_imgwidth;
    double ny = 1.0 - (p.y / (double)_imgheight);
    return dcel::Point(_extents.minx + nx * (_extents.maxx - _extents.minx),
                       _extents.miny + ny * (_extents.maxy - _extents.miny));
}

Extents2d gen::MapGenerator::_getTextExtents(std::string text, dcel::Point pos) {
    TextExtents extents = _fontData.getTextExtents(text); 
    dcel::Point px = _getPixelCoordinates(pos);

    dcel::Point minp(px.x + extents.offx, px.y + extents.offy);
    dcel::Point maxp(minp.x + extents.width, minp.y + extents.height);
    minp = _getMapCoordinates(minp);
    maxp = _getMapCoordinates(maxp);

    return Extents2d(minp.x , maxp.y, maxp.x, minp.y);
}

std::vector<Extents2d> gen::MapGenerator::_getCharacterExtents(std::string text, 
                                                               dcel::Point pos) {
    std::vector<TextExtents> extents = _fontData.getCharacterExtents(text);
    dcel::Point px = _getPixelCoordinates(pos);

    std::vector<Extents2d> charextents;
    charextents.reserve(extents.size());
    for (unsigned int i = 0; i < extents.size(); i++) {
        dcel::Point minp(px.x + extents[i].offx, px.y + extents[i].offy);
        dcel::Point maxp(minp.x + extents[i].width, minp.y + extents[i].height);
        minp = _getMapCoordinates(minp);
        maxp = _getMapCoordinates(maxp);

        charextents.push_back(Extents2d(minp.x , maxp.y, maxp.x, minp.y));
    } 

    return charextents;
}

jsoncons::json gen::MapGenerator::_getLabelJSON(LabelCandidate &label) {
    dcel::Point npos = _normalizeMapCoordinate(label.position);
    dcel::Point nmin = _normalizeMapCoordinate(label.extents.minx, 
                                               label.extents.miny);
    dcel::Point nmax = _normalizeMapCoordinate(label.extents.maxx, 
                                               label.extents.maxy);

    jsoncons::json json;
    json["text"] = label.text;
    json["fontface"] = label.fontface;
    json["fontsize"] = label.fontsize;
    json["position"] = std::vector<double>({npos.x, npos.y});
    json["extents"] = std::vector<double>({nmin.x, nmin.y, nmax.x, nmax.y});

    std::vector<double> charextents;
    for (unsigned int i = 0; i < label.charextents.size(); i++) {
        Extents2d extents = label.charextents[i];
        nmin = _normalizeMapCoordinate(extents.minx, extents.miny);
        nmax = _normalizeMapCoordinate(extents.maxx, extents.maxy);
        charextents.push_back(nmin.x);
        charextents.push_back(nmin.y);
        charextents.push_back(nmax.x);
        charextents.push_back(nmax.y);
    }
    json["charextents"] = charextents;
    json["score"] = label.baseScore;

    return json;
}

dcel::Point gen::MapGenerator::_normalizeMapCoordinate(double x, double y) {
    return dcel::Point((x - _extents.minx) / (_extents.maxx - _extents.minx),
                       (y - _extents.miny) / (_extents.maxy - _extents.miny));
}

dcel::Point gen::MapGenerator::_normalizeMapCoordinate(dcel::Point &p) {
    return _normalizeMapCoordinate(p.x, p.y);
}

std::vector<gen::MapGenerator::LabelOffset> 
gen::MapGenerator::_getLabelOffsets(Label label, double markerRadius) {
    Extents2d extents = _getTextExtents(label.text, label.position);
    std::vector<Extents2d> charextents = _getCharacterExtents(label.text,
                                                              label.position);

    double r = markerRadius;
    double textwidth = extents.maxx - extents.minx;
    double textheight = extents.maxy - extents.miny;
    double textheightstart = charextents[0].maxy - charextents[0].miny;
    int endidx = charextents.size() - 1;
    double textheightend = charextents[endidx].maxy - charextents[endidx].miny;
    double starty = charextents[0].miny - extents.miny;
    double endy = charextents[endidx].miny - extents.miny;

    std::vector<dcel::Point> offsets({
        // Labels to the right of marker
        dcel::Point(1.0 * r, -starty + 1.2 * r),
        dcel::Point(1.2 * r, -starty + 0.9 * r),
        dcel::Point(1.4 * r, -starty + 0.0 * r),
        dcel::Point(1.4 * r, -starty + 0.5 * r - 0.5 * textheightstart),
        dcel::Point(1.4 * r, -starty - 0.5 * r - 0.5 * textheightstart),
        dcel::Point(1.4 * r, -starty + 0.0 * r - textheightstart),
        dcel::Point(1.0 * r, -starty - 1.0 * r - textheightstart),

        // Labels to the left of marker
        dcel::Point(-1.2 * r - textwidth, -endy + 1.0 * r),
        dcel::Point(-1.3 * r - textwidth, -endy + 0.5 * r),
        dcel::Point(-1.4 * r - textwidth, -endy + 0.0 * r),
        dcel::Point(-1.4 * r - textwidth, -endy + 0.5 * r - 0.5 * textheightend),
        dcel::Point(-1.3 * r - textwidth, -endy - 0.5 * r - 0.5 * textheightend),
        dcel::Point(-1.3 * r - textwidth, -endy + 0.0 * r - textheightend),

        // Labels above/below marker
        dcel::Point(-(1.0 / 3.0) * textwidth, 1.4 * r),
        dcel::Point(-(1.0 / 3.0) * textwidth, -1.4 * r - textheight),
        dcel::Point(-0.5 * textwidth, 1.4 * r),
        dcel::Point(-0.5 * textwidth, -1.4 * r - textheight),
        dcel::Point(-(2.0 / 3.0) * textwidth, -1.4 * r - textheight),
        dcel::Point(-(2.0 / 3.0) * textwidth, -1.4 * r - textheight)
    });

    std::vector<double> scores({0.41, 0.33, 0.00, 0.04, 0.30, 0.12, 0.59, 0.63, 
                                0.44, 0.07, 0.10, 0.02, 0.37, 0.70, 0.74, 0.67, 
                                0.89, 0.74, 1.0});

    std::vector<LabelOffset> labelOffsets;
    for (unsigned int i = 0; i < offsets.size(); i++) {
        labelOffsets.push_back(LabelOffset(offsets[i], scores[i]));
    }

    return labelOffsets;
}

void gen::MapGenerator::_initializeMarkerLabelScores(std::vector<Label> &labels) {
    _initializeLabelEdgeScores(labels);
    _initializeLabelMarkerScores(labels);
    _initializeLabelContourScores(labels);
    _initializeLabelRiverScores(labels);
    _initializeLabelBorderScores(labels);
    _initializeLabelBaseScores(labels);
}

void gen::MapGenerator::_initializeAreaLabelScores(std::vector<Label> &labels) {
    _initializeAreaLabelOrientationScores(labels);
    _initializeLabelEdgeScores(labels);
    _initializeAreaLabelMarkerScores(labels);
    _initializeLabelContourScores(labels);
    _initializeLabelRiverScores(labels);
    _initializeLabelBorderScores(labels);
    _initializeLabelBaseScores(labels);

    for (unsigned int j = 0; j < labels.size(); j++) {
        std::sort(labels[j].candidates.begin(),
                  labels[j].candidates.end(), _sortAreaLabelsByScore);

        std::vector<LabelCandidate> candidates;
        int n = (int)fmin(labels[j].candidates.size(), _numAreaLabelCandidates);
        for (int i = 0; i < n; i++) {
            candidates.push_back(labels[j].candidates[i]);
        }
        labels[j].candidates = candidates;
    }
}

bool gen::MapGenerator::_sortAreaLabelsByScore(LabelCandidate label1, 
                                               LabelCandidate label2) { 
    return label1.baseScore < label2.baseScore;
}

void gen::MapGenerator::_initializeLabelEdgeScores(std::vector<Label> &labels) {
    Extents2d extents;
    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            extents = labels[j].candidates[i].extents;
            labels[j].candidates[i].edgeScore = _getEdgeScore(extents);
        }
    }
}

double gen::MapGenerator::_getEdgeScore(Extents2d extents) {
    dcel::Point minp(extents.minx, extents.miny);
    if (!_extents.containsPoint(minp)) {
        return _edgeScorePenalty;
    }

    dcel::Point maxp(extents.maxx, extents.maxy);
    if (!_extents.containsPoint(maxp)) {
        return _edgeScorePenalty;
    }

    return 0.0;
}

void gen::MapGenerator::_initializeLabelMarkerScores(std::vector<Label> &labels) {
    Extents2d extents;
    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            extents = labels[j].candidates[i].extents;
            labels[j].candidates[i].markerScore = _computeLabelMarkerScore(extents);
        }
    }
}

double gen::MapGenerator::_computeLabelMarkerScore(Extents2d extents) {
    int count = 0;
    double mapheight = _extents.maxy - _extents.miny;
    double r = (_cityMarkerRadius / (double)_imgheight) * mapheight;
    r *= _labelMarkerRadiusFactor;
    for (unsigned int i = 0; i < _cities.size(); i++) {
        dcel::Point p = _cities[i].position;
        Extents2d markerExtents(p.x - r, p.y - r, p.x + r, p.y + r);
        if (_isExtentsOverlapping(extents, markerExtents)) {
            count++;
        }
    }

    r = (_townMarkerRadius / (double)_imgheight) * mapheight;
    r *= _labelMarkerRadiusFactor;
    for (unsigned int i = 0; i < _towns.size(); i++) {
        dcel::Point p = _towns[i].position;
        Extents2d markerExtents(p.x - r, p.y - r, p.x + r, p.y + r);
        if (_isExtentsOverlapping(extents, markerExtents)) {
            count++;
        }
    }

    return count * _markerScorePenalty;
}

void gen::MapGenerator::_initializeAreaLabelMarkerScores(std::vector<Label> &labels) {
    Extents2d extents;
    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            extents = labels[j].candidates[i].extents;
            labels[j].candidates[i].markerScore = _computeAreaLabelMarkerScore(extents);
        }
    }
}

double gen::MapGenerator::_computeAreaLabelMarkerScore(Extents2d extents) {
    int count = 0;
    double mapheight = _extents.maxy - _extents.miny;
    double r = (_cityMarkerRadius / (double)_imgheight) * mapheight;
    r *= _areaLabelMarkerRadiusFactor;
    for (unsigned int i = 0; i < _cities.size(); i++) {
        dcel::Point p = _cities[i].position;
        Extents2d markerExtents(p.x - r, p.y - r, p.x + r, p.y + r);
        if (_isExtentsOverlapping(extents, markerExtents)) {
            count++;
        }
    }

    r = (_townMarkerRadius / (double)_imgheight) * mapheight;
    r *= _areaLabelMarkerRadiusFactor;
    for (unsigned int i = 0; i < _towns.size(); i++) {
        dcel::Point p = _towns[i].position;
        Extents2d markerExtents(p.x - r, p.y - r, p.x + r, p.y + r);
        if (_isExtentsOverlapping(extents, markerExtents)) {
            count++;
        }
    }

    return count * _markerScorePenalty;
}

void gen::MapGenerator::_initializeLabelContourScores(std::vector<Label> &labels) {
    std::vector<dcel::Point> contourPoints;
    _getDataPoints(_contourData, contourPoints);

    double dx = _spatialGridResolutionFactor*_resolution;
    SpatialPointGrid pointGrid(contourPoints, dx);
    for (unsigned int i = 0; i < labels.size(); i++) {
        _computeContourScores(labels[i], pointGrid);
    }
}

void gen::MapGenerator::_getDataPoints(std::vector<std::vector<double> > &data,
                                       std::vector<dcel::Point> &points) {
    double width = _extents.maxx - _extents.minx;
    double height = _extents.maxy - _extents.miny;

    double eps = 1e-9;
    for (unsigned int j = 0; j < data.size(); j++) {
        bool isLoop = fabs(data[j][0] - data[j][data[j].size() - 2]) < eps &&
                      fabs(data[j][1] - data[j][data[j].size() - 1]) < eps;

        for (unsigned int i = 0; i < data[j].size(); i += 2) {
            if (i != data[j].size() - 2 || !isLoop) {
                double x = _extents.minx + data[j][i] * width;
                double y = _extents.miny + data[j][i + 1] * height;
                points.push_back(dcel::Point(x, y));
            }
        }
    }
}

void gen::MapGenerator::_computeContourScores(Label &label, 
                                              SpatialPointGrid &grid) {
    int mincount = std::numeric_limits<int>::max();
    int maxcount = 0;
    std::vector<int> counts(label.candidates.size(), 0);
    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        int n = _getLabelPointCount(label.candidates[i], grid);
        counts[i] = n;
        if (n > 0 && n < mincount) { mincount = n; }
        if (n > 0 && n > maxcount) { maxcount = n; }
    }

    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        if (counts[i] == 0) {
            label.candidates[i].contourScore = 0.0;
        } else {
            if (maxcount > mincount) {
                double f = (double)(counts[i]-mincount) / (double)(maxcount-mincount);
                double scorediff = _maxContourScorePenalty - _minContourScorePenalty;
                label.candidates[i].contourScore = _minContourScorePenalty + f * scorediff;
            } else {
                label.candidates[i].contourScore = _maxContourScorePenalty;
            }
        }
    }
}

int gen::MapGenerator::_getLabelPointCount(LabelCandidate &c, 
                                            SpatialPointGrid &grid) {
    int count = 0;
    for (unsigned int i = 0; i < c.charextents.size(); i++) {
        count += grid.getPointCount(c.charextents[i]);
    }
    return count;
}

void gen::MapGenerator::_initializeLabelRiverScores(std::vector<Label> &labels) {
    std::vector<dcel::Point> riverPoints;
    _getDataPoints(_riverData, riverPoints);

    double dx = _spatialGridResolutionFactor*_resolution;
    SpatialPointGrid pointGrid(riverPoints, dx);
    for (unsigned int i = 0; i < labels.size(); i++) {
        _computeRiverScores(labels[i], pointGrid);
    }
}

void gen::MapGenerator::_computeRiverScores(Label &label, 
                                            SpatialPointGrid &grid) {
    int mincount = std::numeric_limits<int>::max();
    int maxcount = 0;
    std::vector<int> counts(label.candidates.size(), 0);
    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        int n = _getLabelPointCount(label.candidates[i], grid);
        counts[i] = n;
        if (n > 0 && n < mincount) { mincount = n; }
        if (n > 0 && n > maxcount) { maxcount = n; }
    }

    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        if (counts[i] == 0) {
            label.candidates[i].riverScore = 0.0;
        } else {
            if (maxcount > mincount) {
                double f = (double)(counts[i]-mincount) / (double)(maxcount-mincount);
                double scorediff = _maxRiverScorePenalty - _minRiverScorePenalty;
                label.candidates[i].riverScore = _minRiverScorePenalty + f * scorediff;
            } else {
                label.candidates[i].riverScore = _maxRiverScorePenalty;
            }
        }
    }
}

void gen::MapGenerator::_initializeLabelBorderScores(std::vector<Label> &labels) {
    std::vector<dcel::Point> borderPoints;
    _getDataPoints(_borderData, borderPoints);

    double dx = _spatialGridResolutionFactor*_resolution;
    SpatialPointGrid pointGrid(borderPoints, dx);
    for (unsigned int i = 0; i < labels.size(); i++) {
        _computeBorderScores(labels[i], pointGrid);
    }
}

void gen::MapGenerator::_computeBorderScores(Label &label, 
                                            SpatialPointGrid &grid) {
    int mincount = std::numeric_limits<int>::max();
    int maxcount = 0;
    std::vector<int> counts(label.candidates.size(), 0);
    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        int n = _getLabelPointCount(label.candidates[i], grid);
        counts[i] = n;
        if (n > 0 && n < mincount) { mincount = n; }
        if (n > 0 && n > maxcount) { maxcount = n; }
    }

    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        if (counts[i] == 0) {
            label.candidates[i].borderScore = 0.0;
        } else {
            if (maxcount > mincount) {
                double f = (double)(counts[i]-mincount) / (double)(maxcount-mincount);
                double scorediff = _maxBorderScorePenalty - _minBorderScorePenalty;
                label.candidates[i].borderScore = _minBorderScorePenalty + f * scorediff;
            } else {
                label.candidates[i].borderScore = _maxBorderScorePenalty;
            }
        }
    }
}

void gen::MapGenerator::_initializeAreaLabelOrientationScores(std::vector<Label> &labels) {
    for (unsigned int i = 0; i < labels.size(); i++) {
        _initializeAreaLabelOrientationScore(labels[i]);
    }
}

void gen::MapGenerator::_initializeAreaLabelOrientationScore(Label &label) {
    std::vector<dcel::Point> facePositions = _computeFacePositions();
    std::vector<bool> isFaceInMap(_voronoi.faces.size(), false);
    for (unsigned int i = 0; i < _voronoi.faces.size(); i++) {
        isFaceInMap[i] = _extents.containsPoint(facePositions[i]);
    }

    dcel::Point centerOfMass(0.0, 0.0);
    std::vector<dcel::Point> territoryPoints;
    std::vector<dcel::Point> waterPoints;
    std::vector<dcel::Point> enemyPoints;
    int territoryID = label.candidates[0].cityid;
    for (unsigned int i = 0; i < _territoryData.size(); i++) {
        if (!isFaceInMap[i]) { continue; }

        int id = _territoryData[i];
        if (id == territoryID) {
            territoryPoints.push_back(facePositions[i]);
            centerOfMass.x += facePositions[i].x;
            centerOfMass.y += facePositions[i].y;
        } else if (id == -1) {
            waterPoints.push_back(facePositions[i]);
        } else {
            enemyPoints.push_back(facePositions[i]);
        }
    }

    centerOfMass.x /= (double)territoryPoints.size();
    centerOfMass.y /= (double)territoryPoints.size();
    double maxdistsq = 0.0;
    for (unsigned int i = 0; i < territoryPoints.size(); i++) {
        double dx = centerOfMass.x - territoryPoints[i].x;
        double dy = centerOfMass.y - territoryPoints[i].y;
        maxdistsq = fmax(dx*dx + dy*dy, maxdistsq); 
    }
    double territoryRadius = sqrt(maxdistsq);

    double dx = _spatialGridResolutionFactor*_resolution;
    SpatialPointGrid territoryGrid(territoryPoints, dx);
    SpatialPointGrid enemyGrid(enemyPoints, dx);
    SpatialPointGrid waterGrid(waterPoints, dx);

    for (unsigned int i = 0; i < label.candidates.size(); i++) {
        double score = _calculationAreaLabelOrientationScore(
                    label.candidates[i], territoryGrid, enemyGrid, waterGrid);

        Extents2d e = label.candidates[i].extents;
        dcel::Point c(0.5*(e.maxx + e.minx), 0.5*(e.maxy + e.miny));
        double dx = c.x - centerOfMass.x;
        double dy = c.y - centerOfMass.y;
        double dist = sqrt(dx*dx + dy*dy);
        score += dist / territoryRadius;

        label.candidates[i].orientationScore = score;
    }
}

double gen::MapGenerator::_calculationAreaLabelOrientationScore(
                                LabelCandidate &label,
                                SpatialPointGrid &territoryGrid,
                                SpatialPointGrid &enemyGrid,
                                SpatialPointGrid &waterGrid) {

    int territoryCount = _getLabelPointCount(label, territoryGrid);
    int enemyCount = _getLabelPointCount(label, enemyGrid);
    int waterCount = _getLabelPointCount(label, waterGrid);
    int total = territoryCount + enemyCount + waterCount;
    double territoryPCT = (double)territoryCount / (double)total;
    double enemyPCT = (double)enemyCount / (double)total;
    double waterPCT = (double)waterCount / (double)total;

    double score = territoryPCT*_territoryScore +
                   enemyPCT*_enemyScore +
                   waterPCT*_waterScore;

    return score;
}

void gen::MapGenerator::_initializeLabelBaseScores(std::vector<Label> &labels) {
    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            double score = _computeLabelBaseScore(labels[j].candidates[i]);
            labels[j].candidates[i].baseScore = score;
        }
    }
}

double gen::MapGenerator::_computeLabelBaseScore(LabelCandidate &label) {
    double sum = label.orientationScore + label.edgeScore + label.markerScore +
                 label.contourScore + label.riverScore + label.borderScore;
    double avg = (1.0 / 6.0) * sum;
    return avg;
}

void gen::MapGenerator::_generateLabelPlacements(std::vector<Label> &labels) {
    _randomizeLabelPlacements(labels);
    _initializeLabelCollisionData(labels);
    double score = _calculateLabelPlacementScore(labels);

    int numLabels = labels.size();
    double temperature = _initialTemperature;
    int numTemperatureChanges = 0;
    int numRepositionings = 0;
    int numSuccessfulRepositionings = 0;
    int maxSuccessfulRepositionings = _successfulRepositioningFactor * numLabels;
    int maxTotalRepositionings = _totalRepositioningFactor * numLabels;
    while (numTemperatureChanges < _maxTemperatureChanges) {
        int randlidx = _randomRangeInt(0, labels.size());
        int randcidx = _randomRangeInt(0, labels[randlidx].candidates.size());
        int lastcidx = labels[randlidx].candidateIdx;
        labels[randlidx].candidateIdx = randcidx;

        double newScore = _calculateLabelPlacementScore(labels);
        double diff = newScore - score;
        if (diff < 0 && fabs(diff) > 1e-9) {
            score = newScore;
            numSuccessfulRepositionings++;
        } else {
            double prob = 1.0 - exp(-diff / temperature);
            if (_randomRangeDouble(0, 1) < prob) {
                labels[randlidx].candidateIdx = lastcidx;
            } else {
                score = newScore;
            }
        }

        numRepositionings++;
        if (numSuccessfulRepositionings > maxSuccessfulRepositionings || 
            numRepositionings > maxTotalRepositionings) {
            if (numSuccessfulRepositionings == 0) {
                break;
            }

            temperature *= _annealingFactor;
            numTemperatureChanges++;
            numRepositionings = 0;
            numSuccessfulRepositionings = 0;
        }
    }
}

void gen::MapGenerator::_randomizeLabelPlacements(std::vector<Label> &labels) {
    for (unsigned int i = 0; i < labels.size(); i++) {
        labels[i].candidateIdx = _randomRangeInt(0, labels[i].candidates.size());
    }
}

int gen::MapGenerator::_randomRangeInt(int minval, int maxval) {
    return minval + (rand() % (int)(maxval - minval));
}

double gen::MapGenerator::_randomRangeDouble(double minval, double maxval) {
    return minval + (double)rand() / ((double)RAND_MAX / (maxval - minval));
}

void gen::MapGenerator::_initializeLabelCollisionData(std::vector<Label> &labels) {
    int uid = 0;
    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            labels[j].candidates[i].parentIdx = j;
            labels[j].candidates[i].collisionIdx = uid;
            uid++;
        }
    }

    for (unsigned int j = 0; j < labels.size(); j++) {
        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            _initializeLabelCollisionData(labels, labels[j].candidates[i]);
        }
    }
}

void gen::MapGenerator::_initializeLabelCollisionData(std::vector<Label> &labels,
                                                      LabelCandidate &label) {
    for (unsigned int j = 0; j < labels.size(); j++) {
        if ((int)j == label.parentIdx) {
            continue;
        }

        for (unsigned int i = 0; i < labels[j].candidates.size(); i++) {
            if (_isLabelOverlapping(label, labels[j].candidates[i])) {
                CollisionData cdata;
                cdata.id = labels[j].candidates[i].collisionIdx;
                label.collisionData.push_back(cdata);
            }
        }
    }
}

double gen::MapGenerator::_calculateLabelPlacementScore(std::vector<Label> &labels) {
    int maxid = labels.back().candidates.back().collisionIdx;
    std::vector<bool> isCandidateActive(maxid + 1, false);
    for (unsigned int i = 0; i < labels.size(); i++) {
        int cid = labels[i].candidateIdx;
        int activeIdx = labels[i].candidates[cid].collisionIdx;
        isCandidateActive[activeIdx] = true;
    }

    double sum = 0.0;
    for (unsigned int i = 0; i < labels.size(); i++) {
        labels[i].score = _calculateLabelPlacementScore(labels[i], isCandidateActive);
        sum += labels[i].score;
    }
    double avg = sum / (double)labels.size();

    return avg;
}

double gen::MapGenerator::_calculateLabelPlacementScore(Label &label, 
                                                        std::vector<bool> &isActive) {
    int cid = label.candidateIdx;
    double baseScore = label.candidates[cid].baseScore;
    double overlapScore = 0.0;
    for (unsigned int i = 0; i < label.candidates[cid].collisionData.size(); i++) {
        int collisionIdx = label.candidates[cid].collisionData[i].id;
        if (isActive[collisionIdx]) {
            overlapScore += _overlapScorePenalty;
        }
    }

    return baseScore + overlapScore;
}

bool gen::MapGenerator::_isLabelOverlapping(LabelCandidate &label1, 
                                            LabelCandidate &label2) {
    return _isExtentsOverlapping(label1.extents, label2.extents);
}

bool gen::MapGenerator::_isExtentsOverlapping(Extents2d &e1, Extents2d &e2) {
    return e1.minx < e2.maxx && e1.maxx > e2.minx &&
           e1.miny < e2.maxy && e1.maxy > e2.miny;
}