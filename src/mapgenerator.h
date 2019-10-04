#ifndef MAPGENERATOR_H
#define MAPGENERATOR_H

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <queue>
#include <string>
#include <map>

#include "jsoncons/json.hpp"
#include "extents2d.h"
#include "dcel.h"
#include "poissondiscsampler.h"
#include "delaunay.h"
#include "voronoi.h"
#include "vertexmap.h"
#include "nodemap.h"
#include "fontface.h"
#include "spatialpointgrid.h"
#include "resources.h"
#include "stopwatch.h"
#include "fastnoise.h"
#include "config.h"
#include "cereal/cereal.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "mapinstruction.h"

#if defined(_WIN32)
#undef max
#endif

namespace gen {

	class MapGenerator {

	public:
		MapGenerator();
		MapGenerator(Extents2d extents, double resolution,
			int imgwidth, int imgheight);
		MapGenerator(Extents2d extents, double resolution);

		void initialize();
		void normalize();
		void round();
		void relax();
		void setSeaLevel(double level);
		void setSeaLevelToMedian();
		void addHill(double px, double py, double radius, double height, bool multiply);
		void addCone(double px, double py, double radius, double height, bool multiply);
		void addDepression(double px, double py, double radius, double height, bool multiply);
		void addPit(double px, double py, double radius, double height, bool multiply);
		void addSlope(double px, double py, double dirx, double diry,
			double radius, double height, bool multiply);
		void multiply(double min, double max, double amount);
		void addNoise(double freq, double strength, bool multiply);
		void erode(double amount);
		void erode();
		void makeContinent();
		double randomDouble(double min, double max);

		void generateBiomes();
		
		void performInstructions();
		void addInstruction(gen::MapInstruction instruction);

		void addCity(std::string cityName, std::string territoryName);
		void addTown(std::string townName);

		void readHeightMapFile(std::string filename);
		void readVoronoiFile(std::string filename);
		void readInstructionFile(std::string filename);
		void outputInstructionFile(std::string filename);
		void outputVoronoiDiagram(std::string filename);
		void outputHeightMap(std::string filename);
		std::vector<char> getDrawData();

		Extents2d getExtents();
		void setDrawScale(double scale);

		void setMapGlobalPosition(double scale, double offset);

		void enableSlopes();
		void enableRivers();
		void enableContour();
		void enableBorders();
		void enableCities();
		void enableTowns();
		void enableLabels();
		void enableAreaLabels();

		void disableSlopes();
		void disableRivers();
		void disableContour();
		void disableBorders();
		void disableCities();
		void disableTowns();
		void disableLabels();
		void disableAreaLabels();

	private:
		typedef std::vector<dcel::Vertex> VertexList;

		struct Biome {
			double type;
			std::vector<double> vertices;

		};

		struct Segment {
			dcel::Point p1;
			dcel::Point p2;
		};

		struct CityLocation {
			dcel::Point position;
			int faceid;
		};

		struct City {
			std::string cityName;
			std::string territoryName;
			dcel::Point position;
			int faceid;
			std::vector<double> movementCosts;
		};

		struct Town {
			std::string townName;
			dcel::Point position;
			int faceid;
		};

		struct CollisionData {
			int id;
		};

		struct LabelCandidate {
			std::string text;
			std::string fontface;
			int fontsize;
			dcel::Point position;
			Extents2d extents;
			std::vector<Extents2d> charextents;
			int cityid;

			double orientationScore;
			double edgeScore;
			double markerScore;
			double contourScore;
			double riverScore;
			double borderScore;
			double baseScore;

			int parentIdx;
			int collisionIdx;
			std::vector<CollisionData> collisionData;
		};

		struct Label {
			std::string text;
			std::string fontface;
			int fontsize;
			dcel::Point position;

			std::vector<LabelCandidate> candidates;
			int candidateIdx = -1;
			double score = 0.0;
		};

		struct LabelOffset {
			dcel::Point offset;
			double score = 0.0;

			LabelOffset() {}
			LabelOffset(dcel::Point p, double s) : offset(p), score(s) {}
		};

		void _initializeVoronoiData();
		void _initializeMapData();
		void _initializeNeighbourMap();
		void _initializeFaceNeighbours();
		void _initializeFaceVertices();
		void _initializeFaceEdges();
		jsoncons::json _getExtentsJSON();
		void _outputVertices(std::vector<dcel::Vertex>& verts,
			std::string filename);
		std::vector<double> _computeFaceValues(NodeMap<double>& heightMap);
		std::vector<dcel::Point> _computeFacePositions();
		dcel::Point _computeFacePosition(int fidx);
		bool _isEdgeInMap(dcel::HalfEdge& h);
		bool _isContourEdge(dcel::HalfEdge& h,
			std::vector<double>& faceheights,
			double isolevel);
		void _calculateErosionMap(NodeMap<double>& erosionMap);
		void _fillDepressions();
		void _calculateFlowMap(NodeMap<int>& flowMap);
		void _calculateFluxMap(NodeMap<double>& fluxMap);
		double _calculateFluxCap(NodeMap<double>& fluxMap);
		void _calculateSlopeMap(NodeMap<double>& slopeMap);
		double _calculateSlope(int i);

