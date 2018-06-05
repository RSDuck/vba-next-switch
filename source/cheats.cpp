#include <fstream>
#include <stdbool.h>
#include <stdint.h>
#include <sstream>
#include <iostream>
#include <string>
#include "switch/ui.h"

#include "GBACheats.h"

void cheatsReadHumanReadable(const char *file) {
	std::ifstream infile(file);
	std::string line;
    std::string description = "";
    int curLine = 1;
	while (std::getline(infile, line)) {

		if(line.length() == 0)
            continue;
        
        if(line[0] == '#') {
            description = line.substr(1);
            continue;
        }



        std::istringstream iss(line);
        std::string cheatFirstPart;
        std::string cheatSecondPart;

        bool enabled = true;

        if (!(iss >> cheatFirstPart >> cheatSecondPart)) {
            uiStatusMsg("Failed to parse cheat at line " + curLine);
			continue;
		}
        cheatFirstPart = std::string( 8 - cheatFirstPart.length(), '0').append(cheatFirstPart);
        cheatSecondPart = std::string( 8 - cheatSecondPart.length(), '0').append(cheatSecondPart);
        std::string fullCheat = cheatFirstPart.append(cheatSecondPart);

        iss >> std::boolalpha >> enabled;

        cheatsAddGSACode(fullCheat.c_str(), description.c_str(), true);
        if(!enabled)
            cheatsDisable(cheatsNumber-1);

        curLine++;
	}
}

void cheatsWriteHumanReadable(const char *file) {
    std::string description = "";
    std::ofstream outfile(file);
    outfile << std::boolalpha;   

    for(int i = 0; i < cheatsNumber; i++) {
        std::string localDesc = cheatsList[i].desc;
        if(localDesc != description) {
            description = localDesc;
            outfile << "#" << description << std::endl;
        }

        std::string codestring = cheatsList[i].codestring;
        std::string cheatFirstPart = codestring.substr(0,8);
        std::string cheatSecondPart = codestring.substr(8);
        outfile << cheatFirstPart << " " << cheatSecondPart << " " << cheatsList[i].enabled << std::endl;
    }

}
