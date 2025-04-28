#pragma once
#include "GraphicsAPIs.hpp"

namespace Cori {
	class ShaderProgram {
	public:
		virtual ~ShaderProgram() {};
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void UseInPipeline(const uint32_t& PipelinePrgramID) const = 0;

		virtual void SetBool(const std::string& name, const bool& value) const = 0;
		virtual void SetInt(const std::string& name, const int& value) const = 0;
		virtual void SetFloat(const std::string& name, const float& value) const = 0;
		virtual void SetVec2(const std::string& name, const glm::vec2& value) const = 0;
		virtual void SetVec3(const std::string& name, const glm::vec3& value) const = 0;
		virtual void SetVec4(const std::string& name, const glm::vec4& value) const = 0;
		virtual void SetMat2(const std::string& name, const glm::mat2& value) const = 0;
		virtual void SetMat3(const std::string& name, const glm::mat3& value) const = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) const = 0;
		
		virtual std::string GetShaderNames() const = 0;

		static ShaderProgram* Create(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);
	};
}