		void _performInstruction(MapInstruction& mapInstruction);

		void _calculatePrecipitationMap(NodeMap<double>& precipitationMap);
		void _calculateTemperatureMap(NodeMap<double>& temperatureMap);
		void _calculateBiomeMap(NodeMap<double>& biomeMap);
		double _getLifeZone(double temp, double precip);
		void _getBiomeDrawData(std::vector<jsoncons::json>& faceVertices);
		jsoncons::json _getBiomeJSON(double i, std::vector<double>& vertices);
		std::string _toBiomeString(double v);
		double _calculateHeightTemperature(int i, double max);
		void _calculateLatitudeTemperatures(NodeMap<double>& temperatureMap);
		double _calculateLatitudeTemperature(int i);
		double _calculateHeightPrecipitation(int i);
		double _calculateVertexNoise(int i, FastNoise& noiseMap);

		void _getContourDrawData(std::vector<std::vector<double> >& data);
		void _getContourPaths(std::vector<VertexList>& paths);
		bool _isLandFace(int fidx);
		void _getLandFaces(std::vector<bool>& isLandFace);
		void _getFaceHeights(std::vector<double>& faceHeights);
		bool _isLand(double isolevel);
		void _initializeLandFaceTable();
		void _cleanupLandFaces(std::vector<bool>& isLandFace);
		void _getConnectedFaces(int seed, std::vector<bool>& isLandFace,
			std::vector<bool>& isFaceProcessed,
			std::vector<int>& faces);
		bool _isContourEdge(dcel::HalfEdge& h);
		bool _isContourEdge(dcel::Vertex& v1, dcel::Vertex& v2);
		void _getContourPath(int seed, std::vector<bool>& isContourVertex,
			std::vector<bool>& isEndVertex,
			std::vector<bool>& isVertexInContour,
			VertexList& path);

		void _getRiverDrawData(std::vector<std::vector<double> >& data);
		void _getRiverPaths(std::vector<VertexList>& paths);
		void _getRiverVertices(VertexList& vertices);
		bool _isLandVertex(int vidx);
		bool _isCoastVertex(int vidx);
		void _getFixedRiverVertices(VertexList& riverVertices,
			VertexList& fixedVertices);
		VertexList _smoothPath(VertexList& path,
			double factor);

		void _getSlopeDrawData(std::vector<double>& data);
		void _getSlopeSegments(std::vector<Segment>& segments);
		void _calculateHorizontalSlopeMap(NodeMap<double>& slopeMap);
		double _calculateHorizontalSlope(int i);
		void _calculateVerticalSlopeMap(NodeMap<double>& slopeMap);
		double _calculateVerticalSlope(int i);
		void _calculateVertexNormal(int vidx, double* nx, double* ny, double* nz);

		void _getCityDrawData(std::vector<double>& data);
		void _getTownDrawData(std::vector<double>& data);
		CityLocation _getCityLocation();
		void _getCityScores(NodeMap<double>& cityScores);
		double _getPointDistance(dcel::Point& p1, dcel::Point& p2);
		double _pointToEdgeDistance(dcel::Point p);
		void _updateCityMovementCost(City& city);

		void _getTerritoryDrawData(std::vector<std::vector<double> >& data);
		void _getTerritoryBorders(std::vector<VertexList>& borders);
		void _getFaceTerritories(std::vector<int>& faceTerritories);
		void _cleanupFaceTerritories(std::vector<int>& faceTerritories);
		void _smoothTerritoryBoundaries(std::vector<int>& faceTerritories);
		void _getConnectedTerritories(std::vector<int>& faceTerritories,
			std::vector<std::vector<int> >& connected);
		void _getConnectedTerritory(int fidx,
			std::vector<int>& faceTerritories,
			std::vector<bool>& isFaceProcessed,
			std::vector<int>& connectedFaces);
		void _getDisjointTerritories(std::vector<int>& faceTerritories,
			std::vector<std::vector<int> >& connected,
			std::vector<std::vector<int> >& disjoint);
		void _claimDisjointTerritories(std::vector<std::vector<int> >& disjoint,
			std::vector<int>& faceTerritories);
		int _getTerritoryOwner(std::vector<int>& territory,
			std::vector<int>& faceTerritories);
		void _getBorderPaths(std::vector<int>& faceTerritories,
			std::vector<VertexList>& borders);
		void _getBorderEdges(std::vector<int>& faceTerritories,
			std::vector<dcel::HalfEdge>& borderEdges);
		bool _isBorderEdge(dcel::HalfEdge& h, std::vector<int>& faceTerritories);
		bool _isBorderEdge(dcel::Vertex& v1, dcel::Vertex& v2,
			std::vector<int>& faceTerritories);
		void _getBorderPath(int vidx, std::vector<int>& faceTerritories,
			std::vector<bool>& isEndVertex,
			std::vector<bool>& isVertexProcessed,
			VertexList& path);

