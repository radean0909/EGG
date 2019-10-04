#include "argtable3/argtable3.h"

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <random>

#include "render.h"
#include "config.h"
#include "stopwatch.h"
#include "mapgenerator.h"

double randomDouble(double min, double max) {
    return min + (double)rand() / ((double)RAND_MAX / (max - min));
}

dcel::Point randomPoint(Extents2d &extents) {
    double px = randomDouble(extents.minx, extents.maxx);
    px = randomDouble(extents.minx, extents.maxx);
    double py = randomDouble(extents.miny, extents.maxy);

    return dcel::Point(px, py);
}

dcel::Point randomDirection() {
    double angle = randomDouble(0, 2*3.14159);
    return dcel::Point(sin(angle), cos(angle));
}

void outputMap(gen::MapGenerator &map) {
    gen::config::print("Generating map draw data...");
    StopWatch timer;
    timer.start();
    std::vector<char> drawdata = map.getDrawData();
    timer.stop();
    gen::config::print("Finished Generating map draw data in " +
                       gen::config::toString(timer.getTime()) + " seconds.\n");

    std::string outfile = gen::config::outfile;
    std::string outfileExt = gen::config::outfileExt;
    #ifdef PYTHON_RENDERING_SUPPORTED
        if (outfileExt != std::string(".png")) {
            outfile += ".png";
        }

        timer.reset();
        timer.start();
        gen::config::print("Drawing map...");
        gen::render::drawMap(drawdata, outfile);
        timer.stop();
        gen::config::print("Finished drawing map in " +
                           gen::config::toString(timer.getTime()) + " seconds.\n");

        gen::config::print("Wrote map to image: " + outfile);
    #else
        if (outfileExt != std::string(".json")) {
            outfile += ".json";
        }

        std::ofstream file(outfile);
        file << std::string(drawdata.data());
        file.close();

        std::string msg("Project build without drawing support. Install Python "
                        "and the Pycairo graphics library "
                        "(http://cairographics.org/pycairo/) to enable drawing "
                        "support.\nWrote map draw data to file: " + outfile);
        gen::config::print(msg);

    #endif
}

