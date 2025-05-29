#pragma once 
#include <string>
#include <fstream>
#include <sstream>
#include "../ShaderProgram.hpp"
#include "../../Profiling/Trackable.hpp"

namespace Cori {
	class OpenGLShaderProgram : public ShaderProgram {
	public:
		OpenGLShaderProgram(const std::string_view debugName, const std::string_view vertexPath, const std::string_view fragmentPath, const std::string_view geometryPath = "");
		virtual ~OpenGLShaderProgram();

		virtual void Bind() const override;
		virtual void Unbind() const override;
		
		virtual uint32_t GetID() const override { return m_ID; }

		virtual void SetBool(const std::string& name, const bool value) const override;
		virtual void SetInt(const std::string& name, const int value) const override;
		virtual void SetFloat(const std::string& name, const float value) const override;
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

		mutable std::unordered_map<std::string, int32_t> m_UniformLocations;

		std::string m_DebugName;

		std::string m_ShaderNames;

		bool CheckCompileErrors(uint32_t shader, std::string type);
	};

}