		void _getLabelDrawData(std::vector<jsoncons::json>& data);
		void _initializeLabels(std::vector<Label>& labels);
		void _initializeMarkerLabels(std::vector<Label>& labels);
		void _initializeAreaLabels(std::vector<Label>& labels);
		void _initializeCityLabel(City& city, Label& label);
		void _initializeTownLabel(Town& town, Label& label);
		void _initializeAreaLabel(City& city, Label& label);
		std::vector<LabelCandidate> _getMarkerLabelCandidates(Label label,
			double markerRadius);
		std::vector<LabelCandidate> _getAreaLabelCandidates(Label label,
			City& city);
		void _getAreaLabelSamples(City& city, std::vector<dcel::Point>& samples);
		void _shuffleVector(std::vector<int>& vector);
		dcel::Point _getPixelCoordinates(dcel::Point& p);
		dcel::Point _getMapCoordinates(dcel::Point& p);
		Extents2d _getTextExtents(std::string text, dcel::Point pos);
		std::vector<Extents2d> _getCharacterExtents(std::string text, dcel::Point pos);
		jsoncons::json _getLabelJSON(LabelCandidate& label);
		dcel::Point _normalizeMapCoordinate(dcel::Point& p);
		dcel::Point _normalizeMapCoordinate(double x, double y);
		std::vector<LabelOffset> _getLabelOffsets(Label label, double radius);
		void _initializeMarkerLabelScores(std::vector<Label>& labels);
		void _initializeAreaLabelScores(std::vector<Label>& labels);
		void _initializeLabelEdgeScores(std::vector<Label>& labels);
		static bool _sortAreaLabelsByScore(LabelCandidate label1, LabelCandidate label2);
		double _getEdgeScore(Extents2d extents);
		void _initializeLabelMarkerScores(std::vector<Label>& labels);
		double _computeLabelMarkerScore(Extents2d extents);
		void _initializeAreaLabelMarkerScores(std::vector<Label>& labels);
		double _computeAreaLabelMarkerScore(Extents2d extents);
		void _initializeLabelContourScores(std::vector<Label>& labels);
		void _getDataPoints(std::vector<std::vector<double> >& data,
			std::vector<dcel::Point>& points);
		void _computeContourScores(Label& label, SpatialPointGrid& grid);
		int _getLabelPointCount(LabelCandidate& c, SpatialPointGrid& grid);
		void _initializeLabelRiverScores(std::vector<Label>& labels);
		void _computeRiverScores(Label& label, SpatialPointGrid& grid);
		void _initializeLabelBorderScores(std::vector<Label>& labels);
		void _computeBorderScores(Label& label, SpatialPointGrid& grid);
		void _initializeAreaLabelOrientationScores(std::vector<Label>& labels);
		void _initializeAreaLabelOrientationScore(Label& label);
		double _calculationAreaLabelOrientationScore(LabelCandidate& label,
			SpatialPointGrid& territoryGrid,
			SpatialPointGrid& enemyGrid,
			SpatialPointGrid& waterGrid);
		void _initializeLabelBaseScores(std::vector<Label>& labels);
		double _computeLabelBaseScore(LabelCandidate& label);
		void _generateLabelPlacements(std::vector<Label>& labels);
		void _randomizeLabelPlacements(std::vector<Label>& labels);
		int _randomRangeInt(int minval, int maxval);
		double _randomRangeDouble(double minval, double maxval);
		void _initializeLabelCollisionData(std::vector<Label>& labels);
		void _initializeLabelCollisionData(std::vector<Label>& labels,
			LabelCandidate& label);
		double _calculateLabelPlacementScore(std::vector<Label>& labels);
		double _calculateLabelPlacementScore(Label& label,
			std::vector<bool>& isCandidateActive);
		bool _isLabelOverlapping(LabelCandidate& label1, LabelCandidate& label2);
		bool _isExtentsOverlapping(Extents2d& e1, Extents2d& e2);

		Extents2d _extents;
		double _resolution;
		int _imgwidth;
		int _imgheight;
		int _defaultImageHeight = 1080;
		double _defaultResolution = 0.1;
		double _defaultExtentsWidth = 20.0 * 1.7777;
		double _defaultExtentsHeight = 20.0;

