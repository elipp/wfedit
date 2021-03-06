#include "glext_loader.h"
#include "shader.h"
#include "lin_alg.h"

#define PRINT(x, ...) do { printf(x, __VA_ARGS__); } while(0)

static char logbuffer[1024];

#define RED_BOLD "\033[1;31m"
#define COLOR_RESET "\033[0m"

static size_t cpp_getfilesize(std::ifstream& in) {
	in.seekg(0, std::ios::end);
	long length = in.tellg();
	in.seekg(0, std::ios::beg);

	return length;
}

ShaderProgram::ShaderProgram(const std::string &name_base, const std::unordered_map<GLuint,std::string> &bindattrib_loc_names_map) { 	

	for (int i = 0; i < 5; i++) shaderObjIDs[i] = SHADER_NONE;	

	bad = false;

	id_string = name_base;
	shader_filenames[VertexShader] = name_base + "/vs";
	shader_filenames[TessellationControlShader] = name_base + "/tcs";
	shader_filenames[TessellationEvaluationShader] = name_base + "/tes";
	shader_filenames[GeometryShader] = name_base + "/gs";
	shader_filenames[FragmentShader] = name_base + "/fs";

	GLsizei vs_len, tcs_len, tes_len, gs_len, fs_len;

	char *vs_buf = NULL, 
	     *tcs_buf = NULL,
	     *tes_buf = NULL,
	     *gs_buf = NULL, 
	     *fs_buf = NULL;

	
	// note: program will leak memory on error, but then again these kinds of errors are considered "fatal"

	// read applicable shader source files into buffers, run glCreateShader

	vs_buf = ShaderProgram::readShaderFromFile(shader_filenames[VertexShader], &vs_len);
	if (!vs_buf) { set_bad(); goto cleanup; }	// vertex shader is mandatory
	shaderObjIDs[VertexShader] = glCreateShader(GL_VERTEX_SHADER);

	tcs_buf = ShaderProgram::readShaderFromFile(shader_filenames[TessellationControlShader], &tcs_len);
	if (tcs_buf) {
		shaderObjIDs[TessellationControlShader] = glCreateShader(GL_TESS_CONTROL_SHADER);

		tes_buf = ShaderProgram::readShaderFromFile(shader_filenames[TessellationEvaluationShader], &tes_len);
		if (tes_buf) { 
			shaderObjIDs[TessellationEvaluationShader] = glCreateShader(GL_TESS_EVALUATION_SHADER);
		}	
		else {
			PRINT("ShaderProgram error: %s: TessellationControlShader enabled but no TessellationEvaluationShader provided.\n", name_base.c_str());
			set_bad(); goto cleanup;
		}
	}

	gs_buf = ShaderProgram::readShaderFromFile(shader_filenames[GeometryShader], &gs_len);
	if (gs_buf) {
		shaderObjIDs[GeometryShader] = glCreateShader(GL_GEOMETRY_SHADER);
	}

	fs_buf = ShaderProgram::readShaderFromFile(shader_filenames[FragmentShader], &fs_len);
	if (!fs_buf) { set_bad(); goto cleanup; } // fragment shader is mandatory
	shaderObjIDs[FragmentShader] = glCreateShader(GL_FRAGMENT_SHADER);

	// shader sources
	glShaderSource(shaderObjIDs[VertexShader], 1, (const GLchar**)&vs_buf,  (const GLint*)&vs_len);

	if (shaderObjIDs[TessellationControlShader] != SHADER_NONE) {
		glShaderSource(shaderObjIDs[TessellationControlShader], 1, (const GLchar**)&tcs_buf, (const GLint*)&tcs_len);
	}
	if (shaderObjIDs[TessellationEvaluationShader] != SHADER_NONE) {
		glShaderSource(shaderObjIDs[TessellationEvaluationShader], 1, (const GLchar**)&tes_buf, (const GLint*)&tes_len);
	}
	
	if (shaderObjIDs[GeometryShader] != SHADER_NONE) {
		glShaderSource(shaderObjIDs[GeometryShader], 1, (const GLchar**)&gs_buf, (const GLint*)&gs_len);
	}
	glShaderSource(shaderObjIDs[FragmentShader], 1, (const GLchar**)&fs_buf, (const GLint*)&fs_len);

	// compile everything
	for (int i = VertexShader; i <= FragmentShader; i++) {
		if (shaderObjIDs[i] != SHADER_NONE) { 
			glCompileShader(shaderObjIDs[i]);
		}
	}
	programHandle = glCreateProgram();              

	// attach
	
	for (int i = VertexShader; i <= FragmentShader; i++) {
		if (shaderObjIDs[i] != SHADER_NONE) {
			glAttachShader(programHandle, shaderObjIDs[i]);
		}
	}

	for (auto &iter : bindattrib_loc_names_map) {
		PRINT("Binding attrib \"%s\" to location %u (glBindAttribLocation).\n", iter.second.c_str(), iter.first);
		glBindAttribLocation(programHandle, iter.first, iter.second.c_str());
	}

	// if binding is used, it must be done before glLinkProgram is called :P

	glLinkProgram(programHandle);
	glUseProgram(programHandle);

	if (!checkShaderCompileStatus_all()) 
	{
		set_bad();
		goto cleanup;
	}
	if (!checkProgramLinkStatus()) {
		set_bad();
		goto cleanup;
	}

	
	#define my_delete_arr(arr) if(arr) delete [] arr; arr = NULL;

	construct_uniform_map();
	printStatus();

cleanup:
	my_delete_arr(vs_buf);
	my_delete_arr(tcs_buf);
	my_delete_arr(tes_buf);
	my_delete_arr(gs_buf); 
	my_delete_arr(fs_buf);
}

