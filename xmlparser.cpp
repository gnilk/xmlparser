/*-------------------------------------------------------------------------
File    : $Archive: parser.cpp $
Author  : $Author: Fkling $
Version : $Revision: 1 $
Orginal : 2012-05-10, 15:50
Descr   : Implements a fairly speedy XML parser in less than 1000 lines of code

Modified: $Date: $ by $Author: Fkling $
---------------------------------------------------------------------------
TODO: [ -:Not done, +:In progress, !:Completed]
<pre>
  ! Implement discard mode (i.e. just parse, don't create nodes)
  + define a callback interface and enable it (parser can be used as a SAX parser)
    [-] Add states to internal TAG node for 'start','end','content' callback's => No needed
  - Abstract the stream handling (nextChar, peek and rewind)
  - Remove constant token definitions
  - Try to UTF-8 the code (just change the std::string stuff to std::wstring and off we go...)
  ! Check if we really need to trim the strings during parsing -> we do need this (for callbacks to work)
</pre>

\History
- 10.03.2012, FKling, Implementation in Java, converted to C++ not long after

---------------------------------------------------------------------------*/
#include "xmlparser.h"          

using namespace gnilk::xml;

Parser::Parser() {
    // Inherited only comes here
}
Parser::Parser(std::string _data) {
  initialize(_data, NULL);
}
Parser::Parser(std::string _data, IParseEvents *pEventHandler) {
  initialize(_data, pEventHandler);
}

void Parser::initialize(std::string _data, IParseEvents *pEventHandler)
{
#ifndef STATIC_STRING_UTIL
  sUtil = new StringUtil();
#endif
  attrName ="";
  attrValue="";
  token = "";

  this->pEventHandler = pEventHandler;
  data = _data;
  root = new Tag("root");
  pDocument = new Document();
  pDocument->setRoot(root);
  idxCurrent = 0;
  state = oldState = psConsume;
  parseMode = pmDOMBuild;
  parseData();
}

Parser::~Parser() {
#ifndef STATIC_STRING_UTIL
    delete sUtil;
#endif
}

Document *Parser::loadXML(std::string _data, IParseEvents *pEventHandler)
{
  Parser p(_data, pEventHandler);
  return p.getDocument();
}

void Parser::rewind() {
  idxCurrent--;
}

int Parser::nextChar() {
  if (idxCurrent >= (int)data.length()) return EOF;
  return data.at(idxCurrent++);
}

int Parser::peekNextChar() {
  if (idxCurrent >= (int)data.length()) return EOF;
  return data.at(idxCurrent);
}

void Parser::changeState(kParseState newState) {
  oldState = state;
  state = newState;
  enterNewState();
}

Tag* Parser::createTag(std::string name) {
  Tag *tag = new Tag(name);
  return tag;
}

void Parser::endTag(std::string tok) {
  Tag *popped = NULL;
  if (!SUTIL_INVOKE(equalsIgnoreCase(tagStack.top()->getName(), tok))) {
    Tag *top = tagStack.top();
    // can be an empty tag, like <br />
    if (top->hasContent() == false) { 
      popped = tagStack.top(); 
      tagStack.pop();
    } else {
#ifdef _DEBUG
      printf("WARN: Illegal XML, end-tag has no corrsponding start tag!\n");
#endif
    }
  } else {
    if (!tagStack.empty()) {
      popped = tagStack.top();
      tagStack.pop();
    }
  }		

  if (pEventHandler != NULL) {
    pEventHandler->EndTag((ITag *)popped);
  }

  // In the streamed mode we don't keep tag's
  if ((parseMode == pmStream) && (popped != NULL)) {
    delete popped;
  }
}

void Parser::commitTag(Tag *pTag)
{
  if (pEventHandler != NULL) {
    pEventHandler->StartTag((ITag*)pTag);
  }
  // Only store in hierarchy if we are building a 'DOM' tree
  if (parseMode == pmDOMBuild) {
    tagStack.top()->addChild(pTag);
  }
  tagStack.push(pTag);
}

void Parser::enterNewState()
{
  if (state == psTagContent) {
    commitTag(tagCurrent);
  }
}

