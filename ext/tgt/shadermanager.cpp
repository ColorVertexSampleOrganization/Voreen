/**********************************************************************
 *                                                                    *
 * tgt - Tiny Graphics Toolbox                                        *
 *                                                                    *
 * Copyright (C) 2006-2008 Visualization and Computer Graphics Group, *
 * Department of Computer Science, University of Muenster, Germany.   *
 * <http://viscg.uni-muenster.de>                                     *
 *                                                                    *
 * This file is part of the tgt library. This library is free         *
 * software; you can redistribute it and/or modify it under the terms *
 * of the GNU Lesser General Public License version 2.1 as published  *
 * by the Free Software Foundation.                                   *
 *                                                                    *
 * This library is distributed in the hope that it will be useful,    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the       *
 * GNU Lesser General Public License for more details.                *
 *                                                                    *
 * You should have received a copy of the GNU Lesser General Public   *
 * License in the file "LICENSE.txt" along with this library.         *
 * If not, see <http://www.gnu.org/licenses/>.                        *
 *                                                                    *
 **********************************************************************/

#include "tgt/shadermanager.h"

#include <string.h>

#include "tgt/gpucapabilities.h"
#include "tgt/texturemanager.h"

using std::string;

namespace tgt {

const string ShaderObject::loggerCat_("tgt.Shader.ShaderObject");

ShaderObject::ShaderObject(const string& filename, ShaderType type)
    : filename_(filename),
      shaderType_(type),
      inputType_(GL_TRIANGLES),
	  outputType_(GL_TRIANGLE_STRIP),
	  verticesOut_(16)
{
	LGL_ERROR;
    id_ = glCreateShader(shaderType_);
    if (id_ == 0)
        LERROR("ShaderObject::ShaderObject(" + filename + ")::glCreateShader() returned 0");
    isCompiled_ = false;
	LGL_ERROR;
}

ShaderObject::~ShaderObject() {
    if (isCompiled_)
        glDeleteShader(id_);
}

bool ShaderObject::loadSourceFromFile(const string &filename) {
	LDEBUG("Loading " << filename);
    File* file = FileSys.open(filename);

    // check if file is open
	if (!file || !file->open()) {
		LERROR("File not found.");
        return false;
	}

    source_ = file->getAsString();

    file->close();
    delete file;

    return true;
}

void ShaderObject::uploadSource() {
    string completeSrc = replaceIncludes(header_ + source_);
    const GLchar* tmpShaderSource = completeSrc.c_str();
    glShaderSource(id_, 1, &tmpShaderSource, 0);
}

string ShaderObject::replaceIncludes(const string& completeSource) {
	int numReplaced = 0;

    string sourceStr = completeSource;

    string::size_type curPos = sourceStr.find("#include", 0);
    int replaceStart = 0;
    int replaceEnd = 0;

    while (curPos != string::npos) {
        replaceStart = curPos;
        curPos = sourceStr.find("\"", curPos+1);
        replaceEnd = sourceStr.find("\"", curPos+1);
        string fileName(sourceStr, curPos + 1, replaceEnd - curPos - 1);

        fileName = ShdrMgr.completePath(fileName);

        File* file = FileSys.open(fileName);
        string content;
        if (file && file->open()) {
            content = file->getAsString();
            file->close();

            content = "// BEGIN INCLUDE " + fileName + "\n#line 1\n" + content;
            if (content[content.size() - 1] != '\n')
                content += "\n";
            content += "// END INCLUDE " + fileName + "\n#line 1\n";
        }
        delete file;
        sourceStr.replace(replaceStart, replaceEnd - replaceStart + 1, content);
        numReplaced++;
        curPos = replaceEnd;
        curPos = sourceStr.find("#include", curPos + 1);
    }

    if (numReplaced > 0) {
      // write the complete Shader into a file for debugging the shader
//#define TGT_SHADER_INCLUDE_DEBUG
#ifdef TGT_SHADER_INCLUDE_DEBUG
        string basename;
        string::size_type first_pos = filename_.find_last_of("/");
        if (first_pos != string::npos)
            basename = filename_.substr(first_pos + 1);
        else
            basename = filename_;

		std::ofstream out(basename.c_str()); // output file

        if (!out) {
            LERROR("Cannot open output file: " << basename.c_str() << " (VRN_SHADER_INCLUDE_DEBUG)");
        } else {
			out << sourceStr.c_str() << std::endl;
            out.close();
        }
#endif

    }
    return sourceStr;
}

bool ShaderObject::scanDirectives() {
	LDEBUG("Scanning for geometry shader compile directives...");
	string input = getDirective("GL_GEOMETRY_INPUT_TYPE_EXT");
	if (input == "GL_POINTS")
		inputType_ = GL_POINTS;
	else if (input == "GL_LINES")
		inputType_ = GL_LINES;
	else if (input == "GL_LINES_ADJACENCY_EXT")
		inputType_ = GL_LINES_ADJACENCY_EXT;
	else if (input == "GL_TRIANGLES")
		inputType_ = GL_TRIANGLES;
	else if (input == "GL_TRIANGLES_ADJACENCY_EXT")
		inputType_ = GL_TRIANGLES_ADJACENCY_EXT;
	else {
		LERROR("Unknown input type: " << input);
        return false;
	};

	string output = getDirective("GL_GEOMETRY_OUTPUT_TYPE_EXT");
	if (output == "GL_POINTS")
		outputType_ = GL_POINTS;
	else if (output == "GL_LINE_STRIP")
		outputType_ = GL_LINE_STRIP;
	else if (output == "GL_TRIANGLE_STRIP")
		outputType_ = GL_TRIANGLE_STRIP;
	else {
		LERROR("Unknown output type: " << output);
		return false;
	};

	string verticesOut = getDirective("GL_GEOMETRY_VERTICES_OUT_EXT");

	std::istringstream myStream(verticesOut);
	if (myStream >> verticesOut_) {
		LDEBUG("VERTICES_OUT: " << verticesOut_);
	}
	else {
		LERROR("Failed to parse argument(" << verticesOut << ") as integer for directive GL_GEOMETRY_VERTICES_OUT_EXT.");
		return false;
	}

	return true;
}

void ShaderObject::setDirectives(GLuint id) {
	glProgramParameteriEXT(id, GL_GEOMETRY_INPUT_TYPE_EXT, inputType_);
	LGL_ERROR;
	glProgramParameteriEXT(id, GL_GEOMETRY_OUTPUT_TYPE_EXT, outputType_);
	LGL_ERROR;
	glProgramParameteriEXT(id, GL_GEOMETRY_VERTICES_OUT_EXT, verticesOut_);
	LGL_ERROR;
}

string ShaderObject::getDirective(const string& directive) {
	string sourceStr = source_;
    string::size_type curPos = sourceStr.find(directive + "(", 0);
	string::size_type length = directive.length() + 1;
	if (curPos != string::npos) {
		string::size_type endPos = sourceStr.find(")", curPos);

		if (endPos != string::npos) {
			string ret = sourceStr.substr(curPos + length, (endPos - curPos - length));
			// test for space, newline
			if ((ret.find(" ", 0) == string::npos) && (ret.find("\n", 0) == string::npos)) {
				LINFO("Directive " << directive << ": " << ret);
				return ret;
			}
			else {
				LERROR("No spaces/newlines allowed inbetween directive brackets! Directive: " << directive);
				return "";
			}
		}
		LERROR("Missing ending bracket for directive " << directive);
		return "";
	}
	else {
		LWARNING("Could not locate directive " << directive << "!");
		return "";
	}
}

bool ShaderObject::compileShader() {
    isCompiled_ = false;
    glCompileShader(id_);
    GLint check = 0;
    glGetShaderiv(id_, GL_COMPILE_STATUS, &check);
    if (check)
        isCompiled_ = true;
    return true;
}

string ShaderObject::getCompilerLog() {
    GLint len;
    glGetShaderiv(id_, GL_INFO_LOG_LENGTH , &len);

    if (len > 1) {
        GLchar* log = new GLchar[len];
        if (len == 0)
            return "Memory allocation for log failed!";
        GLsizei l;  // length returned
        glGetShaderInfoLog(id_, len, &l, log);
        string retStr(log);
        delete[] log;
        return retStr;
    }
    return "";
}

void ShaderObject::setHeader(const string& h) {
    header_ = h;
}

void ShaderObject::generateHeader(const string& defines) {
    string out = "// START OF PROGRAM GENERATED DEFINES";

    string separator = " ";
    int oldPos=0, pos=0;
    string add = "";

    while ((pos = defines.find_first_of(separator, oldPos)) != -1 ) {
        add = defines.substr(oldPos,pos-oldPos);
        if (add != "")
            out += "\n#define " + add;
        oldPos = pos + 1;
    }

    add = defines.substr(oldPos);
    if (add != "")
        out += "\n#define " + add;

    out += "\n// END OF PROGRAM GENERATED DEFINES\n#line 1\n";
    setHeader(out);
}

bool ShaderObject::rebuildFromFile() {
    tgtAssert(!filename_.empty(), "the filename must be set");

    if (!loadSourceFromFile(filename_)) {
        LWARNING("Failed to load vertexshader " << filename_);
        return false;
    }
    else {
        uploadSource();

        if (!compileShader()) {
            LERROR("Failed to compile vertexshader " << filename_);
            LERROR("Compiler Log: \n" << getCompilerLog());
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------

const string Shader::loggerCat_("tgt.Shader.Shader");

Shader::Shader() {
    id_ = glCreateProgram();
    if (id_ == 0)
        LERROR("Shader::Shader():: glCreateProgram() returned 0");
    isLinked_ = false;
}

Shader::~Shader() {
    for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
        glDetachShader(id_, (*iter)->id_);
        delete (*iter);
    }
    objects_.clear();
    glDeleteShader(id_);
}

void Shader::attachObject(ShaderObject* obj) {
    glAttachShader(id_, obj->id_);
    objects_.push_back(obj);
}

void Shader::detachObject(ShaderObject* obj) {
    // Maybe first check if obj is in objects_?
    glDetachShader(id_, obj->id_);
    objects_.remove(obj);
    isLinked_ = false;
}

void Shader::activate() {
    if (isLinked_)
        glUseProgram(id_);
}

bool Shader::isActivated() {
    GLint shader_nr;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shader_nr);
    return (id_ == static_cast<GLuint>(shader_nr));
}

GLint Shader::getCurrentProgram() {
    GLint id;
    glGetIntegerv(GL_CURRENT_PROGRAM, &id);
    return id;
}

void Shader::detachObjectsByType(ShaderType type) {
    for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
        if ((*iter)->getType() == type)
            detachObject(*iter);
        delete (*iter);
    }
    isLinked_ = false;
}

bool Shader::linkProgram() {
    if (isLinked_) {
        // program is already linked: detach and re-attach everything
        for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
            glDetachShader(id_, (*iter)->id_);
            glAttachShader(id_, (*iter)->id_);
			if ((*iter)->getType() == GEOMETRY_SHADER)
				(*iter)->setDirectives(id_);
        }
    }
	else {
		for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
			if ((*iter)->getType() == GEOMETRY_SHADER)
					(*iter)->setDirectives(id_);
		}
	}

    isLinked_ = false;
    glLinkProgram(id_);
    GLint check = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &check);
    if (check)
        isLinked_ = true;

    return isLinked_;
}

