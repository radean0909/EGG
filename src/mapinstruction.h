#ifndef MAPINSTRUCTION_H
#define MAPINSTRUCTION_H

#include <string>
#include <vector>


namespace gen {
class MapInstruction {
		public:
			MapInstruction() {};
			MapInstruction(std::string fname, std::vector<double> p) { 
                FnName = fname;
                Params = p;
            };
			
			std::string FnName;
			std::vector<double> Params;

			template <class Archive>
			void serialize( Archive & ar) {
				ar( CEREAL_NVP(FnName), CEREAL_NVP(Params) );
			}
	};
}

#endif