#pragma once RegisterInFactory
#include "../ShaderProgram.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"
#include "Graphics/GraphicsAPIs.hpp"

namespace Cori {
	class OpenGLShaderProgram final : public ShaderProgram, public Profiling::Trackable<OpenGLShaderProgram, ShaderProgram>, public RegisterInFactory<ShaderProgram, OpenGLShaderProgram, GraphicsAPIs, GraphicsAPIs::OpenGL, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&> {
	public:
		static bool PreCreateHook(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath);
		OpenGLShaderProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath);
		~OpenGLShaderProgram() override;

		void Bind() const override;
		void Unbind() const override;

		uint32_t GetID() const override { return m_ID; }

		void SetBool(const std::string& name, const bool value) const override;
		void SetInt(const std::string& name, const int32_t value) const override;
		void SetFloat(const std::string& name, const float value) const override;
		void SetVec2(const std::string& name, const glm::vec2& value) const override;
		void SetVec3(const std::string& name, const glm::vec3& value) const override;
		void SetVec4(const std::string& name, const glm::vec4& value) const override;
		void SetMat2(const std::string& name, const glm::mat2& value) const override;
		void SetMat3(const std::string& name, const glm::mat3& value) const override;
		void SetMat4(const std::string& name, const glm::mat4& value) const override;

		std::string GetShaderNames() const override { return m_ShaderNames; }
	private:
		uint32_t m_ID;
		bool m_CreationSuccessful{ true };

		mutable std::unordered_map<std::string, int32_t> m_UniformLocations;

		std::string m_DebugName;

		std::string m_ShaderNames;

		bool CheckCompileErrors(uint32_t shader, std::string type);

		CORI_REGISTERED_FACTORY_INIT;
	};

}