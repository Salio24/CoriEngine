#pragma once

namespace Cori {
	namespace Graphics {
		/**
		 * @brief ShaderProgram, there is no use for it on the client side for now.
		 */
		class ShaderProgram {
		public:
			class Descriptor {
			public:
				constexpr Descriptor(std::string name, std::filesystem::path vertexPath, std::filesystem::path fragmentPath, std::filesystem::path geometryPath = {}) noexcept
					: m_VertexPath(std::move(vertexPath)),
					m_FragmentPath(std::move(fragmentPath)),
					m_GeometryPath(std::move(geometryPath)),
					m_Name(std::move(name)),
					m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
				{ }

				using AssetType = ShaderProgram;

				[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

				constexpr bool operator==(const Descriptor& other) const noexcept {
					return m_RuntimeID == other.m_RuntimeID;
				}

				struct Hasher {
					std::size_t operator()(const Descriptor& handle) const noexcept {
						return std::hash<uint32_t>{}(handle.m_RuntimeID);
					}
				};

				const std::filesystem::path m_VertexPath;
				const std::filesystem::path m_FragmentPath;
				const std::filesystem::path m_GeometryPath;
				const std::string m_Name;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
			};

			virtual ~ShaderProgram() = default;
			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;

			[[nodiscard]] virtual uint32_t GetID() const = 0;

			virtual void SetBool(const char* name, const bool value) const = 0;
			virtual void SetInt(const char* name, const int32_t value) const = 0;
			virtual void SetFloat(const char* name, const float value) const = 0;
			virtual void SetVec2(const char* name, const glm::vec2& value) const = 0;
			virtual void SetVec3(const char* name, const glm::vec3& value) const = 0;
			virtual void SetVec4(const char* name, const glm::vec4& value) const = 0;
			virtual void SetMat2(const char* name, const glm::mat2& value) const = 0;
			virtual void SetMat3(const char* name, const glm::mat3& value) const = 0;
			virtual void SetMat4(const char* name, const glm::mat4& value) const = 0;

			[[nodiscard]] virtual std::string GetShaderNames() const = 0;

			[[nodiscard]] static std::shared_ptr<ShaderProgram> Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath = {});

			[[nodiscard]] static std::shared_ptr<ShaderProgram> Create(const Descriptor& descriptor);
		};
	}
}