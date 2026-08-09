#pragma once
#include "Utility/ReflectionHelpers.hpp"

namespace Cori {
	namespace Core {
		struct CVarDesc;
	}

	/**
	 * @brief The annotation vocabulary a settings struct uses to describe its own fields.
	 * @details These are ordinary structural types applied with the C++26 annotation syntax, e.g.
	 * [[=CVar::Desc{"..."}]]. The schema generator reads them back with std::meta::annotations_of, so a field
	 * carries its help text, its bounds and its policy right where it is declared instead of in a parallel table.
	 * @note Annotation values are const qualified, matching them needs std::meta::remove_cv on the type.
	 */
	namespace CVar {
		/**
		 * @brief Help text for a field, shown by the console and by `help <name>`.
		 * @note Fixed capacity because std::string_view is not a structural type and so cannot live in an annotation.
		 */
		struct Desc {
			static constexpr uint64_t s_Capacity{ 160 };

			char m_Text[s_Capacity]{};

			consteval Desc(const char* text) { // NOLINT implicit by design, it is only ever written as [[=CVar::Desc{"..."}]]
				uint64_t i = 0;
				for (; text[i] != '\0'; ++i) {
					if (i + 1 >= s_Capacity) {
						throw "CVar::Desc text is longer than Desc::s_Capacity";
					}
					m_Text[i] = text[i];
				}
				m_Text[i] = '\0';
			}

			[[nodiscard]] constexpr std::string_view View() const { return m_Text; }
		};

		/**
		 * @brief Inclusive bounds. Values coming from the console or from the archive are clamped into this range.
		 */
		struct Range {
			double m_Min{ 0.0 };
			double m_Max{ 0.0 };
		};

		/**
		 * @brief Signature of a field's change callback.
		 * @note Takes the desc rather than the value so one function can serve several fields, and so it can read
		 * whatever it needs off the struct itself.
		 */
		using ChangeFn = void (*)(const Core::CVarDesc&);

		/**
		 * @brief Calls this after the field has been given a new value, on the thread that wrote it.
		 * @details Optional and per field, a field without one costs a null pointer in its desc. The function only
		 * has to be declared where the settings struct is written, so the definition can stay in the subsystem's
		 * own translation unit.
		 * \n Nothing the renderer reads needs one of these: it works from the frame snapshot and can compare that
		 * against what it last built. These are for the main thread subsystems that hold no snapshot and have to be
		 * told, an audio mixer or the window.
		 */
		struct OnChange {
			ChangeFn m_Fn{ nullptr };
		};

		struct ArchiveTag {};
		struct CheatTag {};
		struct ReadOnlyTag {};
		struct RestartTag {};

		/**
		 * @brief Persist this field to the user settings archive when it differs from its default.
		 */
		inline constexpr ArchiveTag Archive{};

		/**
		 * @brief Field is a debug affordance, refused unless cheats are enabled.
		 */
		inline constexpr CheatTag Cheat{};

		/**
		 * @brief Field can be inspected but never written from the console.
		 */
		inline constexpr ReadOnlyTag ReadOnly{};

		/**
		 * @brief Field only takes effect on the next run, overriding its struct's tier. Writes only reach the archive.
		 */
		inline constexpr RestartTag Restart{};
	}

	namespace Core {

		/**
		 * @brief How a write to a field reaches the thing that reads it.
		 * @details eLive - written straight into the live struct.
		 * \n eRestart - never written into the live struct at all, the value only reaches the archive.
		 * @note It is not safe to read a field off anything than main thread.
		 */
		enum class ApplyTier : uint8_t {
			eLive,
			eRestart
		};

		enum class CVarType : uint8_t {
			eBool,
			eInt8,
			eInt16,
			eInt32,
			eInt64,
			eUInt8,
			eUInt16,
			eUInt32,
			eUInt64,
			eFloat,
			eDouble
		};

		enum class CVarFlags : uint32_t {
			eNone     = 0,
			eArchive  = 1u << 0,
			eCheat    = 1u << 1,
			eReadOnly = 1u << 2
		};

		[[nodiscard]] constexpr CVarFlags operator|(const CVarFlags a, const CVarFlags b) noexcept {
			return static_cast<CVarFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		}

		constexpr CVarFlags& operator|=(CVarFlags& a, const CVarFlags b) noexcept {
			a = a | b;
			return a;
		}

		[[nodiscard]] constexpr bool HasFlag(const CVarFlags value, const CVarFlags flag) noexcept {
			return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
		}

