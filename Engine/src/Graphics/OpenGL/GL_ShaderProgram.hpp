#pragma once
#include "../ShaderProgram.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"
#include "Graphics/GraphicsAPIs.hpp"

namespace Cori {
	namespace Graphics {
		class OpenGLShaderProgram final : public ShaderProgram, public Profiling::Trackable<OpenGLShaderProgram, ShaderProgram>, public Core::RegisterInFactory<ShaderProgram, OpenGLShaderProgram, GraphicsAPIs, GraphicsAPIs::OpenGL, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&> {
		public:
			static bool PreCreateHook(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath);
			OpenGLShaderProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath);
			~OpenGLShaderProgram() override;

			void Bind() const override;
			void Unbind() const override;

			uint32_t GetID() const override { return m_ID; }

			void SetBool(const char* name, const bool value) const override;
			void SetInt(const char* name, const int32_t value) const override;
			void SetFloat(const char* name, const float value) const override;
			void SetVec2(const char* name, const glm::vec2& value) const override;
			void SetVec3(const char* name, const glm::vec3& value) const override;
			void SetVec4(const char* name, const glm::vec4& value) const override;
			void SetMat2(const char* name, const glm::mat2& value) const override;
			void SetMat3(const char* name, const glm::mat3& value) const override;
			void SetMat4(const char* name, const glm::mat4& value) const override;

			std::string GetShaderNames() const override { return m_ShaderNames; }
		private:
			struct TransparentHash {
				using is_transparent = void;
				size_t operator()(std::string_view sv) const noexcept {
					return std::hash<std::string_view>{}(sv);
				}
			};

			struct TransparentEqual {
				using is_transparent = void;
				bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
					return lhs == rhs;
				}
			};

			uint32_t m_ID;
			bool m_CreationSuccessful{ true };

			mutable std::unordered_map<std::string, int32_t, TransparentHash, TransparentEqual> m_UniformLocations;

			std::string m_DebugName;

			std::string m_ShaderNames;

			bool CheckCompileErrors(uint32_t shader, std::string type);

			CORI_REGISTERED_FACTORY_INIT;
		};
	}
}