void Parser::parseData() {
  char c;
  std::string attrName = "";
  std::string attrValue = "";
  std::string token = "";
  tagStack.push(root);
  while((c=nextChar())!=EOF) {
    switch(state) 
    {
    case psConsume:
      if (c=='<') {
        int next = peekNextChar();
        if (next == '/') {		// ? '</' - distinguish between token <  and </
          nextChar(); // consume '/'
          changeState(psEndTagStart);
        } else if (next == '!') {
          // Action tag started <!--
          nextChar();
          token = ""; // Reset token
          changeState(psCommentStart);
        } else if (next == '?') {
          // Header tag started '<?xml
          nextChar();
          token = "";
          changeState(psTagHeader);
        } else {
          changeState(psTagStart);
        }
        token="";
      } else {
        token+=c;
      }
      break;
    case psCommentStart : // Make sure we hit '--'
      if ((c == '-') && (peekNextChar()=='-')) {
        nextChar();
        changeState(psCommentConsume);
      } else {
#ifdef _DEBUG
        printf("Warning: Illegal start of tag, expected start of comment ('<!--') but found found '<!-'\n");
#endif
        // TODO: If strict, abort here!
        rewind();   // rewind '-'
        rewind();   // rewind '!'
        changeState(psTagStart);
      }
      break;
    case psCommentConsume:  // parse until -->
      if ((c=='-') && (peekNextChar()=='>')) {
        if (token == "-") {
          nextChar();
          changeState(psConsume);
        }
      } else if (c=='-') {
        token="-";  // Store this in order to track -->
      }
      break;
    case psTagHeader : // <? 
      if (isspace(c)) {
        // drop them
        tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
        token = "";
        changeState(psTagAttributeName);				
      } else {
        token+=c;
      }				
      break;
    case psTagStart :	// from psConsume when finding: '<'          
      if (isspace(c)) {					          
        tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
        token = "";          
        changeState(psTagAttributeName);
      } else if (c=='/' && peekNextChar()=='>') {   // catch tags like '<tag/>'
        nextChar(); // consume '>'
        tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
        token = "";
        commitTag(tagCurrent);
        changeState(psConsume);
      } else if (c=='>') {
        tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
        token = "";
        changeState(psTagContent);
      } else {
        token+=c;
      }				
      break;
    case psEndTagStart : // from psConsume when finding: </
      if (isspace(c)) {
        // drop them
      } else if (c=='>') {
        // trim and terminate token
        std::string tmptok(SUTIL_INVOKE(trim(token)));
        endTag(tmptok);
        // clear token and consume more data
        token = "";			
        changeState(psConsume);
      } else {
        token+=c;
      }				
      break;
    case psTagAttributeName : // from psTagStart when finding white-space, from psTagHeader (<?) when finding white-space
      if (isspace(c)) continue;
      if ((c == '=') && (peekNextChar() == '"')) {
        nextChar(); // consume "
        attrName = token;
        token = "";
        changeState(psTagAttributeValue);
      } else if ((c == '=') && (peekNextChar() == '#')) {
        nextChar(); // consume #
        attrName = token;
        attrValue = "#";
        tagCurrent->addAttribute(attrName,  attrValue);
        token = "";					
      } else if (c=='>') {	// End of tag
        token="";
        changeState(psTagContent);
      } else if ((c=='/') && (peekNextChar()=='>')) {
        nextChar();
        commitTag(tagCurrent);
        endTag(SUTIL_INVOKE(trim(token)));
        token="";
        changeState(psConsume);
      } else if ((c=='?') && (peekNextChar()=='>')) {
        nextChar();
        commitTag(tagCurrent);
        endTag(SUTIL_INVOKE(trim(token)));
        token="";
        changeState(psConsume);          
      } else { 
        token += c;
      }
      break;
    case psTagAttributeValue : // from psTagAttributeName after '='
      if (c=='"') {
        attrValue = token;
        tagCurrent->addAttribute(attrName, attrValue);
        changeState(psTagAttributeName);
      } else {
        token +=c;
      }
      break;
    case psTagContent:
      if (c == '<') {	// can't use 'peekNext' since we might have >< which is legal
        tagCurrent->setContent(SUTIL_INVOKE(trim(token)));
        token = "";
        changeState(psConsume);
        rewind();	// rewind so we will see tag start next time
      } else {
        token += c;
      }
      break;
    } // switch
  } // while (!eof)
} // parseData