		/**
		 * @brief One enumerator of an enum typed field, so the console can complete and print names instead of integers.
		 */
		struct EnumEntry {
			std::string_view m_Name;
			int64_t m_Value{ 0 };
		};

		/**
		 * @brief Everything about one field that is known at compile time.
		 * @details Descs are generated into a constexpr array per settings struct and live in read only storage, so
		 * registration costs nothing but linking a base pointer. Nothing here is per instance, the value itself is
		 * reached as *(base + m_Offset).
		 */
		struct CVarDesc {
			std::string_view m_Name;
			std::string_view m_Help;
			std::string_view m_Field;
			std::string_view m_Struct;
			std::span<const EnumEntry> m_Enumerators;
			CVar::ChangeFn m_OnChange{ nullptr };
			double m_Default{ 0.0 };
			double m_Min{ 0.0 };
			double m_Max{ 0.0 };
			uint32_t m_Offset{ 0 };
			CVarType m_Type{ CVarType::eFloat };
			CVarFlags m_Flags{ CVarFlags::eNone };
			ApplyTier m_Tier{ ApplyTier::eLive };
			bool m_HasRange{ false };

			[[nodiscard]] constexpr bool IsEnum() const { return !m_Enumerators.empty(); }
		};

		namespace Internal {
			template <typename>
			inline constexpr bool AlwaysFalseCVar = false;

			template <typename T>
			consteval CVarType StorageTypeOf() {
				if constexpr (std::is_enum_v<T>)               { return StorageTypeOf<std::underlying_type_t<T>>(); }
				else if constexpr (std::is_same_v<T, bool>)    { return CVarType::eBool; }
				else if constexpr (std::is_same_v<T, float>)   { return CVarType::eFloat; }
				else if constexpr (std::is_same_v<T, double>)  { return CVarType::eDouble; }
				else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
					if constexpr (sizeof(T) == 1)      { return CVarType::eInt8; }
					else if constexpr (sizeof(T) == 2) { return CVarType::eInt16; }
					else if constexpr (sizeof(T) == 4) { return CVarType::eInt32; }
					else                               { return CVarType::eInt64; }
				}
				else if constexpr (std::is_integral_v<T>) {
					if constexpr (sizeof(T) == 1)      { return CVarType::eUInt8; }
					else if constexpr (sizeof(T) == 2) { return CVarType::eUInt16; }
					else if constexpr (sizeof(T) == 4) { return CVarType::eUInt32; }
					else                               { return CVarType::eUInt64; }
				}
				else {
					static_assert(AlwaysFalseCVar<T>, "Type is not usable as a CVar.");
					return CVarType::eDouble;
				}
			}

			template <typename E> requires std::is_enum_v<E>
			consteval auto MakeEnumTable() {
				static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^E));

