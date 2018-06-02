#include "theme.h"

void setTheme(themeMode_t t) {
    switch (t) {
        case modeLight:
            currentTheme.backgroundColor = MakeColor(239, 239, 239, 255);
            currentTheme.textColor = MakeColor(45, 45, 45, 255);
            currentTheme.textActiveColor = MakeColor(50, 80, 240, 255);
            currentTheme.textActiveBGColor = MakeColor(253, 253, 253, 255);
            currentTheme.btnA = btnADark;
            currentTheme.btnB = btnBDark;
            currentTheme.btnX = btnXDark;
            currentTheme.btnY = btnYDark;
            break;
        case modeDark:
            currentTheme.backgroundColor = MakeColor(45, 45, 45, 255);
            currentTheme.textColor = MakeColor(255, 255, 255, 255);
            currentTheme.textActiveColor = MakeColor(0, 255, 197, 255);
            currentTheme.textActiveBGColor = MakeColor(33, 34, 39, 255);
            currentTheme.btnA = btnALight;
            currentTheme.btnB = btnBLight;
            currentTheme.btnX = btnXLight;
            currentTheme.btnY = btnYLight;
            break;        
    }
}

theme_t currentTheme;