string Shader::getLinkerLog() {
    GLint len;
    glGetProgramiv(id_, GL_INFO_LOG_LENGTH , &len);

    if (len > 1) {
        GLchar* log = new GLchar[len];

        if (len == 0)
            return "Memory allocation for log failed!";

        GLsizei l;  // length returned
        glGetProgramInfoLog(id_, len, &l, log);
        string retStr(log);
        delete[] log;

        return retStr;
    }

    return "";
}

bool Shader::rebuild() {
    if (isLinked_) {
        // program is already linked: detach and re-attach everything
        for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
            glDetachShader(id_, (*iter)->id_);
            (*iter)->uploadSource();

            if (!(*iter)->compileShader())
                return false;

            glAttachShader(id_, (*iter)->id_);
        }
    }
    isLinked_ = false;
    glLinkProgram(id_);
    GLint check = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &check);

    if (check) {
        isLinked_ = true;
        return true;
    } else {
        LERROR("Shader::rebuild(): Failed to link shader." );
        LERROR("Linker Log: \n" << getLinkerLog());
        return false;
    }
}

bool Shader::rebuildFromFile() {
    bool result = true;

    for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter)
        result &= (*iter)->rebuildFromFile();

    result &= rebuild();

    return result;
}

void Shader::setHeaders(const string& customHeader, bool processHeader) {
    for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter) {
        if (processHeader)
            (*iter)->generateHeader(customHeader);
        else
            (*iter)->setHeader(customHeader);
    }
}

