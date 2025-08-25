#include "GL_ShaderProgram.hpp"
#include <glad/gl.h>
#include "FileSystem/FileManager.hpp"

namespace Cori {

	bool OpenGLShaderProgram::PreCreateHook(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) {
		if (!std::filesystem::exists(vertexPath)) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "Could not find vertex shader at the specified path: '{}', file does not exist. This will likely not crash the application but will seriously mess up with rendering.", vertexPath.string());
		}
		if (!std::filesystem::exists(fragmentPath)) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "Could not find fragment shader at the specified path: '{}', file does not exist. This will likely not crash the application but will seriously mess up with rendering.", fragmentPath.string());
		}
		if (!geometryPath.empty()) {
			if (!std::filesystem::exists(geometryPath)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "Could not find geometry shader at the specified path: '{}', file does not exist. This will likely not crash the application but will seriously mess up with rendering.", geometryPath.string());
			}
		}
		return true;
	}

	OpenGLShaderProgram::OpenGLShaderProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) {
		bool geometryShaderPresent = false;

		if (!geometryPath.empty()) {
			geometryShaderPresent = true;
		}

#ifdef DEBUG_BUILD
		m_ShaderNames = CORI_SECOND_LINE_SPACING + "Vertex shader: " + vertexPath.filename().string() + "\n" + CORI_SECOND_LINE_SPACING + "Fragment shader: " + fragmentPath.filename().string() + "\n" + CORI_SECOND_LINE_SPACING + "Geometry shader: " + (geometryShaderPresent ? geometryPath.filename().string() + "" : "Not specified (not an error)");
#endif

		const std::string vertexCode = FileManager::ReadTextFile(vertexPath);
		const char* vertexSource = vertexCode.c_str();


		const std::string fragmentCode = FileManager::ReadTextFile(fragmentPath);
		const char* fragmentSource = fragmentCode.c_str();

		GLuint geometry;
		const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vertexSource, nullptr);
		glCompileShader(vertex);
		if (CheckCompileErrors(vertex, "VERTEX") == false) {
			m_CreationSuccessful = false;
		}
		const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fragmentSource, nullptr);
		glCompileShader(fragment);
		if (CheckCompileErrors(fragment, "FRAGMENT") == false) {
			m_CreationSuccessful = false;
		}
		if (geometryShaderPresent) {

			const std::string geometryCode = FileManager::ReadTextFile(geometryPath);
			const char* geometrySource = geometryCode.c_str();

			geometry = glCreateShader(GL_GEOMETRY_SHADER);
			glShaderSource(geometry, 1, &geometrySource, nullptr);
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
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "(GL_RuntimeID: {}): Creation of {} with shaders:\n{}\n{}Has been successful", m_ID, m_DebugName, m_ShaderNames, CORI_SECOND_LINE_SPACING);
		}
		else {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "(GL_RuntimeID: {}): Creation of {} with shaders:\n{}\n{}Has failed", m_ID, m_DebugName,m_ShaderNames, CORI_SECOND_LINE_SPACING);
		}
	}

	OpenGLShaderProgram::~OpenGLShaderProgram() {
		glDeleteProgram(m_ID);
	}

	void OpenGLShaderProgram::Bind() const {
		glUseProgram(m_ID);
	}

	void OpenGLShaderProgram::Unbind() const {
		glUseProgram(0);
	}

	void OpenGLShaderProgram::SetBool(const std::string& name, const bool value) const {
		if (!m_UniformLocations.contains(name)) {
			m_UniformLocations.insert({ name, static_cast<int32_t>(glGetUniformLocation(m_ID, name.c_str())) });
		}

		glProgramUniform1i(m_ID, static_cast<GLint>(m_UniformLocations.at(name)), static_cast<GLint>(value));
	}

	void OpenGLShaderProgram::SetInt(const std::string& name, const int32_t value) const {
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

	bool OpenGLShaderProgram::CheckCompileErrors(const uint32_t shader, std::string type) {
		GLint success;
		GLchar infoLog[1024];
		bool result = true;
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "(GL_RuntimeID: {0}): Shader compilation error in with shaders:\n{4}{1}\n{4}Problematic type: {2}\n{4}InfoLog: {3}", m_ID, m_ShaderNames, type, infoLog, CORI_SECOND_LINE_SPACING);
				result = false;
			}
		}
		else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::ShaderProgram }, "(GL_RuntimeID: {0}): Shader linking error in with shaders:\n{4}{1}\n{4}Problematic type: {2}\n{4}InfoLog: {3}", m_ID, m_ShaderNames, type, infoLog, CORI_SECOND_LINE_SPACING);
				result = false;
			}
		}
		return result;
	}
}