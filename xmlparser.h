#pragma once
/*-------------------------------------------------------------------------
File    : $Archive: parser.h $
Author  : $Author: Fkling $
Version : $Revision: 1 $
Orginal : 2012-05-10, 15:50
Descr   : Implements a fairly speedy XML parser

Modified: $Date: $ by $Author: Fkling $
---------------------------------------------------------------------------
TODO: [ -:Not done, +:In progress, !:Completed]
<pre>
- Split string utils out of here
</pre>


\History
- 10.03.2012, FKling, Implementation in Java, converted to C++ not long after

---------------------------------------------------------------------------*/

#include <string>
#include <list>
#include <stack>
#include <functional>

namespace gnilk {
  namespace xml {

    // -- start config
#define XML_PARSER_STATIC_STRING_UTIL

    // -- end config



#ifdef XML_PARSER_STATIC_STRING_UTIL
#define SUTIL_INVOKE(__x__) (StringUtilStatic::__x__)
#else
#define SUTIL_INVOKE(__x__) (sUtil->__x__)
#endif

    // TODO: Move to own file
    class StringUtil
    {
      static std::string whiteSpaces;


    public:
      void trimRight( std::string& str, const std::string& trimChars = whiteSpaces );
      void trimLeft( std::string& str, const std::string& trimChars = whiteSpaces );
      std::string &trim( std::string& str, const std::string& trimChars = whiteSpaces );
      std::string toLower(std::string s);
      bool equalsIgnoreCase(std::string a, std::string b);

    };

    class StringUtilStatic
    {
      static std::string whiteSpaces;

    public:
      __inline static void trimRight( std::string& str, const std::string& trimChars = whiteSpaces )
      {
        std::string::size_type pos = str.find_last_not_of( trimChars );
        str.erase( pos + 1 );    
      }

      __inline static void trimLeft( std::string& str, const std::string& trimChars = whiteSpaces )
      {
        std::string::size_type pos = str.find_first_not_of( trimChars );
        str.erase( 0, pos );
      }

      __inline static std::string &trim( std::string& str, const std::string& trimChars = whiteSpaces )
      {
        trimRight( str, trimChars );
        trimLeft( str, trimChars );
        return str;
      }

      __inline static std::string toLower(std::string s) {
        std::string res = "";
        for(size_t i=0;i<s.length();i++) {
          res+=((char)tolower(s.at(i)));
        }
        return res;
      }
      __inline static bool equalsIgnoreCase(std::string a, std::string b) {
        std::string sa = toLower(a);
        std::string sb = toLower(b);
        return (sa==sb);
      }

    };

    //
    // Here are the public interfaces
    //
    class IAttribute {
    public:
      virtual std::string& getName() = 0;
      virtual std::string& getValue() = 0;
    };

    class ITag {
    public:
      virtual bool hasContent() = 0;
      virtual std::string &getName() = 0;
      virtual std::string &getContent() = 0;

      virtual std::string toString() = 0;

      virtual bool hasAttribute(std::string name) = 0;
      virtual std::string getAttributeValue(std::string name, std::string defValue) = 0;


      virtual std::list<IAttribute *> &getAttributes() = 0;
      virtual std::list<ITag *> &getChildren() = 0;
      virtual ITag *getParent() = 0;
      virtual ITag *getFirstChild(std::string name) = 0;
      virtual ITag *getChildWithAttributeValue(std::string name, std::string attribute, std::string value) = 0;
    };

    typedef std::function<void(ITag *tag, std::list<IAttribute *>&attributes)> OnTagDelegate;

    class IDocument {
    public:
      virtual ITag *getRoot() = 0;
      virtual void traverse(OnTagDelegate startHandler, OnTagDelegate endHandler) = 0;
      virtual void traverseFromNode(ITag *node, OnTagDelegate startHandler, OnTagDelegate endHandler) = 0;      
    };