bool Shader::load(const string& filename, const string& customHeader, bool processHeader) {
    return loadSeparate(filename + ".vert", filename + ".frag", customHeader, processHeader);
}

bool Shader::loadSeparate(const string& vert_filename, const string& frag_filename,
                          const string& customHeader, bool processHeader, const string& geom_filename)
{
    tgt::ShaderObject* frag = 0;
    tgt::ShaderObject* vert = 0;
	tgt::ShaderObject* geom = 0;

    if (!vert_filename.empty()) {
        vert = new tgt::ShaderObject(vert_filename, VERTEX_SHADER);

        if (customHeader != "") {
            if (processHeader)
                vert->generateHeader(customHeader);
            else
                vert->setHeader(customHeader);
        }

        if (!vert->loadSourceFromFile(vert_filename)) {
            LWARNING("Failed to load vertexshader " << vert_filename);
            delete vert;
            vert = 0;
        }
        else {
            vert->uploadSource();

            if (!vert->compileShader()) {
                LERROR("Failed to compile vertexshader " << vert_filename);
                LERROR("Compiler Log: \n" << vert->getCompilerLog());
                delete vert;
                return false;
            }
        }
    }

	if (!geom_filename.empty()) {
		geom = new tgt::ShaderObject(geom_filename, tgt::GEOMETRY_SHADER);

        if (customHeader != "") {
            if (processHeader)
                geom->generateHeader(customHeader);
            else
                geom->setHeader(customHeader);
        }

        if (!geom->loadSourceFromFile(geom_filename)) {
            LWARNING("Failed to load geometryshader " << geom_filename);
            delete geom;
            geom = 0;
        }
        else {
            geom->uploadSource();

			geom->scanDirectives();

            if (!geom->compileShader()) {
                LERROR("Failed to compile geometryshader " << geom_filename);
                LERROR("Compiler Log: \n" << geom->getCompilerLog());
                delete geom;
                return false;
            }
        }
    }

    if (!frag_filename.empty()) {
        frag = new tgt::ShaderObject(frag_filename, FRAGMENT_SHADER);

        if (customHeader != "") {
            if (processHeader)
                frag->generateHeader(customHeader);
            else
                frag->setHeader(customHeader);
        }

        if (!frag->loadSourceFromFile(frag_filename)) {
            LWARNING("Failed to load fragmentshader " << frag_filename);
            delete frag;
            frag = 0;
        }
        else {
			frag->uploadSource();

            if (!frag->compileShader()) {
                LERROR("Failed to compile fragmentshader " << frag_filename);
                LERROR("Compiler Log: \n" << frag->getCompilerLog());
                delete frag;
                return false;
            }
        }
    }

    if (frag)
        attachObject(frag);
    if (vert)
        attachObject(vert);
	if (geom)
        attachObject(geom);

    if (!linkProgram()) {
        LERROR("Failed to link shader (" << vert_filename << ","  << frag_filename << "," << geom_filename << ")\n");
        if (vert) {
            LERROR(vert->filename_ << " Vertex shader compiler log: \n" << vert->getCompilerLog());
            detachObject(vert);
            delete vert;
        }
        //FIXME: why is this commented out?
		/*
		if (geom) {
            LERROR(geom->filename_ << " Geometry shader compiler log: \n" << geom->getCompilerLog());
            detachObject(geom);
            delete geom;
        }
		if (frag) {
            LERROR(frag->filename_ << " Fragment shader compiler log: \n" << frag->getCompilerLog());
            detachObject(frag);
            delete frag;
        }
		*/
        LERROR("Linker Log: \n" << getLinkerLog());
        return false;
    }


    if (vert && vert->getCompilerLog().size() > 1) {
        LDEBUG("Vertex shader compiler log for file '" << vert_filename
             << "': \n" << vert->getCompilerLog());
    }
	if (geom && geom->getCompilerLog().size() > 1) {
        LDEBUG("Geometry shader compiler log for file '" << geom_filename
             << "': \n" << geom->getCompilerLog());
    }
	if (frag && frag->getCompilerLog().size() > 1) {
        LDEBUG("Fragment shader compiler log for file '" << frag_filename
             << "': \n" << frag->getCompilerLog());
    }

    if (getLinkerLog().size() > 1) {
        LDEBUG("Linker log for '" << vert_filename << "' and '"
             << frag_filename << "' and '"
             << geom_filename << "': \n" << getLinkerLog());
    }

    return true;
}

