// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_ShaderProgram.hpp"
#include <glad/gl.h>

namespace Cori {

    OpenGLShaderProgram::OpenGLShaderProgram(const char* vertexPath, const char* fragmentPath, const char* geometryPath) {

#ifdef DEBUG_BUILD
        m_ShaderNames = "  Vertex shader: " + GetFilename(vertexPath) + "\n  Fragment shader: " + GetFilename(fragmentPath) + "\n  Geometry shader: " + GetFilename(geometryPath);
#endif

        // 1. retrieve the vertex/fragment source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        std::string geometryCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        std::ifstream gShaderFile;
        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        gShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
            // if geometry shader path is present, also load a geometry shader
            if (geometryPath != nullptr)
            {
                gShaderFile.open(geometryPath);
                std::stringstream gShaderStream;
                gShaderStream << gShaderFile.rdbuf();
                gShaderFile.close();
                geometryCode = gShaderStream.str();
            }
        }
        catch (std::ifstream::failure& e)
        {
            CORI_CORE_ERROR("Creation of OpenGLShaderProgram with shaders:\n{0}\nERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: {1}", m_ShaderNames, e.what());
        }
        const char* vShaderCode = vertexCode.c_str();

        const char* fShaderCode = fragmentCode.c_str();
        // 2. compile shaders
        GLuint vertex, fragment, geometry;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        if (CheckCompileErrors(vertex, "VERTEX") == false) {
            m_CreationSuccessful = false;
        }
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        if (CheckCompileErrors(fragment, "FRAGMENT") == false) {
            m_CreationSuccessful = false;
        }
        // if geometry shader is given, compile geometry shader
        if (!geometryPath)
        {
            const char* gShaderCode = geometryCode.c_str();
            geometry = glCreateShader(GL_GEOMETRY_SHADER);
            glShaderSource(geometry, 1, &gShaderCode, NULL);
            glCompileShader(geometry);
            if (CheckCompileErrors(geometry, "GEOMETRY") == false) {
                m_CreationSuccessful = false;
            }
        }
        // shader Program
        m_ID = glCreateProgram();
        glProgramParameteri(m_ID, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glAttachShader(m_ID, vertex);
        glAttachShader(m_ID, fragment);
        if (!geometryPath) {
            glAttachShader(m_ID, geometry);
        }
        glLinkProgram(m_ID);

        if (CheckCompileErrors(m_ID, "PROGRAM") == false) {
			m_CreationSuccessful = false;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (!geometryPath) {
            glDeleteShader(geometry);
        }

        if (m_CreationSuccessful) {
            CORI_CORE_INFO("Creation of OpenGLShaderProgram with shaders:\n{0}\nHas been successful", m_ShaderNames);
        }
        else {
            CORI_CORE_ERROR("Creation of OpenGLShaderProgram with shaders:\n{0}\nHas failed", m_ShaderNames);
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

    void OpenGLShaderProgram::UseInPipeline(const uint32_t PipelinePrgramID) const {
        glUseProgramStages(PipelinePrgramID, GL_ALL_SHADER_BITS, m_ID);
    }

    void OpenGLShaderProgram::SetBool(const std::string& name, const bool value) const {
        glProgramUniform1i(m_ID, glGetUniformLocation(m_ID, name.c_str()), static_cast<int>(value));
    }

    void OpenGLShaderProgram::SetInt(const std::string& name, const int value) const {
        glProgramUniform1i(m_ID, glGetUniformLocation(m_ID, name.c_str()), value);
    }

    void OpenGLShaderProgram::SetFloat(const std::string& name, const float value) const {
        glProgramUniform1f(m_ID, glGetUniformLocation(m_ID, name.c_str()), value);
    }

    void OpenGLShaderProgram::SetVec2(const std::string& name, const glm::vec2& value) const {
        glProgramUniform2fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
    }

    void OpenGLShaderProgram::SetVec3(const std::string& name, const glm::vec3& value) const {
        glProgramUniform3fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
    }

    void OpenGLShaderProgram::SetVec4(const std::string& name, const glm::vec4& value) const {
        glProgramUniform4fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
    }

    void OpenGLShaderProgram::SetMat2(const std::string& name, const glm::mat2& value) const {
        glProgramUniformMatrix2fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, GL_FALSE, &value[0][0]);
    }

    void OpenGLShaderProgram::SetMat3(const std::string& name, const glm::mat3& value) const {
        glProgramUniformMatrix3fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, GL_FALSE, &value[0][0]);
    }

    void OpenGLShaderProgram::SetMat4(const std::string& name, const glm::mat4& value) const {
        glProgramUniformMatrix4fv(m_ID, glGetUniformLocation(m_ID, name.c_str()), 1, GL_FALSE, &value[0][0]);
    }

    bool OpenGLShaderProgram::CheckCompileErrors(uint32_t shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        bool result = true;
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                CORI_CORE_ERROR("Shader compilation error in OpenGLShaderProgram with shaders:\n{0}\nProblematic type: {1} \nInfoLog: {2}", m_ShaderNames, type, infoLog);
                result = false;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                CORI_CORE_ERROR("Shader linking error in OpenGLShaderProgram with shaders:\n{0}\nProblematic type: {1} \nInfoLog: {2}", m_ShaderNames, type, infoLog);
                result = false;
            }
        }
        return result;
    }

    std::string OpenGLShaderProgram::GetFilename(const char* filepath) {
        if (!filepath) { return "No filepath specified"; }

        const char* last_slash = std::strrchr(filepath, '/');
        const char* last_backslash = std::strrchr(filepath, '\\');

        const char* filename = filepath;
        if (last_slash && last_backslash) {
            filename = (last_slash > last_backslash) ? last_slash + 1 : last_backslash + 1;
        }
        else if (last_slash) {
            filename = last_slash + 1;
        }
        else if (last_backslash) {
            filename = last_backslash + 1;
        }

		return static_cast<std::string>(filename);

    }
}