    class IParseEvents
    {
    public:
      virtual void StartTag(ITag *pTag) = 0;
      virtual void EndTag(ITag *pTag) = 0;
      virtual void ContentTag(ITag *pTag, const std::string &content) = 0;
    };

    //
    // internal parser classes here and default implementations of said interfaces
    //

    class Attribute : public IAttribute {
    private:
      std::string name;
      std::string value;
    public:
      Attribute() {}
      virtual std::string &getName() { return name; }
      void setName(std::string &_name) { name = _name; }

      virtual std::string& getValue() { return value; }
      void setValue(std::string &_value) {value = _value; }
    };

    class Tag : public ITag {
    private:
      std::string name;
      std::string content;

      std::list<IAttribute *> attributes;
      std::list<ITag *>children;
      Tag *parent;
    public:
      Tag(std::string _name);
      virtual ~Tag();

      virtual bool hasContent();
      virtual std::string toString();

      void addAttribute(std::string _name, std::string _value);
      void addChild(Tag *tag);

      void setParent(Tag *tag);
      ITag *getParent();

      virtual std::string &getName() { return name; }
      void setName(std::string &_name) { name = _name; }

      virtual std::string &getContent() { return content; }
      void setContent(std::string &_content) { content = _content; }

      bool hasAttribute(std::string name);
      std::string getAttributeValue(std::string name, std::string defValue);


      virtual std::list<IAttribute *> &getAttributes() { return attributes; }
      virtual std::list<ITag *> &getChildren() { return children; }
      virtual ITag *getFirstChild(std::string name);
      virtual ITag *getChildWithAttributeValue(std::string name, std::string attribute, std::string value);
    };


    // Document container
    // - TODO: Keep <?xml > strings in separate tag lists
    class Document : public IDocument {
      Tag *root;

    public:
      Document();
      virtual ~Document();

      //public std::string &getData() { return data; };
      virtual ITag *getRoot() { return root; };
      virtual void traverse(OnTagDelegate startHandler, OnTagDelegate endHandler);
      virtual void traverseFromNode(ITag *node, OnTagDelegate startHandler, OnTagDelegate endHandler);
      void setRoot(Tag *pRoot) { root = pRoot; }
      void dumpTagTree(ITag *root, int depth);


    private:
      // OnTagDelegate startHandler;
      // OnTagDelegate endHandler;

      void traverseNodes(OnTagDelegate startHandler, OnTagDelegate endHandler, std::list<ITag *> &tags);
      std::string indentString(int depth);
    };

    //
    // TODO: Break this out to 'xmlutils.h/cpp'
    //
    #define DOCPATH_DEFAULT_SEPARATOR (".")

    class DocPath {
    public:
      DocPath();
      DocPath(std::string separator);

      ITag *findFirst(Document *doc, std::string tag, std::string value);
    private:
      void onDefinitionTagDataStart(ITag *tag, std::list<IAttribute *>&attributes);
      void onDefinitionTagDataEnd(ITag *tag, std::list<IAttribute *>&attributes);
 
      std::string pathSeparator;
      ITag *findResult;
      std::string searchTag;
      std::string searchValue;
    };

    enum kParseState {
      psConsume,
      psTagStart,
      psEndTagStart,
      psTagAttributeName,
      psTagAttributeValue,
      psTagContent,
      psTagHeader,
      psCommentStart,
      psCommentConsume,
      psDocType,
    };
    enum kParseMode {
      pmStream,
      pmDOMBuild,
    };

    //
    // Had to do this in order to try out a few things without to much changes
    // context is an internal class
    //
    class IParseContext {
    public:
      virtual Tag *createTag(std::string name) = 0;
      virtual void endTag(std::string tok) = 0;
      virtual void commitTag(Tag *pTag) = 0;

      virtual void rewind() = 0;
      virtual int nextChar() = 0;
      virtual int peekNextChar() = 0;
      virtual void changeState(kParseState newState) = 0;
    public:
      // the guilty ones...
      Tag *tagCurrent;
      std::string attrName;
      std::string attrValue;

    };