// -- Tag's
Tag::Tag(std::string _name) {
  setName(_name);
  content.clear();
}

Tag::~Tag() {

}

void Tag::addAttribute(std::string _name, std::string _value) {
  Attribute *attr = new Attribute();
  attr->setName(_name);
  attr->setValue(_value);
  attributes.push_back(attr);
}

void Tag::addChild(Tag *tag) {
  getChildren().push_back(tag);
}


bool Tag::hasContent() {
  return (!content.empty());
}


std::string Tag::toString() {
  return std::string(name + " ("+content+")");
}


// -- Document container
Document::Document() {
  root = NULL;
}

Document::~Document() {

}

std::string Document::indentString(int depth) {
  std::string s= "";
  for(int i=0;i<depth;i++) s+=" ";
  return s;
}

// DEBUG HELPER!
void Document::dumpTagTree(ITag *root, int depth) {
  std::string indent = indentString(depth);
  //System.out.println(indent+"T:"+root->getName());
  printf("%sT:%s\n",indent.c_str(), root->getName().c_str());
  std::list<ITag *> &tags  = root->getChildren();

  std::list<ITag *>::iterator it = tags.begin();
  while(it != tags.end()) {
    ITag *child = *it;
    dumpTagTree(child, depth+2);
    it++;
  }		
}




/////////// -- StringUtils.cpp
///////// -------- Class StringUtil
std::string StringUtil::whiteSpaces( " \f\n\r\t\v" );

