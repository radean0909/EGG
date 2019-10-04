#ifndef RESOURCES_H
#define RESOURCES_H

#define RESOURCES_EXECUTABLE_DIRECTORY 	"D:/Encounter/MapGen/src/lib/FantasyMapGenerator/build/x64-Test"
#define RESOURCES_FONT_DATA_DIRECTORY 	"D:/Encounter/MapGen/src/lib/FantasyMapGenerator/build/x64-Test/fontdata"
#define RESOURCES_CITY_DATA_DIRECTORY 	"D:/Encounter/MapGen/src/lib/FantasyMapGenerator/build/x64-Test/citydata"
#define RESOURCES_FONT_DATA_RESOURCE 	"D:/Encounter/MapGen/src/lib/FantasyMapGenerator/build/x64-Test/fontdata/fontdata.json"
#define RESOURCES_CITY_DATA_RESOURCE 	"D:/Encounter/MapGen/src/lib/FantasyMapGenerator/build/x64-Test/citydata/countrycities.json"

#include <string>

namespace gen{
namespace resources {
    
extern std::string getExecutableDirectory();
extern std::string getFontDataDirectory();
extern std::string getCityDataDirectory();
extern std::string getFontDataResource();
extern std::string getCityDataResource();
    
}
}

#endif