				std::array<EnumEntry, enumerators.size()> out{};
				uint64_t i = 0;
				template for (constexpr auto enumerator : enumerators) {
					out[i] = EnumEntry{
						.m_Name = std::define_static_string(std::meta::identifier_of(enumerator)),
						.m_Value = static_cast<int64_t>(static_cast<std::underlying_type_t<E>>([:enumerator:]))
					};
					++i;
				}
				return out;
			}

			template <typename E> requires std::is_enum_v<E>
			inline constexpr auto EnumTable = MakeEnumTable<E>();
		}

		template <typename T>
		concept HasCVarCategory = requires { { T::s_Category } -> std::convertible_to<std::string_view>; };

		template <typename T>
		concept HasCVarTier = requires { { T::s_Tier } -> std::convertible_to<ApplyTier>; };

		/**
		 * @brief A type usable with CORI_SETTINGS.
		 */
		template <typename T>
		concept IsSettingsStruct = std::is_aggregate_v<T> && std::is_trivially_copyable_v<T> && HasCVarCategory<T>;

		namespace Internal {
			template <typename T>
			consteval ApplyTier StructTier() {
				if constexpr (HasCVarTier<T>) {
					return T::s_Tier;
				}
				else {
					return ApplyTier::eLive;
				}
			}

			/**
			 * @brief Turns a settings struct into a flat array of descs, one per field. Runs entirely at compile time.
			 */
			template <IsSettingsStruct T>
			consteval auto MakeSchema() {
				static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

				static constexpr T defaults{};
				static constexpr ApplyTier structTier = StructTier<T>();
				static constexpr std::string_view structName = std::define_static_string(std::meta::identifier_of(^^T));

				std::array<CVarDesc, members.size()> out{};

				uint64_t i = 0;
				template for (constexpr auto member : members) {
					using M = [:std::meta::type_of(member):];
					static_assert(std::is_enum_v<M> || std::is_arithmetic_v<M>, "A settings struct field must be an arithmetic or enum type.");

					constexpr CVar::Range range = Utility::AnnotationOr<CVar::Range>(member, CVar::Range{ 0.0, 0.0 });
					constexpr CVar::Desc help = Utility::AnnotationOr<CVar::Desc>(member, CVar::Desc{ "" });

					constexpr ApplyTier tier = Utility::HasAnnotation<CVar::RestartTag>(member) ? ApplyTier::eRestart : structTier;

					CVarFlags flags = CVarFlags::eNone;
					if constexpr (Utility::HasAnnotation<CVar::ArchiveTag>(member)) {
						flags |= CVarFlags::eArchive;
					}

					if constexpr (Utility::HasAnnotation<CVar::CheatTag>(member)) {
						flags |= CVarFlags::eCheat;
					}

					if constexpr (Utility::HasAnnotation<CVar::ReadOnlyTag>(member)) {
						flags |= CVarFlags::eReadOnly;
					}

					double defaultValue = 0.0;
					if constexpr (std::is_enum_v<M>) {
						defaultValue = static_cast<double>(static_cast<std::underlying_type_t<M>>(defaults.[:member:]));
					}
					else {
						defaultValue = static_cast<double>(defaults.[:member:]);
					}

					std::string joined;
					if (!T::s_Category.empty()) {
						joined += T::s_Category;
						joined += '.';
					}

					joined += std::meta::identifier_of(member);

					out[i] = CVarDesc{
						.m_Name = std::define_static_string(joined),
						.m_Help = std::define_static_string(help.View()),
						.m_Field = std::define_static_string(std::meta::identifier_of(member)),
						.m_Struct = structName,
						.m_Enumerators = [] {
							if constexpr (std::is_enum_v<M>) { return std::span<const EnumEntry>(EnumTable<M>); }
							else                             { return std::span<const EnumEntry>{}; }
						}(),
						.m_OnChange = Utility::AnnotationOr<CVar::OnChange>(member, CVar::OnChange{}).m_Fn,
						.m_Default = defaultValue,
						.m_Min = range.m_Min,
						.m_Max = range.m_Max,
						.m_Offset = static_cast<uint32_t>(std::meta::offset_of(member).bytes),
						.m_Type = StorageTypeOf<M>(),
						.m_Flags = flags,
						.m_Tier = tier,
						.m_HasRange = range.m_Min != range.m_Max
					};

					i++;
				}
				return out;
			}

			/**
			 * @brief Links a settings struct into the console registry.
			 * @details Defined in Console.cpp so this header stays free of the registry, which lets a settings
			 * struct be declared anywhere without dragging the console in with it.
			 */
			void RegisterCVarBlock(std::span<const CVarDesc> schema, void* live, uint64_t liveSize, std::string_view structName, ApplyTier tier);
		}

		template <IsSettingsStruct T>
		inline constexpr auto SettingsSchema = Internal::MakeSchema<T>();

		/**
		 * @brief Registers a settings struct instance with the console when it is constructed.
		 * @details Only ever used through CORI_SETTINGS. It captures the address of the instance, never its value,
		 * so it does not care whether the instance has been initialized yet.
		 */
		template <IsSettingsStruct T>
		class SettingsRegistrar {
		public:
			explicit SettingsRegistrar(T* instance) {
				Internal::RegisterCVarBlock(SettingsSchema<T>, instance, sizeof(T), SettingsSchema<T>.empty() ? std::string_view{} : SettingsSchema<T>.front().m_Struct, Internal::StructTier<T>());
			}

			SettingsRegistrar(const SettingsRegistrar&) = delete;
			SettingsRegistrar& operator=(const SettingsRegistrar&) = delete;
			SettingsRegistrar(SettingsRegistrar&&) = delete;
			SettingsRegistrar& operator=(SettingsRegistrar&&) = delete;
		};
	}
}

/**
 * @brief Defines a settings struct instance and registers every one of its fields with the console.
 * @details Both the instance and its registrar have static storage duration, so the fields are addressable by
 * name from the moment the program starts. The instance is constant initialized, the registrar only stores its
 * address, so neither depends on the other's initialization order.
 */
#define CORI_SETTINGS(Type, Name) \
	inline Type Name{}; \
	inline const ::Cori::Core::SettingsRegistrar<Type> CORI_CONCAT(s_CVarRegistrar_, Name){ &Name }