void StringUtil::trimRight( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
{
  std::string::size_type pos = str.find_last_not_of( trimChars );
  str.erase( pos + 1 );    
}


void StringUtil::trimLeft( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
{
  std::string::size_type pos = str.find_first_not_of( trimChars );
  str.erase( 0, pos );
}

std::string &StringUtil::trim( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
{
  trimRight( str, trimChars );
  trimLeft( str, trimChars );
  return str;
}

std::string StringUtil::toLower(std::string s) {
  std::string res = "";
  for(size_t i=0;i<s.length();i++) {
    res+=((char)tolower(s.at(i)));
  }
  return res;
}
bool StringUtil::equalsIgnoreCase(std::string a, std::string b) {
  std::string sa = toLower(a);
  std::string sb = toLower(b);
  return (sa==sb);
}


///////// -------- Class StringUtilStatic
std::string StringUtilStatic::whiteSpaces( " \f\n\r\t\v" );

//void StringUtilStatic::trimRight( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
//{
//  std::string::size_type pos = str.find_last_not_of( trimChars );
//  str.erase( pos + 1 );    
//}
//
//
//void StringUtilStatic::trimLeft( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
//{
//  std::string::size_type pos = str.find_first_not_of( trimChars );
//  str.erase( 0, pos );
//}
//
//std::string &StringUtilStatic::trim( std::string& str, const std::string& trimChars /*= whiteSpaces*/ )
//{
//  trimRight( str, trimChars );
//  trimLeft( str, trimChars );
//  return str;
//}
//
//std::string StringUtilStatic::toLower(std::string s) {
//  std::string res = "";
//  for(size_t i=0;i<s.length();i++) {
//    res+=((char)tolower(s.at(i)));
//  }
//  return res;
//}
//bool StringUtilStatic::equalsIgnoreCase(std::string a, std::string b) {
//  std::string sa = toLower(a);
//  std::string sb = toLower(b);
//  return (sa==sb);
//}

// -- ParseStateFuncs.cpp

ParseStateFunc::ParseStateFunc(std::string _data, IParseEvents *pEventHandler)
{
  initialize(_data, pEventHandler);
}

void ParseStateFunc::stateConsume(char c) {
  if (c=='<') {
    int next = peekNextChar();
    if (next == '/') {		// ? '</' - distinguish between token <  and </
      nextChar(); // consume '/'
      changeState(psEndTagStart);
    } else if (next == '!') {
      // Action tag started <!--
      nextChar();
      token = ""; // Reset token
      changeState(psCommentStart);
    } else if (next == '?') {
      // Header tag started '<?xml
      nextChar();
      token = "";
      changeState(psTagHeader);
    } else {
      changeState(psTagStart);
    }
    token="";
  } else {
    token+=c;
  }
}
void ParseStateFunc::stateCommentStart(char c)
{
  if ((c == '-') && (peekNextChar()=='-')) {
    nextChar();
    changeState(psCommentConsume);
  } else {
#ifdef _DEBUG
    //printf("Warning: Illegal start of tag, expected start of comment ('<!--') but found found '<!-'\n");
#endif
    // TODO: If strict, abort here!
    rewind();   // rewind '-'
    rewind();   // rewind '!'
    changeState(psTagStart);
  }
}
void ParseStateFunc::stateTagStart(char c)
{
  if (isspace(c)) {					          
    tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";          
    changeState(psTagAttributeName);
  } else if (c=='/' && peekNextChar()=='>') {   // catch tags like '<tag/>'
    nextChar(); // consume '>'
    tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    commitTag(tagCurrent);
    changeState(psConsume);
  } else if (c=='>') {
    tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psTagContent);
  } else {
    token+=c;
  }				

}

void ParseStateFunc::stateEndTagStart(char c)
{
  if (isspace(c)) {
    // drop them
  } else if (c=='>') {
    std::string tmptok(SUTIL_INVOKE(trim(token)));
    token = "";			
    endTag(tmptok);
    changeState(psConsume);
  } else {
    token+=c;
  }				
}

void ParseStateFunc::stateTagHeader(char c)
{
  if (isspace(c)) {
    // drop them
    tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psTagAttributeName);				
  } else {
    token+=c;
  }				
}

void ParseStateFunc::stateCommentConsume(char c) {
  if ((c=='-') && (peekNextChar()=='>')) {
    if (token == "-") {
      nextChar();
      changeState(psConsume);
    }
  } else if (c=='-') {
    token="-";  // Store this in order to track -->
  }
}

void ParseStateFunc::stateAttributeName(char c) {
  if (isspace(c)) return;
  if ((c == '=') && (peekNextChar() == '"')) {
    nextChar(); // consume "
    attrName = token;
    token = "";
    changeState(psTagAttributeValue);
  } else if ((c == '=') && (peekNextChar() == '#')) {
    nextChar(); // consume #
    attrName = token;
    attrValue = "#";
    tagCurrent->addAttribute(attrName,  attrValue);
    token = "";					
  } else if (c=='>') {	// End of tag
    token="";
    changeState(psTagContent);
  } else if ((c=='/') && (peekNextChar()=='>')) {
    nextChar();
    commitTag(tagCurrent);
    endTag(SUTIL_INVOKE(trim(token)));
    token="";
    changeState(psConsume);
  } else if ((c=='?') && (peekNextChar()=='>')) {
    nextChar();
    commitTag(tagCurrent);
    endTag(SUTIL_INVOKE(trim(token)));
    token="";
    changeState(psConsume);          
  } else { 
    token += c;
  }
}

void ParseStateFunc::stateAttributeValue(char c) {
  if (c=='"') {
    attrValue = token;
    tagCurrent->addAttribute(attrName, attrValue);
    changeState(psTagAttributeName);
  } else {
    token +=c;
  }
}

void ParseStateFunc::stateTagContent(char c) {
  if (c == '<') {	// can't use 'peekNext' since we might have >< which is legal
    tagCurrent->setContent(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psConsume);
    rewind();	// rewind so we will see tag start next time
  } else {
    token += c;
  }
}

void ParseStateFunc::parseData() {
  char c;
  tagStack.push(root);
  while((c=nextChar())!=EOF) {
    switch(state) 
    {
    case psConsume:
      stateConsume(c);
      break;
    case psCommentStart : // Make sure we hit '--'
      stateCommentStart(c);
      break;
    case psCommentConsume:  // parse until -->
      stateCommentConsume(c);
      break;
    case psTagHeader : // <? 
      stateTagHeader(c);
      break;
    case psTagStart :	// '<'          
      stateTagStart(c);
      break;
    case psEndTagStart : // </
      stateEndTagStart(c);
      break;
    case psTagAttributeName :
      stateAttributeName(c);
      break;
    case psTagAttributeValue :
      stateAttributeValue(c);
      break;
    case psTagContent:
      stateTagContent(c);
      break;
    }
  }
}

// -- ParseStateClasses
Tag *ParseStateImpl::createTag(std::string name)
{
  return pContext->createTag(name);
}
void ParseStateImpl::endTag(std::string tok)
{
  pContext->endTag(tok);
}
void ParseStateImpl::commitTag(Tag *pTag)
{
  pContext->commitTag(pTag);
}

void ParseStateImpl::rewind()
{
  pContext->rewind();
}
int ParseStateImpl::nextChar()
{
  return pContext->nextChar();
}
int ParseStateImpl::peekNextChar()
{
  return pContext->peekNextChar();
}
void ParseStateImpl::changeState(kParseState newState)
{
  pContext->changeState(newState);
}

void StateConsume::enter() {
  token = "";
}

void StateConsume::consume(char c) {
  if (c=='<') {
    int next = peekNextChar();
    if (next == '/') {		// ? '</' - distinguish between token <  and </
      nextChar(); // consume '/'
      changeState(psEndTagStart);
    } else if (next == '!') {
      // Action tag started <!--
      nextChar();
      token = ""; // Reset token
      changeState(psCommentStart);
    } else if (next == '?') {
      // Header tag started '<?xml
      nextChar();
      token = "";
      changeState(psTagHeader);
    } else {
      changeState(psTagStart);
    }
    token="";
  } else {
    token+=c;
  }
}

void StateCommentStart::enter() {
  token = "";
}

void StateCommentStart::consume(char c) {
  if ((c == '-') && (peekNextChar()=='-')) {
    nextChar();
    changeState(psCommentConsume);
  } else {
#ifdef _DEBUG
    //printf("Warning: Illegal start of tag, expected start of comment ('<!--') but found found '<!-'\n");
#endif
    // TODO: If strict, abort here!
    rewind();   // rewind '-'
    rewind();   // rewind '!'
    changeState(psTagStart);
  }
}

void StateTagStart::enter() {
    token = "";
}

void StateTagStart::consume(char c) {
  if (isspace(c)) {					          
    pContext->tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";          
    changeState(psTagAttributeName);
  } else if (c=='/' && peekNextChar()=='>') {   // catch tags like '<tag/>'
    nextChar(); // consume '>'
    pContext->tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    commitTag(pContext->tagCurrent);
    changeState(psConsume);
  } else if (c=='>') {
    pContext->tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psTagContent);
  } else {
    token+=c;
  }				
}