//FIXME: this returns header AND source, rename?
string Shader::getSource(int i) {
    int j = 0;

    for (ShaderObjects::iterator iter = objects_.begin(); iter != objects_.end(); ++iter, ++j) {
        if (i == j)
            return (*iter)->header_ + (*iter)->source_;
    }

    // else
    return "";
}

GLint Shader::getUniformLocation(const string &name, bool ignoreError) {
    GLint l;
    l = glGetUniformLocation(id_, name.c_str());
    if (l == -1 && !ignoreError)
        LERROR("Failed to locate uniform Location: " << name);
    return l;
}

// Floats
bool Shader::setUniform(const string &name, GLfloat value) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1f(l, value);
    return true;
}

bool Shader::setUniform(const string &name, GLfloat v1, GLfloat v2) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2f(l, v1, v2);
    return true;
}

bool Shader::setUniform(const string &name, GLfloat v1, GLfloat v2, GLfloat v3) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3f(l, v1, v2, v3);
    return true;
}

bool Shader::setUniform(const string &name, GLfloat v1, GLfloat v2, GLfloat v3, GLfloat v4) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4f(l, v1, v2, v3, v4);
    return true;
}

bool Shader::setUniform(const string &name, int count, GLfloat* v) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1fv(l, count, v);
    return true;
}

