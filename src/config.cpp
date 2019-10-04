#include "config.h"

namespace gen {
namespace config {

unsigned int seed = 0;
double resolution = 0.08;
std::string outfileExt = ".png";
std::string outfile = "output" + outfileExt;
std::string voronoiFile = "";
std::string heightMapFile = "";
std::string instructionFile = "";
double erosionAmount = -1.0;
int erosionIterations = 3;
int numCities = -1;
int numTowns = -1;
int imageWidth = 1920;
int imageHeight = 1080;
double defaultExtentsHeight = 20.0;
double drawScale = 1.0;
double mapScale = .2;
double mapOffset = .25;
bool enableSlopes = true;
bool enableRivers = true;
bool enableContour = true;
bool enableBorders = true;
bool enableCities = true;
bool enableTowns = true;
bool enableLabels = true;
bool enableAreaLabels = true;
bool verbose = false;
bool voronoiCreation = false;
bool heightmapCreation = false;
bool instructionCreation = false;
bool randomGeneration = false;

void print(std::string msg) {
    if (gen::config::verbose) {
        std::cout << msg << std::endl;
    }
}

bool parseOptions(int argc, char **argv) {
    OptionArgs opts;
    void *argtable[] = {
        opts.help         = arg_litn("h", "help", 0, 1, "display this help and exit"),
        opts.seed         = arg_strn("s", "seed", "<uint>", 0, 1, "set random generator seed"),
        opts.timeseed     = arg_litn(NULL, "timeseed", 0, 1, "set seed from system time"),
        opts.resolution   = arg_dbln("r", "resolution", "<float>", 0, 1, "level of map detail"),
        opts.outfile      = arg_filen("o", "output", "filename", 0, 1, "output file"),
        opts.output       = arg_filen(NULL, NULL, "<file>", 0, 1, "output file"),
        opts.instructionfile = arg_filen(NULL, "instruction-input", "<file>", 0, 1, "specifies a map instruction jsom file to generate map alterations"),
        opts.voronoifile = arg_filen(NULL, "voronoi-input", "<file>", 0, 1, "specifies a voronoi input file to generate the map from" ),
        opts.heightmapfile = arg_filen(NULL, "heightmap-input", "<file>", 0, 1, "specifies a heightmap input to generate the map from (requires voronoi input as well)"),
        opts.voronoicreation = arg_litn(NULL, "create-voronoi", 0, 1, "enable creation of voronoi input file"),
        opts.heightmapcreation = arg_litn(NULL, "create-heightmap", 0, 1, "enable creation of heightmap input file"),
        opts.instructioncreation = arg_litn(NULL, "create-instruction", 0, 1, "enable creation of map creation instructions when generating map"),
        opts.eroamount    = arg_dbln("e", "erosion-amount", "<float>", 0, 1, "erosion amount"),
        opts.erosteps     = arg_intn(NULL, "erosion-steps", "<int>", 0, 1, "number of erosion iterations"),
        opts.ncities      = arg_intn("c", "cities", "<int>", 0, 1, "number of generated cities"),
        opts.ntowns       = arg_intn("t", "towns", "<int>", 0, 1, "number of generated towns"),
        opts.size         = arg_strn(NULL, "size", "<widthpx:heightpx>", 0, 1, "set output image size"),
        opts.drawscale    = arg_dbln(NULL, "draw-scale", "<float>", 0, 1, "set scale of drawn lines/points"),
        opts.mapscale     = arg_dbln(NULL, "map-scale", "<float>", .1, 1, "set the global scale for the map region"),
        opts.mapoffset    = arg_dbln(NULL, "map-offset", "<float>", 0, 1, "set the offset from the north pole to start the map"),
        opts.generaterandom = arg_litn(NULL, "random-generation", 0, 1, "enable random map generation"),
        opts.noslopes     = arg_litn(NULL, "no-slopes", 0, 1, "disable slope drawing"),
        opts.norivers     = arg_litn(NULL, "no-rivers", 0, 1, "disable river drawing"),
        opts.nocontour    = arg_litn(NULL, "no-contour", 0, 1, "disable contour drawing"),
        opts.noborders    = arg_litn(NULL, "no-borders", 0, 1, "disable border drawing"),
        opts.nocities     = arg_litn(NULL, "no-cities", 0, 1, "disable city drawing"),
        opts.notowns      = arg_litn(NULL, "no-towns", 0, 1, "disable town drawing"),
        opts.nolabels     = arg_litn(NULL, "no-labels", 0, 1, "disable label drawing"),
        opts.noarealabels = arg_litn(NULL, "no-arealabels", 0, 1, "disable area label drawing"),
        opts.drawinfo     = arg_litn(NULL, "drawing-supported", 0, 1, "display whether drawing is supported and exit"),
        opts.verbose      = arg_litn("v", "verbose", 0, 1, "output additional information to stdout"),
        opts.end          = arg_end(20)
    };

    if (arg_nullcheck(argtable) != 0) {
        std::cout << "error: insufficient memory." << std::endl;
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return false;
    }

    int nerrors = arg_parse(argc,argv,argtable);

    std::string progname("map_generation");
    if (opts.help->count > 0) {
        std::cout << "Usage: " << progname;
        arg_print_syntax(stdout, argtable, "\n");
        std::cout << "\nOptions:\n" << std::endl;
        arg_print_glossary(stdout, argtable, "  %-30s %s\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return false;
    }

    if (nerrors > 0) {
        arg_print_errors(stdout, opts.end, progname.c_str());
        std::cout << "Try '" << progname << " --help' for more information." << std::endl;
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return false;
    }

    if (!_displayInfo(opts)) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return false;
    }

    if (!_setOptions(opts)) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return false;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return true;
}

bool _displayInfo(OptionArgs opts) {
    bool isInfoDisplayed = _displayDrawSupportInfo(opts.drawinfo);

    return !isInfoDisplayed;
}

bool _displayDrawSupportInfo(arg_lit *drawinfo) {
    if (drawinfo->count == 0) {
        return false;
    }

    #ifdef PYTHON_RENDERING_SUPPORTED
        std::string result = "True";
    #else
        std::string result = "False";
    #endif

    std::cout << "--drawing-supported=" << result << std::endl;

    return true;
}

bool _setOptions(OptionArgs opts) {
    if (!_setSeed(opts.timeseed, opts.seed)) { return false; }
    if (!_setResolution(opts.resolution)) { return false; }
    if (!_setOutputFile(opts.outfile, opts.output)) { return false; }
    if (!_enableVoronoiCreation(opts.voronoicreation)) { return false; }
    if (!_enableHeightmapCreation(opts.heightmapcreation)) { return false; }
    if (!_enableInstructionCreation(opts.instructioncreation)) { return false; }
    if (!_setVoronoiInput(opts.voronoifile)) { return false; }
    if (!_setHeightmapInput(opts.heightmapfile)) { return false; }
    if (!_setInstructionInput(opts.instructionfile)) { return false; }
    if (!_setErosionAmount(opts.eroamount)) { return false; }
    if (!_setErosionIterations(opts.erosteps)) { return false; }
    if (!_setNumCities(opts.ncities)) { return false; }
    if (!_setNumTowns(opts.ntowns)) { return false; }
    if (!_setImageSize(opts.size)) { return false; }
    if (!_setDrawScale(opts.drawscale)) { return false; }
    if (!_setMapScale(opts.mapscale)) { return false; }
    if (!_setMapOffset(opts.mapoffset)) { return false; }
    if (!_enableRandomGeneration(opts.generaterandom)) { return false; }
    if (!_disableSlopes(opts.noslopes)) { return false; }
    if (!_disableRivers(opts.norivers)) { return false; }
    if (!_disableContour(opts.nocontour)) { return false; }
    if (!_disableBorders(opts.noborders)) { return false; }
    if (!_disableCities(opts.nocities)) { return false; }
    if (!_disableTowns(opts.notowns)) { return false; }
    if (!_disableLabels(opts.nolabels)) { return false; }
    if (!_disableAreaLabels(opts.noarealabels)) { return false; }
    if (!_setVerbosity(opts.verbose)) { return false; }

    return true;
}

bool _setSeed(arg_lit *timeseed, arg_str *seed) {
    if (timeseed->count > 0) {
        gen::config::seed = (unsigned int)time(NULL);
    } else if (seed->count > 0) {
        std::istringstream istr(seed->sval[0]);
        istr >> gen::config::seed;
    }

    return true;
}

bool _setResolution(arg_dbl *res) {
    if (res->count == 0) {
        return true;
    }

    double r = res->dval[0];
    if (r <= 0) {
        std::cout << "error: resolution must be greater than zero." << std::endl; 
        std::cout << "resolution: " << r << std::endl;
        return false;
    }

    gen::config::resolution = r;

    return true;
}

bool _setOutputFile(arg_file *outfile1, arg_file *outfile2) {
    if (outfile1->count > 0) {
        gen::config::outfile = outfile1->filename[0];
        gen::config::outfileExt = outfile1->extension[0];
    } else if (outfile2->count > 0) {
        gen::config::outfile = outfile2->filename[0];
        gen::config::outfileExt = outfile2->extension[0];
    }

    return true;
}

bool _setVoronoiInput(arg_file *voronoifile) {
    if (voronoifile->count > 0) {
        gen::config::voronoiFile = voronoifile->filename[0];
    }

    return true;
}

bool _setHeightmapInput(arg_file *heightmapfile) {
    if (heightmapfile->count > 0) {
        gen::config::heightMapFile = heightmapfile->filename[0];
    }

    return true;
}

bool _setInstructionInput(arg_file *instructionfile) {
    if (instructionfile->count > 0) {
        gen::config::instructionFile = instructionfile->filename[0];
    }

    return true;
}

bool _enableHeightmapCreation(arg_lit *enableheightmapcreation) {
    if (enableheightmapcreation->count > 0) {
        gen::config::heightmapCreation = true;
    }

    return true;
}

bool _enableVoronoiCreation(arg_lit *enablevoronoicreation) {
    if (enablevoronoicreation->count > 0) {
        gen::config::voronoiCreation = true;
    }

    return true;
}

bool _enableInstructionCreation(arg_lit *enableinstructioncreation) {
    if (enableinstructioncreation->count > 0) {
        gen::config::instructionCreation = true;
    }

    return true;
}

bool _setErosionAmount(arg_dbl *amount) {
    if (amount->count == 0) {
        return true;
    }

    double e = amount->dval[0];
    if (e <= 0) {
        std::cout << "error: erosion amount must be greater than zero." << std::endl; 
        std::cout << "erosion amount: " << e << std::endl;
        return false;
    }

    gen::config::erosionAmount = e;

    return true;
}

bool _setErosionIterations(arg_int *iterations) {
    if (iterations->count == 0) {
        return true;
    }

    int n = iterations->ival[0];
    if (n < 0) {
        std::cout << "error: erosion iterations must be greater than or equal to zero." << std::endl; 
        std::cout << "erosion iterations: " << n << std::endl;
        return false;
    }

    gen::config::erosionIterations = n;

    return true;
}

bool _setNumCities(arg_int *ncities) {
    if (ncities->count == 0) {
        return true;
    }

    int n = ncities->ival[0];
    if (n < 0) {
        std::cout << "error: number of cities must be greater than or equal to zero." << std::endl; 
        std::cout << "number of cities: " << n << std::endl;
        return false;
    }

    gen::config::numCities = n;

    return true;
}

bool _setNumTowns(arg_int *ntowns) {
    if (ntowns->count == 0) {
        return true;
    }

    int n = ntowns->ival[0];
    if (n < 0) {
        std::cout << "error: number of towns must be greater than or equal to zero." << std::endl; 
        std::cout << "number of towns: " << n << std::endl;
        return false;
    }

    gen::config::numTowns = n;

    return true;
}

bool _setImageSize(arg_str *size) {
    if (size->count == 0) {
        return true;
    }

    std::string sizestr(size->sval[0]);
    std::string delimiter = ":";
    size_t dpos = sizestr.find(delimiter);
    std::string invalidmsg("error: image size must be in form <widthpx:heighpx>.");
    if (dpos == std::string::npos) {
        std::cout << invalidmsg << std::endl;
        return false;
    }

    std::string widthstr = sizestr.substr(0, dpos);
    std::string heightstr = sizestr.substr(dpos + 1, sizestr.size());
    if (widthstr.size() == 0 || heightstr.size() == 0) {
        std::cout << invalidmsg << std::endl;
        return false;
    }

    int widthpx;
    std::istringstream istreamWidth(widthstr);
    istreamWidth >> widthpx;
    if (istreamWidth.fail() || widthpx <= 0) {
        std::cout << "error: invalid image width value." << std::endl;
        std::cout << "width value: " << widthstr << std::endl;
        return false;
    }

    int heightpx;
    std::istringstream istreamHeight(heightstr);
    istreamHeight >> heightpx;
    if (istreamHeight.fail() || heightpx <= 0) {
        std::cout << "error: invalid image height value." << std::endl;
        std::cout << "height value: " << heightstr << std::endl;
        return false;
    }

    gen::config::imageWidth = widthpx;
    gen::config::imageHeight = heightpx;

    return true;
}

bool _setDrawScale(arg_dbl *drawscale) {
    if (drawscale->count == 0) {
        return true;
    }

    double s = drawscale->dval[0];
    if (s <= 0) {
        std::cout << "error: draw scale must be greater than zero." << std::endl; 
        std::cout << "draw size: " << s << std::endl;
        return false;
    }

    gen::config::drawScale = s;

    return true;
}

bool _setMapScale(arg_dbl *mapscale) {
    if (mapscale->count == 0) {
        return true;
    }

    double s = mapscale->dval[0];
    if (s <= .1) {
        std::cout << "error: map scale must be greater than .1" << std::endl; 
        std::cout << "map scale: " << s << std::endl;
        return false;
    }

    gen::config::mapScale = s;

    return true;
    
}

bool _setMapOffset(arg_dbl *mapoffset) {
    if (mapoffset->count == 0) {
        if (gen::config::mapScale + gen::config::mapOffset > 1) {
            gen::config::mapOffset = 1 - gen::config::mapScale;
            std::cout << "warning: map offset reset due to map scale" << std::endl; 
            std::cout << "map offset: " << gen::config::mapOffset << std::endl;
        }
        return true;
    }

    double o = mapoffset->dval[0];
    if (gen::config::mapScale + o > 1) {
        std::cout << "error: map offset + map scale must be less than 1" << std::endl; 
        std::cout << "map offset: " << o << " , map scale: " << gen::config::mapScale << std::endl;
        return false;
    }

    gen::config::mapOffset = o;
    return true;
}

bool _enableRandomGeneration(arg_lit *generaterandom) {
    if (generaterandom->count > 0) {
        gen::config::randomGeneration = true;
    }

    return true;
}

bool _disableSlopes(arg_lit *noslopes) {
    if (noslopes->count > 0) {
        gen::config::enableSlopes = false;
    }

    return true;
}

bool _disableRivers(arg_lit *norivers) {
    if (norivers->count > 0) {
        gen::config::enableRivers = false;
    }

    return true;
}

bool _disableContour(arg_lit *nocontour) {
    if (nocontour->count > 0) {
        gen::config::enableContour = false;
    }

    return true;
}

bool _disableBorders(arg_lit *noborders) {
    if (noborders->count > 0) {
        gen::config::enableBorders = false;
    }

    return true;
}

bool _disableCities(arg_lit *nocities) {
    if (nocities->count > 0) {
        gen::config::enableCities = false;
    }

    return true;
}

bool _disableTowns(arg_lit *notowns) {
    if (notowns->count > 0) {
        gen::config::enableTowns = false;
    }

    return true;
}

bool _disableLabels(arg_lit *nolabels) {
    if (nolabels->count > 0) {
        gen::config::enableLabels = false;
    }

    return true;
}

bool _disableAreaLabels(arg_lit *noarealabels) {
    if (noarealabels->count > 0) {
        gen::config::enableAreaLabels = false;
    }

    return true;
}

bool _setVerbosity(arg_lit *verbose) {
    if (verbose->count > 0) {
        gen::config::verbose = true;
    }

    return true;
}

}
}