void StateTagEndStart::enter() {
  token = "";
}

void StateTagEndStart::consume(char c) {
  if (isspace(c)) {
    // drop them
  } else if (c=='>') {
    std::string tmptok(SUTIL_INVOKE(trim(token)));
    token = "";			
    endTag(tmptok);
    changeState(psConsume);
  } else {
    token+=c;
  }				
}

void StateTagHeader::enter() {
  token = "";
}

void StateTagHeader::consume(char c) {
  if (isspace(c)) {
    // drop them
    pContext->tagCurrent = createTag(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psTagAttributeName);				
  } else {
    token+=c;
  }				
}

void StateCommentConsume::enter() {
  token = "";
}

void StateCommentConsume::consume(char c) {
  if ((c=='-') && (peekNextChar()=='>')) {
    if (token == "-") {
      nextChar();
      changeState(psConsume);
    }
  } else if (c=='-') {
    token="-";  // Store this in order to track -->
  }
}

void StateAttributeName::enter() {
  token = "";
}

void StateAttributeName::consume(char c) {
  if (isspace(c)) return;
  if ((c == '=') && (peekNextChar() == '"')) {
    nextChar(); // consume "
    pContext->attrName = token;
    token = "";
    changeState(psTagAttributeValue);
  } else if ((c == '=') && (peekNextChar() == '#')) {
    nextChar(); // consume #
    pContext->attrName = token;
    pContext->attrValue = "#";
    pContext->tagCurrent->addAttribute(pContext->attrName,  pContext->attrValue);
    token = "";					
  } else if (c=='>') {	// End of tag
    token="";
    changeState(psTagContent);
  } else if ((c=='/') && (peekNextChar()=='>')) {
    nextChar();
    commitTag(pContext->tagCurrent);
    endTag(SUTIL_INVOKE(trim(token)));
    token="";
    changeState(psConsume);
  } else if ((c=='?') && (peekNextChar()=='>')) {
    nextChar();
    commitTag(pContext->tagCurrent);
    endTag(SUTIL_INVOKE(trim(token)));
    token="";
    changeState(psConsume);          
  } else { 
    token += c;
  }
}