// Integers
bool Shader::setUniform(const string &name, GLint value) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1i(l, value);
    return true;
}

bool Shader::setUniform(const string &name, GLint v1, GLint v2) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2i(l, v1, v2);
    return true;
}

bool Shader::setUniform(const string &name, GLint v1, GLint v2, GLint v3) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3i(l, v1, v2, v3);
    return true;
}

bool Shader::setUniform(const string &name, GLint v1, GLint v2, GLint v3, GLint v4) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4i(l, v1, v2, v3, v4);
    return true;
}

bool Shader::setUniform(const string &name, int count, GLint* v) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1iv(l, count, v);
    return true;
}


#ifdef __APPLE__

// Glew (1.4.0) defines 'GLint' as 'long' on Apple, so these wrappers are necessary.
// On all other platforms 'GLint' is defined as 'int' instead.

bool Shader::setUniform(const string &name, int value) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1i(l, static_cast<GLint>(value));
    return true;
}

bool Shader::setUniform(const string &name, int v1, int v2) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2i(l, static_cast<GLint>(v1), static_cast<GLint>(v2));
    return true;
}

bool Shader::setUniform(const string &name, int v1, int v2, int v3) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3i(l, static_cast<GLint>(v1), static_cast<GLint>(v2), static_cast<GLint>(v3));
    return true;
}

