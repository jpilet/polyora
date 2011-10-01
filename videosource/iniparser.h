//! \ingroup videosource
/*!@{*/
#ifndef __INIPARSER_H
#define __INIPARSER_H

#include <vector>
#include <stdio.h>
#include <float.h>
#include <limits.h>

#ifndef DLLEXPORT
#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else 
#define DLLEXPORT
#endif
#endif

class IniParser;

//! Represent a section in a .ini file.
/*! Grammatical, lexical and semantical information about a .ini section. A
 * section in the .ini file begins by
 * [ section_name ]
 */
class IniSection {
public:
	const char *name; //< the name of the section, ie [ name ] in the file.
	
	/*! parse text starting right after the "]" token. The parser object
	 * should be used to read the file in order to remove comments
	 * correctly and to count lines.
	 * Once parsed, this method do something usefull with the data.
	 */
	virtual bool parse(IniParser *parser)=0;

	/*! This method helps IniParser::dumpExampleFile() to do it's job. It
	 * prints an example section.
	 */
	virtual void print(FILE *f) { fprintf(f,"[%s]\n", name); }

	virtual ~IniSection(){}
};

#define LOADER_BUFFER_LENGTH 256

//! Parser object for .ini file.
/*! An IniParser object represents the grammar and lexical structure of a .ini
 * file, in addition to instructions to handle parsed data.
 *
 * The structure of an .ini file is separated into sections (IniSection) having
 * their own structure.
 *
 * Once a section is created, it is registered with addSection. After that,
 * parse() will be able to parse the file according to described structure. The
 * sections are responsible to make use of parsed data.
 *
 * \author julien.pilet@epfl.ch
 */
class IniParser {
public:

	/*! \param the FptTracker object supposed to be initialized with the
	 * .ini file.
	 */
	DLLEXPORT IniParser(void *userdata);

	/*! Parse and process a .ini file.
	 * \return true on success false on error.
	 */
	DLLEXPORT bool parse(const char *filename);
	DLLEXPORT bool wparse(const wchar_t *filename);
	DLLEXPORT bool parse(FILE *f);

	/*! Lexical analyser : get a character, removing comments and counting
	 * lines.
	 */
	int getChar();

	/*! Get a token. Example: singlewordtoken, "a token with spaces"
	 * Special one character tokens: = [ ]
	 * The token is stored in IniParser::buffer.
	 */
	bool nextToken();

	//! registers a section.
	DLLEXPORT void addSection(IniSection *section) { sections.push_back(section); }

	//! the .ini file. Should be accessed through nextToken() and buffer.
	FILE *in;

	typedef std::vector<IniSection *> IniSectionVector;

	IniSectionVector sections;

	//! Contains the last token parsed by nextToken().
	char buffer[LOADER_BUFFER_LENGTH];

	//! the .ini file name.
	const char *filename;

	/*! print an error, prefixing with the current .ini file name and line and
	 * column counters.
	 */
	void printError(const char *err);

	/*! this pointer is here only to be passed to the IniSection objects.
	 * The IniParser class does not touch it.
	 */
	void *userdata;

	bool readNextSection();

	/*! write an example .ini file to disk.
	 */
	DLLEXPORT bool dumpExampleFile(const char *filename);

private:
	int readChar();

	int col, line;
};


/*! The [ PARAMETERS ] section basically contains a collection of 
 * "keyword = values" parameters.
 * 
 * 4 types of parameters exists: int, double, boolean and string.
 */
class ParamSection : public IniSection {
public:
	DLLEXPORT ParamSection();
	virtual bool parse(IniParser *parser);
	virtual void print(FILE *f);

	/*! register a parameter of type "double".
	 * \param name the keyword name will appear as "keyword = value" in the .ini file.
	 * \param val the pointer where to save the parsed value.
	 * \param def the default value if this parameter is ommited in the .ini file.
	 * \param min the minimum acceptable value.
	 * \param max the maximum acceptable value.
	 */
	DLLEXPORT void addDoubleParam(const char *name, double *val, double def, double min=-DBL_MAX, double max=DBL_MAX);
	
	/*! register a parameter of type "bool". Example:
	 * param = true
	 * param = false
	 * \param name the keyword name will appear as "keyword = value" in the .ini file.
	 * \param val the pointer where to save the parsed value.
	 * \param def the default value if this parameter is ommited in the .ini file.
	 */
	DLLEXPORT void addBoolParam(const char *name, bool *val, bool def);

	/*! register a parameter of type "string". Example:
	 * param = "c:/directory/file.avi"
	 * param = singleword
	 * \param name the keyword name will appear as "keyword = value" in the .ini file.
	 * \param val the pointer where to save the parsed value. Memory will
	 * be allocated and should be freed by the caller.
	 * \param def the default value if this parameter is ommited in the .ini file.
	 */
	DLLEXPORT void addStringParam(const char *name, char **val, const char *def);

	/*! register a parameter of type "int".
	 * \param name the keyword name will appear as "keyword = value" in the .ini file.
	 * \param val the pointer where to save the parsed value.
	 * \param def the default value if this parameter is ommited in the .ini file.
	 * \param min the minimum acceptable value.
	 * \param max the maximum acceptable value.
	 */
	DLLEXPORT void addIntParam(const char *name, int *val, int def, int min = INT_MIN, int max = INT_MAX);
	
	struct ParamVisitor;
		
	struct Param {
		const char *name;
		virtual ~Param() {}
		virtual bool parse(IniParser *parser)=0;
		virtual void print(FILE *f)=0;
		virtual int visit(ParamVisitor *v)=0;
	};
	
	struct DoubleParam : public Param {
		double min,max,*val;
		virtual bool parse(IniParser *parser);
		virtual void print(FILE *f);
		virtual int visit(ParamVisitor *v);
	};
	struct BoolParam : public Param {
		bool *val;
		virtual bool parse(IniParser *parser);
		virtual void print(FILE *f);
		virtual int visit(ParamVisitor *v);
	};
	struct StringParam : public Param {
		char **val;
		virtual bool parse(IniParser *parser);
		virtual void print(FILE *f);
		virtual int visit(ParamVisitor *v);
	};
	struct IntParam : public Param {
		int min, max, *val;
		virtual bool parse(IniParser *parser);
		virtual void print(FILE *f);
		virtual int visit(ParamVisitor *v);
	};

	struct ParamVisitor {
		virtual ~ParamVisitor(){}
		virtual int doDouble(DoubleParam *p)=0;
		virtual int doBool(BoolParam *p)=0;
		virtual int doString(StringParam *p)=0;
		virtual int doInt(IntParam *p)=0;
	};

	typedef std::vector<Param *> ParamVector;
	ParamVector params; 
};

#endif
/*!@}*/