void StateAttributeValue::enter() {
  token = "";
}

void StateAttributeValue::consume(char c) {
  if (c=='"') {
    pContext->attrValue = token;
    pContext->tagCurrent->addAttribute(pContext->attrName, pContext->attrValue);
    changeState(psTagAttributeName);
  } else {
    token +=c;
  }
}

void StateTagContent::enter() {
  token = "";
}

void StateTagContent::consume(char c) {
  if (c == '<') {	// can't use 'peekNext' since we might have >< which is legal
    pContext->tagCurrent->setContent(SUTIL_INVOKE(trim(token)));
    token = "";
    changeState(psConsume);
    rewind();	// rewind so we will see tag start next time
  } else {
    token += c;
  }
}

ParseStateClasses::ParseStateClasses(std::string _data, IParseEvents *pEventHandler)
{
  stateConsume.pContext = this;
  stateConsume.pContext = this;
  stateTagStart.pContext = this;
  stateCommentStart.pContext = this;
  stateTagEndStart.pContext = this;
  stateTagHeader.pContext = this;
  stateCommentConsume.pContext = this;
  stateAttributeName.pContext = this;
  stateAttributeValue.pContext = this;
  stateTagContent.pContext = this;
  pState = dynamic_cast<IParseState *>(&stateConsume);

  initialize(_data, pEventHandler);
}

void ParseStateClasses::changeState(kParseState newState) {
  if (pState != NULL) pState->leave();
  switch(newState) {
    case psConsume            : pState = dynamic_cast<IParseState *>(&stateConsume); break;
    case psTagStart           : pState = dynamic_cast<IParseState *>(&stateTagStart); break;
    case psCommentStart       : pState = dynamic_cast<IParseState *>(&stateCommentStart); break;
    case psEndTagStart        : pState = dynamic_cast<IParseState *>(&stateTagEndStart); break;
    case psTagHeader          : pState = dynamic_cast<IParseState *>(&stateTagHeader); break;
    case psCommentConsume     : pState = dynamic_cast<IParseState *>(&stateCommentConsume); break;
    case psTagAttributeName   : pState = dynamic_cast<IParseState *>(&stateAttributeName); break;
    case psTagAttributeValue  : pState = dynamic_cast<IParseState *>(&stateAttributeValue); break;
    case psTagContent         : pState = dynamic_cast<IParseState *>(&stateTagContent); break;
    default : pState = NULL;
  }
  // State tracking variable managed by base class
  Parser::changeState(newState);
  if (pState != NULL) pState->enter();
}

void ParseStateClasses::initialize(std::string _data, IParseEvents *pEventHandler)
{
  Parser::initialize(_data, pEventHandler);
}

void ParseStateClasses::parseData()
{
  char c;
  tagStack.push(root);
  while((c=nextChar())!=EOF) {
    if (pState != NULL) {
      pState->consume(c);
    }
  }
}

////////////////////////////////////////////////////////////////////////////
/*
State machine token description.

'addToToken' basically means to accumulate the given data to a token which can be used when a state change occur.
'whiteSpace' is a list of characters which are to be treated as white space


psConsume
  "<", psTagStart,
  "</", psEndTagStart,
  "<!", psCommentStart,
  "<?", psTagHeader,
  addToToken

psTagStart
  "/>", psConsume
  ">", psTagContent
  whiteSpace, psTagAttributeName
  addToToken
  
psEndTagStart
    ">", psConsume
    addToToken

psCommentStart
    "--", psCommentConsume
    error

psTagHeader
    whiteSpace, psTagAttributeName
    addToToken

psCommentConsume
    "->", psConsume IFF token=='-'
    "-", setToToken
           
psTagAttributeName
    whiteSpace, continue
    "=\"", psTagAttributeValue
    "=#", psTagAttributeName
    ">", psTagContent
    "/>", psConsume
    "?>", psConsume
    addToToken
    
    
psTagAttributeValue
    "\"", psTagAttributeName
    addToToken

psTagContent,
    "<", psConsume
    addToToken    
                 
*/
