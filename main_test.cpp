// xmlparser.cpp : Defines the entry point for the console application.
//


#include "xmlparser.h"
#include <string>

using namespace gnilk::xml;

static std::string xmldata("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
        <component name=\"Microsoft-Windows-International-Core-WinPE\" processorArchitecture=\"x86\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\
            <SetupUILanguage>\
                <UILanguage>en-US</UILanguage>\
                <WillShowUI>OnError</WillShowUI>\
            </SetupUILanguage>\
            <UILanguage>en-US</UILanguage>\
            <InputLocale>0409:00000409</InputLocale>\
            <SystemLocale>en-US</SystemLocale>\
            <UserLocale>en-US</UserLocale>\
        </component>");



class ParseEventTracker : public IParseEvents
{
public:
      virtual void StartTag(ITag *pTag) {
        printf("ST: <%s>\n",pTag->getName().c_str());
      }
      virtual void EndTag(ITag *pTag) {
        printf("ET: </%s>\n",pTag->getName().c_str());       
        if (pTag->getName() == "WillShowUI") {
          printf("  '%s'\n",pTag->getContent().c_str());
        }
      }
      virtual void ContentTag(ITag *pTag, const std::string &content) {
        printf("CT: %s\n",pTag->getName().c_str());       
      }

};
int main(int argc, char* argv[])
{

  ParseEventTracker events;
  Document *pDoc = Parser::loadXML(xmldata, &events);
  pDoc->dumpTagTree(pDoc->getRoot(),0);



	return 0;
}


