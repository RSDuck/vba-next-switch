
#ifndef TINYXML_H
#define TINYXML_H
#pragma once

#include "tinyxml.h"
#include <string>
#include <map>

#define DEFINE_XML_PROP(name) \
	public:\
		string Get##name() const;\
	private:\
		string m_##name;

using namespace std;

class CRomPathSettings
{

public:
	CRomPathSettings();
	virtual ~CRomPathSettings();

public:
	bool Load(const string & sXmlPath);

	map<string,string>::iterator GetDeviceMapBegin();
	map<string,string>::iterator GetDeviceMapEnd();
	map<string,string>::iterator CRomPathSettings::FindDevice(std::string Device);
	void CreateFavoritesDoc(void);

	DEFINE_XML_PROP(XboxGamePath)
	DEFINE_XML_PROP(Xbox360GamePath)
	DEFINE_XML_PROP(DefaultFontName)

private:
	TiXmlDocument m_XmlDoc;
	bool m_bLoaded;
	map<string, string> m_DeviceMap;
public:
	string m_PreviewPath;
};

#endif
