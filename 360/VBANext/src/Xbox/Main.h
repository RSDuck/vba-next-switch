#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>


#ifndef MAIN_H
#define MAIN_H

typedef struct 
{
	char rompath[MAX_PATH];
	char version[16];
	int pointfilter;
	int aspect;
	int overscan;
	int render;
	int ntsc;
	int bilinear;
	int muteAudio;
	int numfavs;
	int gamesplayed;
	int showFPS;
	int flashSize;
} t_config;

void DoAchievo(unsigned long AcheivoID);

void UpdatePresence(unsigned long type);

class CVBA360App : public CXuiModule
{
	protected:
	// Override RegisterXuiClasses so that CMyApp can register classes.
	virtual long RegisterXuiClasses();
	// Override UnregisterXuiClasses so that CMyApp can unregister classes. 
	virtual long UnregisterXuiClasses();
};

#endif