bool Shader::setUniform(const string &name, int v1, int v2, int v3, int v4) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4i(l, static_cast<GLint>(v1), static_cast<GLint>(v2), static_cast<GLint>(v3), static_cast<GLint>(v4));
    return true;
}

bool Shader::setUniform(const string &name, int count, int* v) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLint* vector = new GLint[count];
    for (int i=0; i<count; i++)
        vector[i] = static_cast<GLint>( v[i] );
    glUniform1iv(l, count, vector);
    delete vector;
    return true;
}

#endif // __APPLE__

bool Shader::setUniform(const string &name, bool value) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform1i(l, static_cast<GLint>(value));
    return true;
}

bool Shader::setUniform(const string &name, bool v1, bool v2) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2i(l, static_cast<GLint>(v1), static_cast<GLint>(v2));
    return true;
}

bool Shader::setUniform(const string &name, bool v1, bool v2, bool v3) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3i(l, static_cast<GLint>(v1), static_cast<GLint>(v2), static_cast<GLint>(v3));
    return true;
}

bool Shader::setUniform(const string &name, bool v1, bool v2, bool v3, bool v4) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4i(l, static_cast<GLint>(v1), static_cast<GLint>(v2), static_cast<GLint>(v3), static_cast<GLint>(v4));
    return true;
}

bool Shader::setUniform(const string &name, int count, GLboolean* v) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLint* vector = new GLint[count];
    for (int i=0; i<count; i++)
        vector[i] = static_cast<GLint>( v[i] );
    glUniform1iv(l, count, vector);
    delete vector;
    return true;
}


// Vectors
bool Shader::setUniform(const string &name, Vector2f value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2fv(l, count, value.elem);
    return true;
}

bool Shader::setUniform(const string &name, Vector2f* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLfloat* values = new GLfloat[2*count];
    for (int i=0; i<count; i++){
        values[2*i] = vectors[i].x;
        values[2*i+1] = vectors[i].y;
    }
    glUniform2fv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, Vector3f value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3fv(l, count, value.elem);
    return true;
}

bool Shader::setUniform(const string &name, Vector3f* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLfloat* values = new GLfloat[3*count];
    for (int i=0; i<count; i++) {
        values[3*i] = vectors[i].x;
        values[3*i+1] = vectors[i].y;
        values[3*i+2] = vectors[i].z;
    }
    glUniform3fv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, Vector4f value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4fv(l, count, value.elem);
    return true;
}

bool Shader::setUniform(const string &name, Vector4f* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLfloat* values = new GLfloat[4*count];
    for (int i=0; i<count; i++) {
        values[4*i] = vectors[i].x;
        values[4*i+1] = vectors[i].y;
        values[4*i+2] = vectors[i].z;
        values[4*i+3] = vectors[i].a;
    }
    glUniform4fv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, int count, Vector4f* vectors) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLfloat* values = new GLfloat[4*count];
    for (int i=0; i<count; i++) {
        values[4*i] = vectors[i].x;
        values[4*i+1] = vectors[i].y;
        values[4*i+2] = vectors[i].z;
        values[4*i+3] = vectors[i].a;
    }
    glUniform4fv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, ivec2 value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform2iv(l, count, static_cast<GLint *>(value.elem));
    return true;
}

bool Shader::setUniform(const string &name, ivec2* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLint* values = new GLint[2*count];
    for (int i=0; i<count; i++) {
        values[2*i] = vectors[i].x;
        values[2*i+1] = vectors[i].y;
    }
    glUniform2iv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, ivec3 value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform3iv(l, count,  static_cast<GLint *>(value.elem));
    return true;
}

bool Shader::setUniform(const string &name, ivec3* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLint* values = new GLint[3*count];
    for (int i=0; i<count; i++) {
        values[3*i] = vectors[i].x;
        values[3*i+1] = vectors[i].y;
        values[3*i+2] = vectors[i].z;
    }
    glUniform3iv(l, count, values);
    return true;
}

bool Shader::setUniform(const string &name, ivec4 value, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniform4iv(l, count,  (GLint *) value.elem);
    return true;
}

