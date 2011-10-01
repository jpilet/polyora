/*
 * Tools for parsing tracker.ini.
 * Julien Pilet, oct 2003
 */

#include "iniparser.h"

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#define _WIN32_WINNT 0x400
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#else
#include <stdio.h>
#endif

void errorPrint(const char* szMessage, ...) 
{
        va_list arglist;
        char str[256];

        va_start(arglist, szMessage);

        vsprintf(str, szMessage, arglist);
        va_end(arglist);

#ifdef WIN32
		MessageBoxA(NULL, str, "Error", MB_ICONERROR | MB_OK);
#else
        fprintf(stderr, "%s\n", str);
#endif
}

IniParser::IniParser(void *userdata) : in(0), userdata(userdata), col(0), line(1)
{
}

int IniParser::readChar() {
	int c= fgetc(in);
	if (c=='\n') {
		line++;
		col=0;
	}
	col++;
	return c;
}

int IniParser::getChar() {

	int c = readChar();

	if (c=='#') {
		while ((c=readChar()) != '\n') {
			if (feof(in)) 
				break;
		}
	}
	return c;
}

bool IniParser::nextToken() {
	const char *blanks=" \t\r\n";
	const char *endstring = "\"";
	const char *onecharToken = "[]=";
	char blanksAndOnecharToken[32];
	const char *stop;
	int c;
	int count = 0;
	bool inString = false;

	// eat all blanks
	do {
		c= getChar();
	} while (strchr(blanks,c));
	
	// is it a single char token ?
	if (strchr(onecharToken, c)) {
		buffer[0] = c;
		buffer[1] = 0;
		return !feof(in);
	}
	
	// is this a string ?
	if (c=='"') {
		stop = endstring;
		inString = true;
	} else {
		stop = blanksAndOnecharToken;
		sprintf(blanksAndOnecharToken, "%s%s", blanks, onecharToken);
		buffer[count++] = c;
	}
	
	
	while (count < (LOADER_BUFFER_LENGTH - 1)) {
		if (inString)
			c = readChar();
		else
			c = getChar();
		
		if (strchr(stop, c) || c == EOF) {
			if (strchr(onecharToken, c)) {
				if (c == '\n') 
					--line;
				--col;
				ungetc(c, in);
			}
			buffer[count]=0;
			break;
		} else {
			buffer[count++]=c;
		}
	}
	
	buffer[LOADER_BUFFER_LENGTH-1]=0;
	return !feof(in);
}

bool IniParser::parse(const char *filename) {
	this->filename = filename;
	return parse(fopen(filename, "rt"));
}

bool IniParser::wparse(const wchar_t *filename) {
#ifdef WIN32
	int len = int(wcslen(filename))*2;
	this->filename = new char[len];
	if (WideCharToMultiByte(CP_ACP, 0, filename, -1, (LPSTR) this->filename, len, "?", NULL) == 0) {
		DWORD err = GetLastError();
		errorPrint("unable to translate string! err=%d", err);
		delete[] this->filename;
		this->filename =0;
		return false;
	}
	return parse(_wfopen(filename, L"rt"));
#else
	return false;
#endif
}

bool IniParser::parse(FILE *f) {

	in = f;

	if (in==0) {
		errorPrint("%s: %s", filename, strerror( errno ));
		return false;
	}
	
	nextToken();
	while (!feof(in)) {
				
		if (strcmp(buffer, "[") ==0) {
			if (!readNextSection()) {
				printError("failed to read section");
				fclose(in);
				in=0;				
				return false;
			}
		} else {
			printError("expecting '['");
			fclose(in);
			in=0;				
			return false;
		}
	}

	fclose(in);
	in=0;
	return true;
}

void IniParser::printError(const char *err) {
	errorPrint("%s:%d:%d (%s) %s\n",
		filename,
		line,
		col,
		buffer,
		err);
}

bool IniParser::readNextSection() {
	
	if (strcmp(buffer, "[") !=0) return false;

	nextToken();

	char *sectionName = new char[strlen(buffer)+1];
	strcpy(sectionName, buffer);

	nextToken();

	if (strcmp(buffer, "]") != 0) {
		//free(sectionName);
		delete[] sectionName;
		printError("expecting ']'");
		return false;
	}

	for (IniSectionVector::iterator it(sections.begin()); it != sections.end(); ++it) 
	{
		if (strcmp(sectionName, (*it)->name) == 0) {
			delete[] sectionName;
			return (*it)->parse(this);
		}
	}

	printError("unkown section name");
	fprintf(stderr, "%s", sectionName);
	delete[] sectionName;
	return false;
}

