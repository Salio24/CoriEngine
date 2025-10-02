#pragma once
#include "Profiling/Trackable.hpp"
#include "Graphics/SpriteAtlas.hpp"
#include "Animation.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class QuadAnimator;
			}
		}
	}
	namespace Graphics {

		class AnimationPack : public Profiling::Trackable<AnimationPack>, public std::enable_shared_from_this<AnimationPack>{
		public:
			enum ConfigType : uint8_t {
				ASEPRITE,
				CORI_UNIFORM,
				CORI_VARYING,
				INVALID
			};

			class Descriptor {
			public:
				constexpr Descriptor(std::string name, std::filesystem::path jsonPath, const ConfigType type) noexcept
					: m_JsonPath(std::move(jsonPath)),
					m_ConfigType(type),
					m_Name(std::move(name)),
					m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
				{ }

				using AssetType = AnimationPack;

				[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

				constexpr bool operator==(const Descriptor& other) const noexcept {
					return m_RuntimeID == other.m_RuntimeID;
				}

				struct Hasher {
					std::size_t operator()(const Descriptor& descriptor) const noexcept {
						return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
					}
				};

				const std::filesystem::path m_JsonPath;
				const ConfigType m_ConfigType;
				const std::string m_Name;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };

			};

			[[nodiscard]] static std::shared_ptr<AnimationPack> Create(const std::filesystem::path& jsonPath, ConfigType type, const float timeStep, const std::string& name);

			[[nodiscard]] static std::shared_ptr<AnimationPack> Create(const Descriptor& descriptor);

			[[nodiscard]] Animation GetAnimation(const uint32_t index);

		protected:
			friend Animation;
			friend World::Components::Entity::QuadAnimator;
			std::vector<AnimationData> m_Animations;

			std::variant<std::shared_ptr<SpriteAtlas>, std::shared_ptr<Texture2D>> m_TextureOrAtlas;

		private:
			std::string m_Name;
		protected:
			ConfigType m_Type;
		private:
			AnimationPack(std::vector<AnimationData> animations, const std::shared_ptr<SpriteAtlas>& spriteAtlas, std::string name, const ConfigType type);
			AnimationPack(std::vector<AnimationData> animations, const std::shared_ptr<Texture2D>& spriteAtlas, std::string name, const ConfigType type);

			AnimationPack();

			bool m_Valid = false;
		};
	}
}