bool Shader::setUniform(const string &name, ivec4* vectors, GLsizei count) {
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    GLint* values = new GLint[4*count];
    for (int i=0; i<count; i++) {
        values[4*i] = vectors[i].x;
        values[4*i+1] = vectors[i].y;
        values[4*i+2] = vectors[i].z;
        values[4*i+3] = vectors[i].a;
    }
    glUniform4iv(l, count, values);
    return true;
}

// Note: Matrix is transposed by OpenGL
bool Shader::setUniform(const string &name, const Matrix2f &value, bool transpose,
                        GLsizei count)
{
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniformMatrix2fv(l, count, !transpose, value.elem);
    return true;
}

bool Shader::setUniform(const string &name, const Matrix3f &value, bool transpose,
                        GLsizei count)
{
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniformMatrix3fv(l, count, !transpose, value.elem);
    return true;
}

bool Shader::setUniform(const string &name, const Matrix4f &value, bool transpose,
                        GLsizei count)
{
    GLint l = getUniformLocation(name);
    if (l == -1)
        return false;
    glUniformMatrix4fv(l, count, !transpose, value.elem);
    return true;
}

GLint Shader::getAttributeLocation(const string &name) {
    GLint l;
    l = glGetAttribLocation(id_, name.c_str());
    if (l == -1)
    {
        LERROR("Failed to locate attribute Location: " << name);
    }
    return l;
}

//------------------------------------------------------------------------------

const string ShaderManager::loggerCat_("tgt.Shader.Manager");

ShaderManager::ShaderManager(bool cache)
  : ResourceManager<Shader>(cache)
{}

Shader* ShaderManager::load(const string& filename, const string& customHeader,
                            bool processHeader, bool activate)
{
    return loadSeparate(filename + ".vert", filename + ".frag", customHeader, processHeader, activate);
}

Shader* ShaderManager::loadSeparate(const string& vert_filename, const string& frag_filename,
                                    const string& customHeader, bool processHeader,
                                    bool activate)
{
    return loadSeparate(vert_filename, "", frag_filename, customHeader, processHeader, activate);
}

Shader* ShaderManager::loadSeparate(const string& vert_filename, const string& geom_filename,
                                    const string& frag_filename,
                                    const string& customHeader, bool processHeader,
                                    bool activate)
{
	LDEBUG("Loading files " << vert_filename << " and " << frag_filename);
    if (!GpuCaps.areShadersSupported()) {
		LERROR("Shaders are not supported.");
        return 0;
	}
    if (isLoaded(vert_filename + frag_filename)) { 
        //FIXME: ugly hack - is it really that ugly? the resulting identifier is unique.
        LDEBUG("Shader already loaded. Increase usage count.");
        increaseUsage(vert_filename+frag_filename);

        return get(vert_filename+frag_filename);
    }

    tgt::Shader* shdr = new tgt::Shader();

    // searching in all paths for every shader
    string vert_completeFilename;
    if (!vert_filename.empty())
        vert_completeFilename = completePath(vert_filename);

    string geom_completeFilename;
    if (!geom_filename.empty())
        geom_completeFilename = completePath(geom_filename);

    string frag_completeFilename;
    if (!frag_filename.empty())
        frag_completeFilename = completePath(frag_filename);

    // loading and linking found shaders
    if (shdr->loadSeparate(vert_completeFilename, frag_completeFilename,
                           customHeader, processHeader, geom_completeFilename))
    {
        reg(shdr, vert_filename+frag_filename+geom_filename);
        if (activate)
            shdr->activate();
        return shdr;
    }
    delete shdr;
    return 0;
}

bool ShaderManager::rebuildAllShadersFromFile() {
    bool result = true;

    for (std::map<Shader*, ResourceManager<Shader>::Resource*>::iterator iter = resourcesByPtr_.begin();
         iter != resourcesByPtr_.end(); ++iter)
    {
        result &= iter->first->rebuildFromFile();
    }

    return result;
}

} // namespace