#pragma once 
#include <string>
#include <fstream>
#include <sstream>
#include "../ShaderProgram.hpp"

namespace Cori {
	class OpenGLShaderProgram : public ShaderProgram {
	public:
		OpenGLShaderProgram(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);
		virtual ~OpenGLShaderProgram();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void UseInPipeline(const uint32_t& PipelinePrgramID) const override;

		virtual void SetBool(const std::string& name, const bool& value) const override;
		virtual void SetInt(const std::string& name, const int& value) const override;
		virtual void SetFloat(const std::string& name, const float& value) const override;
		virtual void SetVec2(const std::string& name, const glm::vec2& value) const override;
		virtual void SetVec3(const std::string& name, const glm::vec3& value) const override;
		virtual void SetVec4(const std::string& name, const glm::vec4& value) const override;
		virtual void SetMat2(const std::string& name, const glm::mat2& value) const override;
		virtual void SetMat3(const std::string& name, const glm::mat3& value) const override;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) const override;

		inline virtual std::string GetShaderNames() const override { return m_ShaderNames; }
	private:
		uint32_t m_ID;
		bool m_CreationSuccessful{ true };

		std::string m_ShaderNames;

		bool CheckCompileErrors(uint32_t shader, std::string type);

		// THIS IS TEMPORARY AND REALY SHOULD NOT BE HERE, WILL MOVE IT TO "IO" CLASS AS SOON AS I WILL HAVE ONE
		std::string GetFilename(const char* filepath);
	};

}