    // Actual parser
    // TODO: Track inner XML by storing startIndex/endIndex of data set when parsing tag's
    //
    class Parser : public IParseContext {
    protected:
      Parser();
    public:
      Parser(std::string _data);
      Parser(std::string _data, IParseEvents *pEventHandler);
      virtual ~Parser();

      static Document *loadXML(std::string _data, IParseEvents *pEventHandler = NULL);
      Document *getDocument() { return pDocument; }

    protected:
      virtual void initialize(std::string _data, IParseEvents *pEventHandler);
      virtual void parseData();
      virtual void changeState(kParseState newState);

      Tag *createTag(std::string name);
      void endTag(std::string tok);
      void commitTag(Tag *pTag);

      void rewind();
      int nextChar();
      int peekNextChar();

      void enterNewState();


    protected:
      StringUtil *sUtil;
      Document *pDocument;
      Tag *root;
      kParseState state;
      kParseState oldState;
      kParseMode parseMode;
      std::stack<Tag *> tagStack;
      int idxCurrent;
      std::string data;
      IParseEvents *pEventHandler;
      // parser variables
      std::string token;

    };

    // -------------- Main stuff ends here, rest is just for performance testing of various calling techniques

    // 
    // class where each state is implemented in a seprate function
    //
    class ParseStateFunc : public Parser{
      /////////
    public:
      ParseStateFunc(std::string _data, IParseEvents *pEventHandler);

      virtual void parseData();       
      __inline void stateConsume(char c);
      __inline void stateCommentStart(char c);
      __inline void stateTagStart(char c);
      __inline void stateEndTagStart(char c);
      __inline void stateTagHeader(char c);
      __inline void stateCommentConsume(char c);
      __inline void stateAttributeName(char c);
      __inline void stateAttributeValue(char c);
      __inline void stateTagContent(char c);
      __inline void stateDTDDocTypeContent(char c);
    };


    //
    // -- classes related to the state-class parser implementation
    // i.e each state has it's own class..

    class IParseState {
    public:
      virtual void enter() = 0;
      virtual void consume(char c) = 0;
      virtual void leave() = 0;
    };
    class ParseStateImpl : public IParseState {
    public:
      IParseContext *pContext;
      std::string token;

    public:
      virtual void enter() {}
      virtual void consume(char c) {}
      virtual void leave() {}

      // -- helpers
      Tag *createTag(std::string name);
      void endTag(std::string tok);
      void commitTag(Tag *pTag);

      void rewind();
      int nextChar();
      int peekNextChar();
      void changeState(kParseState newState);

    };
    class StateConsume : public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateCommentStart : public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateTagStart : public ParseStateImpl {
    public:

      virtual void enter();
      virtual void consume(char c);
    };
    class StateTagEndStart : public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateTagHeader : public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateCommentConsume: public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateAttributeName: public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateAttributeValue: public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateTagContent: public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };
    class StateTagDTDDocType: public ParseStateImpl {
    public:
      virtual void enter();
      virtual void consume(char c);
    };


    class ParseStateClasses : public Parser {
    private:
      IParseState *pState;
      StateConsume stateConsume;
      StateCommentStart stateCommentStart;
      StateTagStart stateTagStart;
      StateTagEndStart stateTagEndStart;
      StateTagHeader stateTagHeader;
      StateCommentConsume stateCommentConsume;
      StateAttributeName stateAttributeName;
      StateAttributeValue stateAttributeValue;
      StateTagContent stateTagContent;
      StateTagDTDDocType stateTagDTDDocType;
    public:
      ParseStateClasses(std::string _data, IParseEvents *pEventHandler);
      virtual void changeState(kParseState newState);
      virtual void initialize(std::string _data, IParseEvents *pEventHandler);
      virtual void parseData();
    };
  }

}