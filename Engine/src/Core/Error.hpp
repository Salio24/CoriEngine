#ifndef ERROR_H
#define ERROR_H
#include "Logger.hpp"
#include "Utility/TemplateUtils.hpp"

namespace Cori {
	namespace Core {
		/**
		 * @brief All types from Types pack are std::exception or derived from it.
		 */
		template <typename... Types>
		concept AllAreExceptions = (std::derived_from<Types, std::exception> && ...);

		/**
		 * @brief Custom error class mainly used in std::expected.
		 * @tparam DeclaredTypes Types that will be obtainable.
		 * @details Can hold variables that are passed upon creation that are later obtainable via Get method, duplicate types are illegal. As it is derived from std::exception it can be thrown.
		 * \n If a CoriError object was constructed and never 'seen' it will print the formated error message upon destruction.
		 * \n DeclaredTypes typenames will be embedded into the formated error message alongside the user-defined description for each one, and if they are streamable their value will be embedded as well.
		 * \n Example usage:
		 * \n Example specialization: CoriError<int, std::type_index>
		 * \n Constructed like this: CoriError<int, std::type_index>("Main error message", "user-defined description for int", intValue, "user-defined description for std::type_index", type_indexValue);
		 * \n The final error message will be: Main error message | Additional data: (user-defined description for int (int): 'intValue', user-defined description for std::type_index (std::type_index): 'Type is not streamable, you can retrieve it with Get<T>()') |
		 * \n Also all the types from DeclaredTypes can be later retried using Get<T>(), and it also supports retrieving multiple at a time and returning them in a tuple.
		 */
		template <typename... DeclaredTypes>
		class CoriError final : public std::exception {
		public:
			/**
			 * @brief Constructs CoriError object and formats the final error message.
			 * @tparam Args Deduced automatically, no need to specify.
			 * @param message Main error message.
			 * @param args Pairs descriptions and values for each DeclaredTypes.
			 */
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

			/**
			 * @brief Returns the formated message and if called the CoriError is considered 'seen'.
			 * @return Formated error message.
			 */
			const char* what() const noexcept override {
				if (!m_Seen) {
					m_Seen = true;
				}
				return m_Message.c_str();
			}

			/**
			 * @brief Explicitly sets the CoriError object into 'seen' state.
			 */
			void Ignore() const {
				m_Seen = true;
			}

			/**
			 * @brief Retrieves the value of the type requested from the CoriError object.
			 * @tparam T Type to retrieve.
			 * @return Value associated with the type provided.
			 */
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

			/**
			 * @brief The overload of Get method for use with structured bindings.
			 * @tparam Types A pack of types to retrieve.
			 * @return A tuple containing the values requested.
			 */
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

				if constexpr (Utility::IsStreamable<DeclaredT>) {
					ss << description << " (" << CORI_CLEAN_TYPE_NAME(DeclaredT) << "): '" << value << "'";
				} else {
					ss << description << " (" << CORI_CLEAN_TYPE_NAME(DeclaredT) << "): 'Type is not streamable, you can retrieve it with Get<T>()'";
				}

				if constexpr (sizeof...(RestArgs) > 0) {
					ss << ", ";
				}

				m_Payloads.emplace_back(DeclaredT(std::forward<ValT>(value)));

				if constexpr (sizeof...(RestT) > 0) {
					ProcessArgs<RestT...>(ss, std::forward<RestArgs>(restArgs)...);
				}
			}
		};

		/**
		 * @brief This class utilizes std::variant to hold one of the possible exception types.
		 * @tparam Exceptions Possible exceptions. Should be either std::expected or derived from it.
		 */
		template <typename... Exceptions> requires AllAreExceptions<Exceptions...>
		class PossibleErrors {
		public:
			template <typename E> requires(std::is_same_v<std::decay_t<E>, Exceptions> || ...)
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

			/**
			 * @brief Returns a pointer to the exception held inside of the object.
			 * @return Pointer to the exception.
			 */
			[[nodiscard]] const std::exception* GetRaw() const {
				m_ErrorHandled = true;
				return std::visit([](const auto& e) -> const std::exception* {
					return &e;
				}, m_Variant);
			}


			/**
			 * @brief Visits the variant inside of the object with a callable.
			 * @param visitor A callable that is valid with any of the possible errors stored.
			 */
			template <typename Visitor>
			void Visit(Visitor&& visitor) const & {
				m_ErrorHandled = true;
				std::visit(std::forward<Visitor>(visitor), m_Variant);
			}

			/**
			 * @brief Visits the variant inside of the object with a callable.
			 * @param visitor A callable that accepts and is valid with any of the possible errors stored.
			 */
			template <typename Visitor>
			void Visit(Visitor&& visitor) const && {
				m_ErrorHandled = true;
				std::visit(std::forward<Visitor>(visitor), std::move(m_Variant));
			}

			/**
			 * @brief Handles the exception with a callable if it is a specific exception type.
			 * @tparam SpecificException Exception type the method will handle.
			 * @param handler A callable function that accepts SpecificException.
			 * @note This can be used as a monadic method.
			 */
			template <typename SpecificException, typename Func>
			PossibleErrors& On(Func&& handler) & {
				auto* e = std::get_if<SpecificException>(&m_Variant);
				if (e) {
					handler(*e);
					m_ErrorHandled = true;
				}
				return *this;
			}

			/**
			 * @brief Handles the exception with a callable if it is a specific exception type.
			 * @tparam SpecificException Exception type the method will handle.
			 * @param handler A callable function that accepts SpecificException.
			 * @note This can be used as a monadic method.
			 */
			template <typename SpecificException, typename Func>
			PossibleErrors&& On(Func&& handler) && {
				auto* e = std::get_if<SpecificException>(&m_Variant);
				if (e) {
					handler(*e);
					m_ErrorHandled = true;
				}
				return std::move(*this);
			}


			/**
			 * @brief Simply returns the formated error message held inside the execution object.
			 * @return Formated error message.
			 */
			const char* GetWhat() const {
				return GetRaw()->what();
			}

		private:
			std::variant<Exceptions...> m_Variant;
			mutable bool m_ErrorHandled{ false };
		};
	}
}

#endif