		dcel::DCEL _voronoi;
		VertexMap _vertexMap;
		NodeMap<std::vector<int> > _neighbourMap;
		std::vector<std::vector<int> > _faceNeighbours;
		std::vector<std::vector<int> > _faceVertices;
		std::vector<std::vector<int> > _faceEdges;
		NodeMap<double> _heightMap;
		NodeMap<double> _fluxMap;
		NodeMap<int> _flowMap;
		bool _isInitialized = false;
		std::vector<MapInstruction> _instructions;

		NodeMap<double> _temperatureMap;
		NodeMap<double> _precipitationMap;
		NodeMap<double> _biomeMap;
		FastNoise _precipitationNoiseMap;
		FastNoise _temperatureNoiseMap;
		double _mapScale = .25;
		double _mapOffset = 0.2;
		double _equatorPosition = 0.5;
		double _coldestZone = 0.05;
		double _colderZone = 0.18;
		double _coldZone = 0.4;
		double _warmZone = 0.6;
		double _warmerZone = 0.8;
		bool _isTemperatureCalculated = false;
		bool _isPrecipitationCalculated = false;

		std::vector<bool> _isLandFaceTable;
		bool _isLandFaceTableInitialized = false;

		double _samplePadFactor = 3.5;
		int _poissonSamplerKValue = 25;
		double _fluxCapPercentile = 0.995;
		double _maxErosionRate = 50.0;
		double _erosionRiverFactor = 500.0;
		double _erosionCreepFactor = 500.0;
		double _defaultErodeAmount = 0.1;
		double _riverFluxThreshold = 0.06;
		double _riverSmoothingFactor = 0.5;
		double _isolevel = 0.0;
		double _minIslandFaceThreshold = 35;
		bool _isHeightMapEroded = false;

		double _minSlopeThreshold = 0.07;
		double _minSlope = 0.0;
		double _maxSlope = 0.7;
		double _minSlopeAngle = 0.2;
		double _maxSlopeAngle = 1.5;
		double _minSlopeLength = 0.75;
		double _maxSlopeLength = 1.3;
		double _minVerticalSlope = -0.25;
		double _maxVerticalSlope = 0.05;

		double _fluxScoreBonus = 2.0;
		double _nearEdgeScorePenalty = 0.5;
		double _nearCityScorePenalty = 2.0;
		double _nearTownScorePenalty = 1.5;
		double _maxPenaltyDistance = 4.0;

		double _landDistanceCost = 0.2;
		double _seaDistanceCost = 0.4;
		double _uphillCost = 0.1;
		double _downhillCost = 1.0;
		double _fluxCost = 0.8;
		double _landTransitionCost = 0.0;

		int _numTerritoryBorderSmoothingInterations = 3;
		double _territoryBorderSmoothingFactor = 0.5;

		std::vector<std::vector<double> > _contourData;
		std::vector<std::vector<double> > _riverData;
		std::vector<std::vector<double> > _borderData;
		std::vector<int> _territoryData;

		std::vector<City> _cities;
		std::vector<Town> _towns;

		FontFace _fontData;
		double _drawScale = 1.0;
		double _cityMarkerRadius = 10.0;    // in pixels
		double _townMarkerRadius = 5.0;
		std::string _cityLabelFontFace;
		std::string _townLabelFontFace;
		std::string _areaLabelFontFace;
		int _cityLabelFontSize = 35;
		int _townLabelFontSize = 25;
		int _areaLabelFontSize = 35;
		int _numAreaLabelSamples = 500;
		int _numAreaLabelCandidates = 120;
		double _spatialGridResolutionFactor = 5.0;
		double _labelMarkerRadiusFactor = 1.0;
		double _areaLabelMarkerRadiusFactor = 7.5;
		double _edgeScorePenalty = 4.0;
		double _markerScorePenalty = 6.0;
		double _minContourScorePenalty = 0.5;
		double _maxContourScorePenalty = 1.5;
		double _minRiverScorePenalty = 0.7;
		double _maxRiverScorePenalty = 2.0;
		double _minBorderScorePenalty = 0.8;
		double _maxBorderScorePenalty = 2.0;
		double _overlapScorePenalty = 4.0;
		double _territoryScore = 0.0;
		double _enemyScore = 6.0;
		double _waterScore = 0.2;


		double _initialTemperature = 0.91023922;     // 1.0 / log(3)
		double _annealingFactor = 0.9;
		double _maxTemperatureChanges = 100;
		int _successfulRepositioningFactor = 5;
		int _totalRepositioningFactor = 20;

		bool _isSlopesEnabled = true;
		bool _isRiversEnabled = true;
		bool _isContourEnabled = true;
		bool _isBordersEnabled = true;
		bool _isCitiesEnabled = true;
		bool _isTownsEnabled = true;
		bool _isLabelsEnabled = true;
		bool _isAreaLabelsEnabled = true;
	};



}

#endif
