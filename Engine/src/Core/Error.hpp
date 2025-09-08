#ifndef ERROR_H
#define ERROR_H
#include "Logger.hpp"
#include "Utility/TemplateUtils.hpp"

namespace Cori {
	namespace Core {
		template <typename... Types>
		concept AllAreExceptions = (std::derived_from<Types, std::exception> && ...);

		template <typename... DeclaredTypes>
		class CoriError final : public std::exception {
		public:
			template <typename... Args>
			explicit CoriError(const std::string& message, Args&&... args) {
				static_assert(sizeof...(Args) == 2 * sizeof...(DeclaredTypes), "Incorrect number of arguments provided. Expected a description and a value for each declared type.");

				m_Payloads.reserve(sizeof...(DeclaredTypes));

				std::stringstream ss;
				ss << message;

				if constexpr (sizeof...(Args) > 0) {
					ss << " | Additional data: (";
					ProcessArgs<DeclaredTypes...>(ss, std::forward<Args>(args)...);
					ss << ") |";
				}

				m_Message = ss.str();
			}

			~CoriError() override {
				if (!m_Seen) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::UnusedError }, "A CoriError was never logged, logging it now. Error: '{}'", what());
				}
			}

			CoriError(const CoriError& other) noexcept : m_Payloads(other.m_Payloads), m_Message(other.m_Message), m_Seen(other.m_Seen) {
				other.m_Seen = true;
			}

			CoriError(CoriError&& other) noexcept : m_Payloads(std::move(other.m_Payloads)), m_Message(std::move(other.m_Message)), m_Seen(other.m_Seen)  {
				other.m_Seen = true;
			}

			CoriError& operator=(const CoriError& other) = delete;
			CoriError& operator=(CoriError&& other) noexcept = delete;

			const char* what() const noexcept override {
				if (!m_Seen) {
					m_Seen = true;
				}
				return m_Message.c_str();
			}

			void ignore() const noexcept {
				m_Seen = true;
			}

			template <typename T>
			T Get() const {
				static_assert(Utility::IsInPack<T, DeclaredTypes...>, "Error: Attempting to Get<T> a type T that was not declared in the CoriError<TypeIsAnalogInPacks...> specialization.");
				for (const auto& payload : m_Payloads) {
					if (payload.type() == typeid(T)) {
						return std::any_cast<T>(payload);
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

			mutable std::vector<std::any> m_Payloads;
			mutable std::string m_Message;
			mutable bool m_Seen{ false };

			// ReSharper disable once CppMemberFunctionMayBeStatic
			void ProcessArgs([[maybe_unused]] std::stringstream& ss) {}

			template <typename...>
			// ReSharper disable once CppMemberFunctionMayBeStatic
			void ProcessArgs([[maybe_unused]] std::stringstream& ss) {}

			template <typename DeclaredT, typename... RestT, typename ValT, typename... RestArgs>
			void ProcessArgs(std::stringstream& ss, const std::string& description, ValT&& value, RestArgs&&... restArgs) {
				static_assert(std::is_constructible_v<DeclaredT, ValT>, "A declared error type cannot be constructed from its provided value.");

				ss << description << "(" << CORI_CLEAN_TYPE_NAME(DeclaredT) << "): '" << value << "'";

				if constexpr (sizeof...(RestArgs) > 0) {
					ss << ", ";
				}

				m_Payloads.emplace_back(DeclaredT(std::forward<ValT>(value)));

				if constexpr (sizeof...(RestT) > 0) {
					ProcessArgs<RestT...>(ss, std::forward<RestArgs>(restArgs)...);
				}
			}
		};

		template <typename... Errors> requires AllAreExceptions<Errors...>
		class PossibleErrors {
		public:
			template <typename E> requires(std::is_same_v<std::decay_t<E>, Errors> || ...)
			PossibleErrors(E&& e) : m_Variant(std::forward<E>(e)) {} // NOLINT (to make clang-tidy happy :) ) constructor should be implicid for std::unexpected to work properly

			~PossibleErrors() {
				if (!m_ErrorHandled) {
					std::visit([](const auto& e) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::UnusedError }, "An error inside PossibleErrors was never used, logging it now. Error: '{}'", e.what());
					}, m_Variant);
				}
			}

			PossibleErrors(const PossibleErrors& other) noexcept : m_Variant(other.m_Variant), m_ErrorHandled(other.m_ErrorHandled) {
				other.m_ErrorHandled = true;
			}

			PossibleErrors(PossibleErrors&& other) noexcept : m_Variant(std::move(other.m_Variant)), m_ErrorHandled(other.m_ErrorHandled) {
				other.m_ErrorHandled = true;
			}

			PossibleErrors& operator=(const PossibleErrors& other) = delete;
			PossibleErrors& operator=(PossibleErrors&& other) noexcept = delete;

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
}

#endif
