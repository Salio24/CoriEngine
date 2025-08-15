#pragma once
#include "Core/Utility/TemplateUtils.hpp"

namespace Cori {
	template <typename... Types>
	concept AllAreExceptions = (std::derived_from<Types, std::exception> && ...);

	template <typename... DeclaredTypes>
	class CoriError final : public std::exception {
	public:
		template <typename... Args>
		explicit CoriError(std::string message, Args&&... args) : m_ErrorMessage(std::move(message)) {
			static_assert(sizeof...(Args) == 2 * sizeof...(DeclaredTypes), "Incorrect number of arguments provided. Expected a description and a value for each declared type.");

			m_Descriptions.reserve(sizeof...(DeclaredTypes));
			m_Payloads.reserve(sizeof...(DeclaredTypes));

			ProcessArgs<DeclaredTypes...>(std::forward<Args>(args)...);
		}

		~CoriError() override {
			if (!m_Seen) {
				CORI_CORE_ERROR_TAGGED({"Unlogged Error"}, "A CoriError was never logged, logging it now. Error: '{}'", what());
			}
		}

		const char* what() const noexcept override {
			if (!m_Seen) {
				std::stringstream ss;
				ss << m_ErrorMessage;

				if (!m_Payloads.empty()) {
					ss << " | additional data: (";
					bool first = true;

					for (auto&& [description, payload] : std::views::zip(m_Descriptions, m_Payloads)) {
						if (!first) { ss << ", "; }
						ss << description << ": ";
						payload.print(ss, payload.m_Value);
						first = false;
					}

					ss << ")";
				}
				m_WhatMessage = ss.str();
				m_Descriptions.clear();
				m_ErrorMessage.clear();
				m_Descriptions.shrink_to_fit();
				m_ErrorMessage.shrink_to_fit();
				m_Seen = true;
			}
			return m_WhatMessage.c_str();
		}

		template <typename T>
		T Get() const {
			static_assert(Utility::IsInPack<T, DeclaredTypes...>, "Error: Attempting to Get<T> a type T that was not declared in the CoriError<TypeIsAnalogInPacks...> specialization.");
			for (const auto& payload : m_Payloads) {
				if (payload.m_Value.type() == typeid(T)) {
					return std::any_cast<T>(payload.m_Value);
				}
			}

			throw std::runtime_error("Type was declared for CoriError but not provided in this specific error instance."); // this should be unreachable, but i will still leave it here just in case
		}

		template <typename... Types> requires (sizeof...(Types) > 1)
		std::tuple<Types...> Get() const {
			return std::make_tuple(Get<Types>()...);
		}

	private:
		static_assert(!Utility::HasDuplicates<DeclaredTypes...>, "CoriError cannot be instantiated with duplicate types in its template parameter list. Each retrievable type must be unique.");

		struct DataPayload {
			std::any m_Value;
			void (*print)(std::ostream&, const std::any&){};
		};

		mutable std::string m_ErrorMessage{};
		mutable std::vector<DataPayload> m_Payloads;
		mutable std::vector<std::string> m_Descriptions;
		mutable std::string m_WhatMessage{};
		mutable bool m_Seen{false};

		template <typename DeclaredT, typename... RestT, typename ValT, typename... RestArgs>
		void ProcessArgs(const std::string& description, ValT&& value, RestArgs&&... restArgs) {
			static_assert(std::is_constructible_v<DeclaredT, ValT>, "A declared error type cannot be constructed from its provided value.");

			m_Descriptions.emplace_back(description);
			m_Payloads.emplace_back(DataPayload{
					DeclaredT(std::forward<ValT>(value)),
					[](std::ostream& os, const std::any& a) { os << std::any_cast<const DeclaredT&>(a); }
				});

			if constexpr (sizeof...(RestT) > 0) {
				ProcessArgs<RestT...>(std::forward<RestArgs>(restArgs)...);
			}
		}

		void ProcessArgs() {}
	};

	template <typename... Errors> requires AllAreExceptions<Errors...>
	class PossibleErrors {
	public:
		template <typename E> requires(std::is_same_v<std::decay_t<E>, Errors> || ...)
		PossibleErrors(E&& e) : m_Variant(std::forward<E>(e)) {} // NOLINT (to make clang-tidy happy :) ) constructor should be implicid for std::unexpected to work properly

		~PossibleErrors() {
			if (!m_ErrorHandled) {
				std::visit([](const auto& e) {
					CORI_CORE_ERROR_TAGGED({"Unused Error"}, "An error inside PossibleErrors was never used, logging it now. Error: '{}'", e.what());
				}, m_Variant);
			}
		}

		PossibleErrors(const PossibleErrors& other) = default;

		PossibleErrors& operator=(const PossibleErrors& other) = default;

		PossibleErrors(PossibleErrors&& other) noexcept : m_Variant(std::move(other.m_Variant)), m_ErrorHandled(other.m_ErrorHandled) {
			other.m_ErrorHandled = true;
		}

		PossibleErrors& operator=(PossibleErrors&& other) noexcept = default;

		[[nodiscard]] const std::exception* GetRaw() const {
			m_ErrorHandled = true;
			return std::visit([](const auto& e) -> const std::exception* {
				return &e;
			}, m_Variant);
		}

		template <typename Visitor>
		void Visit(Visitor&& visitor) const & {
			m_ErrorHandled = true;
			std::visit(std::forward<Visitor>(visitor), m_Variant);
		}

		template <typename Visitor>
		void Visit(Visitor&& visitor) const && {
			m_ErrorHandled = true;
			std::visit(std::forward<Visitor>(visitor), std::move(m_Variant));
		}

		template <typename SpecificError, typename Func>
		PossibleErrors& On(Func&& handler) & {
			auto* e = std::get_if<SpecificError>(&m_Variant);
			if (e) {
				handler(*e);
				m_ErrorHandled = true;
			}
			return *this;
		}

		template <typename SpecificError, typename Func>
		PossibleErrors&& On(Func&& handler) && {
			auto* e = std::get_if<SpecificError>(&m_Variant);
			if (e) {
				handler(*e);
				m_ErrorHandled = true;
			}
			return std::move(*this);
		}

		void JustLog() const {
			CORI_CORE_ERROR("Logging an error inside of PossibleErrors. Error: '{}'", GetRaw()->what());
		}

		const char* GetWhat() const {
			return GetRaw()->what();
		}

	private:
		std::variant<Errors...> m_Variant;
		mutable bool m_ErrorHandled{false};
	};
}