std::vector<std::string> getLabelNames(int num) {
    std::ifstream file(gen::resources::getCityDataResource());
    std::string jsonstr((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    jsoncons::json json = jsoncons::json::parse(jsonstr);

    std::vector<std::string> countries;
    for (const auto& member : json.members()) {
        countries.push_back(member.name());
    }

    std::vector<std::string> cities;
    while ((int)cities.size() < num) {
        int randidx = rand() % (int)countries.size();
        std::string country = countries[randidx];
        for (const auto& member : json[country].elements()) {
            cities.push_back(member.as<std::string>());
        }
    }

    std::string temp;
    for (int i = cities.size() - 2; i >= 0; i--) {
        int j = (rand() % (int)(i - 0 + 1));
        temp = cities[i];
        cities[i] = cities[j];
        cities[j] = temp;
    }

    std::vector<std::string> labelnames;
    labelnames.insert(labelnames.end(), cities.begin(), cities.begin() + num);

    return labelnames;
}

dcel::Point getEndFromVectors(dcel::Point start, dcel::Point dir, double dist) {
    dcel::Point end;
    end.x = dir.x / sqrt(dir.x*dir.x + dir.y*dir.y) * dist + start.x;
    end.y = dir.y / sqrt(dir.x*dir.x + dir.y*dir.y) * dist + start.y;

    return end;
}


void initializeHeightmap(gen::MapGenerator &map) {
    // Region Map
    double pad = 5.0;
    Extents2d extents = map.getExtents();
    double ratio = (extents.maxx - extents.minx) / (extents.maxy - extents.miny);
    Extents2d expandedExtents(extents.minx - pad, extents.miny - pad/ratio,
                              extents.maxx + pad, extents.maxy + pad/ratio);
    
    int n = (int)randomDouble(100, 250);
    double minr = 1.0;
    double maxr = 4.0;
    for (int i = 0; i < n; i++) {
        dcel::Point p;
        do {
            p = randomPoint(expandedExtents);
        } while (p.x < extents.minx + pad/2. || p.x > extents.maxx - pad/2. || p.y < extents.miny + (pad/ratio) / 2. || p.y > extents.maxy - (pad/ratio)/2.);
        double r = randomDouble(minr, maxr);
        double strength = randomDouble(0.5, 1.5);
        if (randomDouble(0, 1) > 0.5) {
            map.addHill(p.x, p.y, r, strength, false);
            std::vector<double> params = {p.x, p.y, r, strength, 0.};
            gen::MapInstruction instruction("addHill", params);
            map.addInstruction(instruction);
        } else {
            map.addCone(p.x, p.y, r, strength, false);
            std::vector<double> params = {p.x, p.y, r, strength, 0.};
            gen::MapInstruction instruction("addCone", params);
            map.addInstruction(instruction);
        }
    }

    std::vector<dcel::Point> continents;
    bool tooClose = false;
    double continentSpacing = randomDouble(3., 5.);
    n = (int)randomDouble(0,2);
    for (int i = 0; i < n; i++) {
        dcel::Point p;
        do {
            p = randomPoint(expandedExtents);

            for ( unsigned int j = 0; j < continents.size(); j++) {
                dcel::Point c = continents[j];
                double dist = sqrt(pow(c.x-p.x, 2) + pow(c.y-p.y, 2));
                if (dist < continentSpacing) {
                    tooClose = true;
                }
            }
        } while ((p.x < extents.minx + pad/2. ||
            p.x > extents.maxx - pad/2. || 
            p.y < extents.miny + pad / 2. || 
            p.y > extents.maxy - pad/2.) || tooClose);
        continents.push_back(p);
        double r = randomDouble(4.0, 8.0);
        double strength = randomDouble(1.0, 3.0);
        map.addCone(p.x, p.y, r, strength, false);
        std::vector<double> params = {p.x, p.y, r, strength, 0.};
        gen::MapInstruction instruction("addCone", params);
        map.addInstruction(instruction);
    }

    if (randomDouble(0, 1) > 0.1) {
        dcel::Point dir = randomDirection();
        dcel::Point lp = randomPoint(extents);
        double slopewidth = randomDouble(0.5, 5.0);
        double strength = randomDouble(1.0, 3.0);
        map.addSlope(lp.x, lp.y, dir.x, dir.y, slopewidth, strength, false);
        std::vector<double> params = {lp.x, lp.y, dir.x, dir.y, slopewidth, strength,  0.};
        gen::MapInstruction instruction("addSlope", params);
        map.addInstruction(instruction);
    }
    double noise = randomDouble(0.5, 1.5);
    double frequency = randomDouble(0.01, 0.1);
    bool multiple = randomDouble(0,1) > .5;

    map.addNoise(frequency, noise, false);
    std::vector<double> params = {frequency, noise,  0.};
    gen::MapInstruction instruction("addNoise", params);
    map.addInstruction(instruction);
    
    if (randomDouble(0, 1) > 0.5) {
        map.normalize();
    } else {
        map.round();
    }
    
    if (randomDouble(0, 1) > 0.5) {
        map.relax();
    }
    
}

void erode(gen::MapGenerator &map, double amount, int iterations) {
    StopWatch timer;
    std::string msg;
    for (int i = 0; i < iterations; i++) {
        timer.reset();
        timer.start();
        map.erode(amount / (double)iterations);
        timer.stop();

        msg = "\tCompleted erosion step " + gen::config::toString(i + 1) + "/" +
              gen::config::toString(iterations) + " in " +
              gen::config::toString(timer.getTime()) + " seconds.";
        gen::config::print(msg);
    }
}

void createBiomes(gen::MapGenerator &map) {
    StopWatch timer;
    std::string msg;
   
    timer.reset();
    timer.start();
    map.generateBiomes();
    timer.stop();

    msg = "\tCompleted biome generation in " +
            gen::config::toString(timer.getTime()) + " seconds.";
    gen::config::print(msg);
}

void addCities(gen::MapGenerator &map, int numCities, 
               std::vector<std::string> &labels) {
    StopWatch timer;
    std::string msg;
    for (int i = 0; i < numCities; i++) {
        timer.reset();
        timer.start();
        std::string cityName = labels.back();
        labels.pop_back();
        std::string territoryNameLowerCase = labels.back();
        std::string territoryName = territoryNameLowerCase;
        labels.pop_back();
        std::transform(territoryName.begin(), territoryName.end(), 
                       territoryName.begin(), ::toupper);
        map.addCity(cityName, territoryName);
        timer.stop();

        msg = "\tAdded city " + gen::config::toString(i+1) + "/" + 
              gen::config::toString(numCities) + "' to map in " + 
              gen::config::toString(timer.getTime()) + " seconds" + 
              " (" + cityName + ", " + territoryNameLowerCase + ").";
        gen::config::print(msg);
    }
}

void addTowns(gen::MapGenerator &map, int numTowns, 
              std::vector<std::string> &labels) {
    StopWatch timer;
    std::string msg;
    for (int i = 0; i < numTowns; i++) {
        timer.reset();
        timer.start();
        std::string townName = labels.back();
        labels.pop_back();
        map.addTown(townName);
        timer.stop();

        msg = "\tAdded town " + gen::config::toString(i+1) + "/" + 
              gen::config::toString(numTowns) + "' to map in " +
              gen::config::toString(timer.getTime()) + " seconds" + " (" + 
              townName + ").";
        gen::config::print(msg);
    }
}

int main(int argc, char **argv) {

    if (!gen::config::parseOptions(argc, argv)) {
        return 0;
    }

    StopWatch totalTimer;
    totalTimer.start();

    srand(gen::config::seed);
    for (int i = 0; i < 1000; i++) { rand(); }
    gen::config::print("Generating map with seed value: " + 
                       gen::config::toString(gen::config::seed));

    int imgWidth = gen::config::imageWidth;
    int imgHeight = gen::config::imageHeight;
    double aspectRatio = (double)imgWidth / (double)imgHeight;
    double extentsHeight = gen::config::defaultExtentsHeight;
    double extentsWidth = aspectRatio * extentsHeight;
    Extents2d extents(0, 0, extentsWidth, extentsHeight);
    gen::MapGenerator map(extents, gen::config::resolution, imgWidth, imgHeight);
    map.setDrawScale(gen::config::drawScale);

    if (!gen::config::enableSlopes) { map.disableSlopes(); }
    if (!gen::config::enableRivers) { map.disableRivers(); }
    if (!gen::config::enableContour) { map.disableContour(); }
    if (!gen::config::enableBorders) { map.disableBorders(); }
    if (!gen::config::enableCities) { map.disableCities(); }
    if (!gen::config::enableTowns) { map.disableTowns(); }
    if (!gen::config::enableLabels) { map.disableLabels(); }
    if (!gen::config::enableAreaLabels) { map.disableAreaLabels(); }
    map.setMapGlobalPosition(gen::config::mapScale, gen::config::mapOffset);

    gen::config::print("\nInitializing map generator...");
    StopWatch timer;
    timer.start();
    if (gen::config::voronoiFile != "" ) {
        map.readVoronoiFile(gen::config::voronoiFile);
    } 

    map.initialize();
        
    timer.stop();
    gen::config::print("Finished initializing map generator in " +
                    gen::config::toString(timer.getTime()) + " seconds.\n");

    if (gen::config::voronoiCreation) {
        std::string outfile = gen::config::outfile;
        std::string voronoifile = "voronoi." + outfile + ".bin";
        map.outputVoronoiDiagram(voronoifile);
    }

    gen::config::print("Initializing height map...");
    timer.reset();
    timer.start();
    if (gen::config::heightMapFile != "" && gen::config::voronoiFile != "") {
        map.readHeightMapFile(gen::config::heightMapFile);
    } 

    if (gen::config::randomGeneration) {
        initializeHeightmap(map);
    }

    timer.stop();
    gen::config::print("Finished initializing height map in " +
                       gen::config::toString(timer.getTime()) + " seconds.\n");


    if (gen::config::instructionFile != "" && gen::config::voronoiFile != "") {
        map.readInstructionFile(gen::config::instructionFile);
        map.performInstructions();
    } 

    if (gen::config::randomGeneration && gen::config::instructionFile == "") {
        int erosionSteps = gen::config::erosionIterations;
        double erosionAmount = randomDouble(0.2, 0.35);
        if (gen::config::erosionAmount >= 0.0) {
            erosionAmount = gen::config::erosionAmount;
        }
        gen::config::print("Eroding height map by " +
                        gen::config::toString(erosionAmount) + " over " + 
                        gen::config::toString(erosionSteps) + " iterations...");
        timer.reset();
        timer.start();
        erode(map, erosionAmount, erosionSteps);
        timer.stop();
        gen::config::print("Finished eroding height map in " +
                        gen::config::toString(timer.getTime()) + " seconds.\n");
        
        map.setSeaLevelToMedian();

        map.makeContinent();
        std::vector<double> params;;
        gen::MapInstruction instruction("makeContinent", params);
        map.addInstruction(instruction);
    }

    if (gen::config::heightmapCreation) {
        std::string outfile = gen::config::outfile;
        std::string heightmapfile = "heightmap." + outfile + ".bin";
        map.outputHeightMap(heightmapfile);
    }

    if (gen::config::instructionCreation) {
        std::string outfile = gen::config::outfile;
        std::string instructionfile = "instructions." + outfile + ".json";
        map.outputInstructionFile(instructionfile);
    }

    createBiomes(map);

    int numCities = (int)randomDouble(3, 7);
    int numTowns = (int)randomDouble(8, 25);
    if (gen::config::numCities >= 0) { numCities = gen::config::numCities; }
    if (gen::config::numTowns >= 0) { numTowns = gen::config::numTowns; }
    if (!gen::config::enableCities) { numCities = 0; }
    if (!gen::config::enableTowns) { numTowns = 0; }

    int numLabels = 2*numCities + numTowns;
    std::vector<std::string> labelNames = getLabelNames(numLabels);

    gen::config::print("Generating " + gen::config::toString(numCities) +
                       " cities...");
    timer.reset();
    timer.start();
    addCities(map, numCities, labelNames);
    timer.stop();
    gen::config::print("Finished generating cities in " +
                       gen::config::toString(timer.getTime()) + " seconds.\n");

    gen::config::print("Generating " + gen::config::toString(numTowns) +
                       " towns...");
    timer.reset();
    timer.start();
    addTowns(map, numTowns, labelNames);
    timer.stop();
    gen::config::print("Finished generating towns in " +
                       gen::config::toString(timer.getTime()) + " seconds.\n");

    outputMap(map);

    totalTimer.stop();
    gen::config::print("\nFinished generating map in " + 
                       gen::config::toString(totalTimer.getTime()) + " seconds.");
    
    return 0;
}