inline const char* shader_present(const GLuint *objIDs, GLint index) {
	if (objIDs[index] != SHADER_NONE) { return "yes"; }
	else { return "no"; }
}

void ShaderProgram::printStatus () const {
	PRINT("ShaderProgram %s status:\n", id_string.c_str());
	PRINT("shader\t\t\tpresent?\tid\n");
	PRINT("Vertex shader: \t\t%s\t\t%d\n", shader_present(shaderObjIDs, VertexShader), shaderObjIDs[VertexShader]);
	PRINT("TessCtrl shader: \t%s\t\t%d\n", shader_present(shaderObjIDs, TessellationControlShader), shaderObjIDs[TessellationControlShader]);
	PRINT("TessEval shader: \t%s\t\t%d\n", shader_present(shaderObjIDs, TessellationEvaluationShader), shaderObjIDs[TessellationEvaluationShader]);
	PRINT("Geometry shader: \t%s\t\t%d\n", shader_present(shaderObjIDs, GeometryShader), shaderObjIDs[GeometryShader]);
	PRINT("Fragment shader: \t%s\t\t%d\n", shader_present(shaderObjIDs, FragmentShader), shaderObjIDs[FragmentShader]);
	PRINT("bad flag: %d\n\n", bad);
}

char* ShaderProgram::readShaderFromFile(const std::string &filename, GLsizei *filesize)
{
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);

	if (!in.is_open()) { 
		PRINT( "(warning: ShaderProgram: couldn't open file %s: no such file or directory)\n", filename.c_str());
		return NULL; 
	}
	size_t length = cpp_getfilesize(in);
	*filesize = length;
	char *buf = new char[length+1];
	in.read(buf, length);
	in.close();

	buf[length] = '\0';

	return buf;
}

GLint ShaderProgram::checkShaderCompileStatus_all() // GL_COMPILE_STATUS
{	
	// should check GL_LINK_STATUS as well (glGetProgramInfoLog for linker)
	GLint succeeded[5] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLchar *log_buffers[5] = { NULL, NULL, NULL, NULL, NULL };
	GLint num_errors = 0;

	for (int i = VertexShader; i <= FragmentShader; i++) {
		if (shaderObjIDs[i] != SHADER_NONE) {
			glGetShaderiv(shaderObjIDs[i], GL_COMPILE_STATUS, &succeeded[i]);

			if (succeeded[i] == GL_FALSE)
			{	
				PRINT( "glGetShaderiv returned GL_FALSE for query GL_COMPILE_STATUS for shader %s, id: %d\n", shader_filenames[i].c_str(), shaderObjIDs[i]);
				GLint log_length = 0;
				++num_errors;
				glGetShaderiv(shaderObjIDs[i], GL_INFO_LOG_LENGTH, &log_length);
				log_buffers[i] = new GLchar[log_length + 1];
				memset(log_buffers[i], 0, log_length);

				glGetShaderInfoLog(shaderObjIDs[i], log_length, NULL, log_buffers[i]);

				log_buffers[i][log_length-1] = '\0';

				DeleteFile("shader.log");	// just to be sure :P

				std::ofstream logfile("shader.log", std::ios::out | std::ios::app);	
				logfile << this->shader_filenames[i] << ": \n";
				logfile << log_buffers[i];
				logfile.close();

				// also print to Visual Studio output window
			}
		}
	}

	if (num_errors > 0) {	

		PRINT( "\nShader %s: error log (glGetShaderInfoLog):\n-----------------------------------------------------------------\n\n", id_string.c_str());
		for (int i = VertexShader; i <= FragmentShader; i++) {
			if (succeeded[i] != GL_TRUE) {
				PRINT( "filename: %s\n\n", shader_filenames[i].c_str());
				PRINT( "%s\n\n", log_buffers[i]);
			}
		}
		PRINT( "\n---------------------------------------------------\n");
	}
	for (int i = VertexShader; i <= FragmentShader; i++) {	
		if (log_buffers[i]) delete [] log_buffers[i];
		log_buffers[i] = NULL;
	}
	if (num_errors > 0) { return false; }
	else { return true; }
}

