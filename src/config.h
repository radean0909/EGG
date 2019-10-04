#ifndef GEN_CONFIG_H
#define GEN_CONFIG_H

#include <stdio.h>
#include <iostream>
#include <sstream>

#include "argtable3/argtable3.h"

namespace gen {
namespace config {

struct OptionArgs {
	struct arg_lit *help;
	struct arg_lit *timeseed;
	struct arg_str *seed;
	struct arg_dbl *resolution;
	struct arg_file *outfile;
	struct arg_file *output;
    struct arg_lit *voronoicreation;
    struct arg_lit *heightmapcreation;
    struct arg_lit *instructioncreation;
    struct arg_file *voronoifile;
    struct arg_file *heightmapfile;
    struct arg_file *instructionfile;
    struct arg_dbl *eroamount;
    struct arg_int *erosteps;
    struct arg_dbl *mapscale;
    struct arg_dbl *mapoffset;
    struct arg_int *ncities;
    struct arg_int *ntowns;
    struct arg_str *size;
    struct arg_dbl *drawscale;
    struct arg_lit *generaterandom;
    struct arg_lit *noslopes;
    struct arg_lit *norivers;
    struct arg_lit *nocontour;
    struct arg_lit *noborders;
    struct arg_lit *nocities;
    struct arg_lit *notowns;
    struct arg_lit *nolabels;
    struct arg_lit *noarealabels;
	struct arg_lit *drawinfo;
    struct arg_lit *verbose;
	struct arg_end *end;
};

extern unsigned int seed;
extern double resolution;
extern std::string outfileExt;
extern std::string outfile;
extern double erosionAmount;
extern int erosionIterations;
extern int numCities;
extern int numTowns;
extern int imageWidth;
extern int imageHeight;
extern double defaultExtentsHeight;
extern double drawScale;
extern double mapOffset;
extern double mapScale;
extern bool voronoiCreation;
extern bool heightmapCreation;
extern bool instructionCreation;
extern std::string voronoiFile;
extern std::string heightMapFile;
extern std::string instructionFile;
extern bool randomGeneration;
extern bool enableSlopes;
extern bool enableRivers;
extern bool enableContour;
extern bool enableBorders;
extern bool enableCities;
extern bool enableTowns;
extern bool enableLabels;
extern bool enableAreaLabels;
extern bool verbose;

template<class T>
std::string toString(T item) {
    std::ostringstream sstream;
    sstream << item;

    return sstream.str();
}

void print(std::string msg);
bool parseOptions(int argc, char **argv);
bool _displayInfo(OptionArgs opts);
bool _displayDrawSupportInfo(arg_lit *drawinfo);
bool _setOptions(OptionArgs opts);
bool _setSeed(arg_lit *timeseed, arg_str *seed);
bool _setResolution(arg_dbl *res);
bool _setOutputFile(arg_file *outfile1, arg_file *outfile2);
bool _setErosionAmount(arg_dbl *amount);
bool _setErosionIterations(arg_int *iterations);
bool _setNumCities(arg_int *ncities);
bool _setNumTowns(arg_int *ntowns);
bool _setImageSize(arg_str *size);
bool _setDrawScale(arg_dbl *linesize);
bool _setMapScale(arg_dbl *mapscale);
bool _setMapOffset(arg_dbl *mapoffset);
bool _enableVoronoiCreation(arg_lit *voronoicreation);
bool _enableHeightmapCreation(arg_lit *heightmapcreation);
bool _enableInstructionCreation(arg_lit *instructioncreation);
bool _setVoronoiInput(arg_file *voronoifile);
bool _setHeightmapInput(arg_file *heightmapfile);
bool _setInstructionInput(arg_file *instructionfile);
bool _enableRandomGeneration(arg_lit *generaterandom);
bool _disableSlopes(arg_lit *noslopes);
bool _disableRivers(arg_lit *norivers);
bool _disableContour(arg_lit *nocontour);
bool _disableBorders(arg_lit *noborders);
bool _disableCities(arg_lit *nocities);
bool _disableTowns(arg_lit *notowns);
bool _disableLabels(arg_lit *nolabels);
bool _disableAreaLabels(arg_lit *noarealabels);
bool _setVerbosity(arg_lit *verbose);

}
}

#endif