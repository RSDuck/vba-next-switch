
#include "RomSettings.h"
 

#define GET_SIMPLE_PROP(name) \
	element = root.FirstChildElement(#name).Element();\
	if (element)\
		m_##name = string(element->GetText());

#define DEFINE_GETFUNC(name)\
	string CRomPathSettings::Get##name() const\
	{\
		if (m_bLoaded)\
		return m_##name;\
		return "";\
	}
	

CRomPathSettings::CRomPathSettings() : m_bLoaded(false)
{
}

CRomPathSettings::~CRomPathSettings()
{	
}


void CRomPathSettings::CreateFavoritesDoc(void)
{
	/*TiXmlDocument doc;
	TiXmlDeclaration * decl = new TiXmlDeclaration( "1.0", "", "" );
	doc.LinkEndChild( decl );
	
	TiXmlElement * element = new TiXmlElement( "Favorite" );
	doc.LinkEndChild( element );
	
	TiXmlElement * windows = new TiXmlElement( "Game" );  
	element->LinkEndChild( windows ); 

	// Declare a printer
	TiXmlPrinter printer;

	// attach it to the document you want to convert in to a std::string
	doc.Accept(&printer);

	// Create a std::string and copy your document data in to the string
	std::string str = printer.CStr();

	str.c_str();*/
}

bool CRomPathSettings::Load(const string & sXmlPath)
{	
	if (!m_XmlDoc.LoadFile(sXmlPath.c_str()))
	{
		 
		return false;
	}

	TiXmlElement * element = m_XmlDoc.FirstChildElement("Settings");
	if (!element)
	{
		 
		return false;
	}

	TiXmlHandle root = TiXmlHandle(element);
 

	element = root.FirstChildElement("MappedDrive").Element();
	for(element; element; element = element->NextSiblingElement("MappedDrive"))
	{
		string DriveName = element->Attribute("DriveName");
		if (DriveName == "")
		{
			 
			continue;
		}
		string DevicePath = element->Attribute("Path");
		if (DevicePath == "")
		{
			 
			continue;
		}
		m_DeviceMap[DriveName] = DevicePath;
	}


	element = root.FirstChildElement("PreviewPath").Element();
	if (element)
		m_PreviewPath = string(element->GetText());

	m_bLoaded = true;

	return true;
}

map<string,string>::iterator CRomPathSettings::GetDeviceMapBegin()
{
	
	return m_DeviceMap.begin();
}

map<string,string>::iterator CRomPathSettings::GetDeviceMapEnd()
{
	return m_DeviceMap.end();
}

map<string,string>::iterator CRomPathSettings::FindDevice(std::string Device)
{

	std::map<std::string, std::string>::iterator it;
	string key = "";

	for (it = m_DeviceMap.begin(); it != m_DeviceMap.end(); ++it)
	{
		if (it->second == Device)
		{
			key = it->first;
			break;
		}
	}

	if (key.length() > 0)
		return it;
	else
		return m_DeviceMap.begin();
}
 