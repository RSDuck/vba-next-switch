#include <fstream>
#include <stdbool.h>
#include <stdint.h>
#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>
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
        std::string cheat;

        bool enabled = true;

        if (!(iss >> cheat)) {
            uiStatusMsg("Failed to parse cheat at line " + curLine);
			continue;
		}

        if(cheat.length() < 12) {
            std::string secPart;
            iss >> secPart;
            cheat.append(secPart);
        }

        std::transform(cheat.begin(), cheat.end(),cheat.begin(), ::toupper);

        if(cheat.length() == 16) {
            // Gameshark v3
            cheatsAddGSACode(cheat.c_str(), description.c_str(), true);
        } else if(cheat.length() == 12) {
            // Gameshark v1/2
            cheat.insert(8, "0000");
            cheatsAddGSACode(cheat.c_str(), description.c_str(), false);
        } else {
            uiStatusMsg("Failed to parse cheat at line " + curLine);
        }

        iss >> std::boolalpha >> enabled;

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
        std::string cheatSecondPart;
        if(cheatsList[i].v3)
            cheatSecondPart = codestring.substr(8);
        else
            cheatSecondPart = codestring.substr(12);
        outfile << cheatFirstPart << " " << cheatSecondPart << " " << cheatsList[i].enabled << std::endl;
    }

}