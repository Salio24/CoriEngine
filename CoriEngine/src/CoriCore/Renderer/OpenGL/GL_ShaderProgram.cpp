// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_ShaderProgram.hpp"
#include <glad/gl.h>
#include "../../AssetManager/FileManager.hpp"

namespace Cori {

	OpenGLShaderProgram::OpenGLShaderProgram(const std::string_view debugName, const std::string_view vertexPath, const std::string_view fragmentPath, const std::string_view geometryPath) {

		m_DebugName = debugName;

		bool geometryShaderPresent = false;

		if (geometryPath != "") {
			geometryShaderPresent = true;
		}

#ifdef DEBUG_BUILD
		m_ShaderNames = CORI_SECOND_LINE_SPACING + "Vertex shader: " + FileManager::GetFilename(vertexPath) + "\n" + CORI_SECOND_LINE_SPACING + "Fragment shader: " + FileManager::GetFilename(fragmentPath) + "\n" + CORI_SECOND_LINE_SPACING + "Geometry shader: " + (geometryShaderPresent ? FileManager::GetFilename(geometryPath) + "" : "Not specified (not an error)");
#endif


		std::string vertexCode = FileManager::ReadTextFile(vertexPath);
		const char* vertexSource = vertexCode.c_str();


		std::string fragmentCode = FileManager::ReadTextFile(fragmentPath);
		const char* fragmentSource = fragmentCode.c_str();

		GLuint vertex, fragment, geometry;
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vertexSource, NULL);
		glCompileShader(vertex);
		if (CheckCompileErrors(vertex, "VERTEX") == false) {
			m_CreationSuccessful = false;
		}
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fragmentSource, NULL);
		glCompileShader(fragment);
		if (CheckCompileErrors(fragment, "FRAGMENT") == false) {
			m_CreationSuccessful = false;
		}
		if (geometryShaderPresent) {

			std::string geometryCode = FileManager::ReadTextFile(geometryPath);
			const char* geometrySource = geometryCode.c_str();

			geometry = glCreateShader(GL_GEOMETRY_SHADER);
			glShaderSource(geometry, 1, &geometrySource, NULL);
			glCompileShader(geometry);
			if (CheckCompileErrors(geometry, "GEOMETRY") == false) {
				m_CreationSuccessful = false;
			}
		}
		m_ID = glCreateProgram();
		//glProgramParameteri(m_ID, GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(m_ID, vertex);
		glAttachShader(m_ID, fragment);
		if (geometryShaderPresent) {
			glAttachShader(m_ID, geometry);
		}
		glLinkProgram(m_ID);

		if (CheckCompileErrors(m_ID, "PROGRAM") == false) {
			m_CreationSuccessful = false;
		}

		glDeleteShader(vertex);
		glDeleteShader(fragment);
		if (geometryShaderPresent) {
			glDeleteShader(geometry);
		}

		if (m_CreationSuccessful) {
			CORI_CORE_DEBUG("GL_ShaderProgram (GL_RuntimeID: {0}): Creation of {1} with shaders:\n{2}\n{3}Has been successful", m_ID, m_DebugName, m_ShaderNames, CORI_SECOND_LINE_SPACING);
		}
		else {
			CORI_CORE_ERROR("GL_ShaderProgram (GL_RuntimeID: {0}): Creation of {1} with shaders:\n{2}\n{3}Has failed", m_ID, m_DebugName,m_ShaderNames, CORI_SECOND_LINE_SPACING);
		}
	}

	OpenGLShaderProgram::~OpenGLShaderProgram() {
		glDeleteProgram(m_ID);
	}

	void OpenGLShaderProgram::Bind() const {
		//CORI_CORE_WARN("You shouldn't really use this when using separable shader programs");
		glUseProgram(m_ID);
	}

	void OpenGLShaderProgram::Unbind() const {
		//CORI_CORE_WARN("You shouldn't really use this when using separable shader programs");
		glUseProgram(0);
	}

	void OpenGLShaderProgram::SetBool(const std::string& name, const bool value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform1i(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), static_cast<GLint>(value));
	}

	void OpenGLShaderProgram::SetInt(const std::string& name, const int value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform1i(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), value);
	}

	void OpenGLShaderProgram::SetFloat(const std::string& name, const float value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform1f(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), value);
	}

	void OpenGLShaderProgram::SetVec2(const std::string& name, const glm::vec2& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform2fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, &value[0]);
	}

	void OpenGLShaderProgram::SetVec3(const std::string& name, const glm::vec3& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform3fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, &value[0]);
	}

	void OpenGLShaderProgram::SetVec4(const std::string& name, const glm::vec4& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform4fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, &value[0]);
	}

	void OpenGLShaderProgram::SetMat2(const std::string& name, const glm::mat2& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniformMatrix2fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, GL_FALSE, &value[0][0]);
	}

	void OpenGLShaderProgram::SetMat3(const std::string& name, const glm::mat3& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniformMatrix3fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, GL_FALSE, &value[0][0]);
	}

	void OpenGLShaderProgram::SetMat4(const std::string& name, const glm::mat4& value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniformMatrix4fv(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), 1, GL_FALSE, &value[0][0]);
	}

	bool OpenGLShaderProgram::CheckCompileErrors(uint32_t shader, std::string type) {
		GLint success;
		GLchar infoLog[1024];
		bool result = true;
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				CORI_CORE_ERROR("GL_ShaderProgram (GL_RuntimeID: {0}): Shader compilation error in {1} with shaders:\n{5}{2}\n{5}Problematic type: {3}\n{5}InfoLog: {4}", m_ID, m_DebugName, m_ShaderNames, type, infoLog, CORI_SECOND_LINE_SPACING);
				result = false;
			}
		}
		else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				CORI_CORE_ERROR("GL_ShaderProgram (GL_RuntimeID: {0}): Shader linking error in {1} with shaders:\n{5}{2}\n{5}Problematic type: {3}\n{5}InfoLog: {4}", m_ID, m_DebugName, m_ShaderNames, type, infoLog, CORI_SECOND_LINE_SPACING);
				result = false;
			}
		}
		return result;
	}
}