GLint ShaderProgram::checkProgramLinkStatus() {
	GLint status;
	GLsizei log_len;
	glGetProgramiv(programHandle, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		glGetProgramInfoLog(programHandle, sizeof(logbuffer), &log_len, logbuffer);
		PRINT( "ShaderProgram::checkProgramLinkStatus: shader program link error. Log:\n%s\n", logbuffer);
		return 0;
	}
	else {
		glGetProgramInfoLog(programHandle, sizeof(logbuffer), &log_len, logbuffer);
		PRINT( "Program %s; LINK:\n", id_string.c_str());
		if (log_len > 0) { PRINT("log: \n"); 
						   PRINT("%s\n\n", logbuffer);
		}
		else { PRINT( "<OK>\n\n"); }

		glValidateProgram(programHandle);
		glGetProgramInfoLog(programHandle, sizeof(logbuffer), &log_len, logbuffer);
		PRINT( "Program %s; VALIDATION:\n\n", id_string.c_str());
		if (log_len > 0) {  PRINT("log: \n"); 
							PRINT("%s\n\n", logbuffer);
		}
		else { PRINT("<OK>\n\n"); }

		PRINT("\n");
		return 1;
	}
}

void ShaderProgram::construct_uniform_map() {
	GLint total = -1;
	glUseProgram(this->programHandle);
	glGetProgramiv(programHandle, GL_ACTIVE_UNIFORMS, &total);
#define UNIFORM_NAME_LEN_MAX 64
	char uniform_name_buf[UNIFORM_NAME_LEN_MAX];
	memset(uniform_name_buf, 0, UNIFORM_NAME_LEN_MAX);

	for (GLuint i = 0; i < total; i++) {
		GLsizei name_len = 0;
		GLint num = 0;
		GLenum type = GL_ZERO;
		glGetActiveUniform(programHandle, i, sizeof(uniform_name_buf) - 1, &name_len, &num, &type, uniform_name_buf);
		uniform_name_buf[name_len] = '\0';
		GLuint loc = glGetUniformLocation(programHandle, uniform_name_buf);
		uniforms.insert(std::pair<std::string, int>(std::string(uniform_name_buf), loc));
		//PRINT( "construct_uniform_map: %s: inserted uniform %s with location %d.\n", id_string.c_str(), uniform_name_buf, loc);
	}
}


bool ShaderProgram::active_uniform(const std::string &name, std::unordered_map<std::string,GLuint>::iterator *iter) {
	glUseProgram(programHandle);
	*iter = uniforms.find(name);
	if (*iter == uniforms.end()) {
		//PRINT("warning: shaderprogram %s: attempt to update non-present uniform \"%s\"!\n", this->id_string.c_str(), name.c_str());
		return false;
	} else {
		return true;
	}

}

void ShaderProgram::update_uniform_mat4(const std::string &uniform_name, const mat4 &m) {
	std::unordered_map<std::string, GLuint>::iterator iter;
	if (active_uniform(uniform_name, &iter)) {
		GLuint uniform_location = iter->second;
		glUniformMatrix4fv(uniform_location, 1, GL_FALSE, (const GLfloat*)m.rawData());
	}
}
void ShaderProgram::update_uniform_vec4(const std::string &uniform_name, const vec4 &v) {
	std::unordered_map<std::string, GLuint>::iterator iter;
	if (active_uniform(uniform_name, &iter)) {
		GLuint uniform_location = iter->second;
		glUniform4fv(uniform_location, 1, (const GLfloat*)v.rawData());
	}
}
void ShaderProgram::update_uniform_1f(const std::string &uniform_name, GLfloat value) {
	std::unordered_map<std::string, GLuint>::iterator iter;
	if (active_uniform(uniform_name, &iter)) {
		GLuint uniform_location = iter->second;
		glUniform1f(uniform_location, value);
	}
}

void ShaderProgram::update_uniform_1i(const std::string &uniform_name, GLint value) {
	std::unordered_map<std::string, GLuint>::iterator iter;
	if (active_uniform(uniform_name, &iter)) {
		GLuint uniform_location = iter->second;
		glUniform1i(uniform_location, value);
	}
}

void ShaderProgram::update_uniform_ivec2(const std::string &uniform_name, const GLint *GLint_doublet) {
	std::unordered_map<std::string, GLuint>::iterator iter;
	if (active_uniform(uniform_name, &iter)) {
		GLuint uniform_location = iter->second;
		glUniform2iv(uniform_location, 2, GLint_doublet);
	}
}

/*	std::ofstream logfile("shader.log", std::ios::out | std::ios::app);

	logfile << "compilation of GLSL shader source file "<< filename << " failed. Contents: \n\n";

	logfile.write(buf, length);
	logfile.put('\n');
	logfile.put('\n');
	logfile.close();
 */