bool IniParser::dumpExampleFile(const char *filename) {
	FILE *f = fopen(filename, "wt");

	if (!f) {
		perror(filename);
		return false;
	}
	for (IniSectionVector::iterator it(sections.begin()); it != sections.end(); ++it) 
		(*it)->print(f);

	fclose(f);

	return true;
}
///////////////////////////////////////////////////////////////////////////////

ParamSection::ParamSection() {
	name = "PARAMETERS";
}

bool ParamSection::parse(IniParser *parser) {

	while (parser->nextToken()) {

		if (strcmp(parser->buffer, "[")==0) return true;

		ParamVector::iterator it;
		for (it = params.begin(); it!= params.end(); ++it) {
			if (strcmp(parser->buffer, (*it)->name)==0) {

				parser->nextToken();
				if (strcmp(parser->buffer, "=") != 0) {
					parser->printError("expecting '='");
					return false;
				}

				if (!parser->nextToken()) {
					parser->printError("unexpected end of file.");
					return false;
				}

				if (!(*it)->parse(parser)) return false;
				break;
			}
		}
		if (it == params.end()) {
			parser->printError("warning: unknown parameter");
			parser->nextToken();
			if (strcmp(parser->buffer, "=") != 0) return false;
			parser->nextToken();
		}
	}

	return true;
}

void ParamSection::print(FILE *f) {
	IniSection::print(f);
		ParamVector::iterator it;
		for (it = params.begin(); it!= params.end(); ++it) {
			fprintf(f, "%s = ", (*it)->name);
			(*it)->print(f);
		}
}

bool ParamSection::DoubleParam::parse(IniParser *parser) 
{
	double d;
	if (sscanf(parser->buffer, "%lf", &d) != 1) {
		parser->printError("warning: cannot convert to double.");
		return false;
	}
				
	if ((d<min) || (d>max)) {
		parser->printError("value out of range");
		return false;
	}
	*val = d;
	return true;
}

void ParamSection::DoubleParam::print(FILE *f) {
	fprintf(f, "%lf\n", *val);
}

int ParamSection::DoubleParam::visit(ParamSection::ParamVisitor *v) {
	return v->doDouble(this);
}

bool ParamSection::BoolParam::parse(IniParser *parser) 
{
	if (strcmp(parser->buffer, "true") == 0) {
		*val = true;
		return true;
	} else if (strcmp(parser->buffer, "false") == 0) {
		*val = false;
		return true;
	}

	parser->printError("boolean option should be \"true\" or \"false\".");
	return false;
}

void ParamSection::BoolParam::print(FILE *f) {
	if (*val) 
		fprintf(f,"true\n");
	else
		fprintf(f,"false\n");
}

int ParamSection::BoolParam::visit(ParamSection::ParamVisitor *v) {
	return v->doBool(this);
}

bool ParamSection::StringParam::parse(IniParser *parser) 
{
	if (*val) free(*val);
	*val = strdup(parser->buffer);
	return true;
}

void ParamSection::StringParam::print(FILE *f) {
	fprintf(f,"\"%s\"\n", *val);
}

int ParamSection::StringParam::visit(ParamSection::ParamVisitor *v) {
	return v->doString(this);
}

bool ParamSection::IntParam::parse(IniParser *parser) 
{
	int d;
	if (sscanf(parser->buffer, "%d", &d) != 1) {
		parser->printError("warning: cannot convert to int.");
		return false;
	}
				
	if ((d<min) || (d>max)) {
		parser->printError("value out of range");
		return false;
	}
	*val = d;
	return true;
}

void ParamSection::IntParam::print(FILE *f) {
	fprintf(f, "%d\n", *val);
}

int ParamSection::IntParam::visit(ParamSection::ParamVisitor *v) {
	return v->doInt(this);
}

void ParamSection::addDoubleParam(const char *name, double *val, double def, double min, double max)
{
	DoubleParam *p = new DoubleParam();

	p->name = name;
	p->min = min;
	p->max = max;
	p->val = val;
	*val = def;

	params.push_back(p);
}

void ParamSection::addBoolParam(const char *name, bool *val, bool def)
{
	BoolParam *p = new BoolParam();
	p->name = name;
	p->val = val;
	*val = def;
	params.push_back(p);
}

void ParamSection::addStringParam(const char *name, char **val, const char *def)
{
	StringParam *p = new StringParam();
	p->name = name;
	p->val = val;
	*val = strdup(def);
	params.push_back(p);
}

void ParamSection::addIntParam(const char *name, int *val, int def, int min, int max)
{
	IntParam *p = new IntParam();

	p->name = name;
	p->min = min;
	p->max = max;
	p->val = val;
	*val = def;

	params.push_back(p);
}
