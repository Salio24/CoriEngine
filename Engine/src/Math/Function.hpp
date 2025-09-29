#pragma once
#include <exprtk.hpp>

namespace Cori {
	namespace Math {
		template<typename T>
		concept IsNumber = std::is_arithmetic_v<T>;

		template<IsNumber NumericType, uint8_t ArgumentAmount>
		class Function {
		public:
			Function() {
				m_Data = std::make_unique<Data>();
				NumericType e = static_cast<NumericType>(std::numbers::e);
				m_Data->m_SymbolTable.add_constant("e", e);
				m_Data->m_SymbolTable.add_constants();
			}

			~Function() = default;

			Function(Function&& other) noexcept = default;
			Function& operator=(Function&& other) noexcept = default;

			Function(const Function&) = delete;
			Function& operator=(const Function&) = delete;

			/**
			 * @brief Registers the names of the arguments for the function. This must be called before AddAlias or Parse.
			 * @param argNames Argument (value) names to register.
			 */
			void RegisterValues(const std::vector<std::string>& argNames) {
				if (argNames.size() != ArgumentAmount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "Size of vector argNames should match the specified argument amount '{}'.", argNames.size(), ArgumentAmount);
					return;
				}

				m_Data->m_ArgNames = argNames;

				for (uint8_t i = 0; i < ArgumentAmount; ++i) {
					m_Data->m_SymbolTable.add_variable(argNames[i], m_Data->m_ArgValues[i]);
				}
			}

			/**
			 * @brief Creates a parameterized alias (a composited function). The alias will accept the same parameters as registered in RegisterValues.
			 * @param aliasName Name to passing to the aliased function.
			 * @param aliasExpression Expression to parse and assign to the aliased function.
			 */
			void AddAlias(const std::string& aliasName, const std::string& aliasExpression) {
				auto aliasFunc = typename exprtk::function_compositor<NumericType>::function(aliasName);

				for (const auto& argName : m_Data->m_ArgNames) {
					aliasFunc.var(argName);
				}

				aliasFunc.expression(aliasExpression);

				if (!m_Data->m_Compositor.add(aliasFunc)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "Failed to create alias '{}'", aliasExpression);
					for (std::size_t i = 0; i < m_Data->m_Compositor.error_count(); ++i) {
						exprtk::parser_error::type error = m_Data->m_Compositor.get_error(i);
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "{}", error.diagnostic);
					}
					m_Valid = false;
				}
			}

			/**
			 * @brief Parses the main mathematical expression. Can use any aliases created with AddAlias.
			 * @param expression Main expression to parse.
			 */
			void Parse(const std::string& expression) {
				m_Data->m_Expression.register_symbol_table(m_Data->m_SymbolTable);

				static exprtk::parser<double> parser;
				if (parser.compile(expression, m_Data->m_Expression)) {
					m_Valid = true;
				} else {
					m_Valid = false;
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "Failed to parse main expression '{}'", expression);
					for (std::size_t i = 0; i < parser.error_count(); ++i) {
						exprtk::parser_error::type error = parser.get_error(i);
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "{}", error.diagnostic);
					}
				}
			}

			NumericType operator()(const IsNumber auto&... args) {
				if (!m_Valid) {
					return 1;
				}

				if (sizeof...(args) != ArgumentAmount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Math::Self, Logger::Tags::Math::Function }, "Amount of arguments '{}' provided to the () operator should be the same as the specified argument amount '{}'.", sizeof...(args), ArgumentAmount);
					return 1;
				}

				uint8_t index = 0;

				([&](const auto& arg) {
					m_Data->m_ArgValues[index++] = static_cast<NumericType>(arg);
				}(args), ...);

				NumericType result = m_Data->m_Expression.value();
				return result;
			}

			[[nodiscard]] bool Success() const {
				return m_Valid;
			}

		private:
			struct Data {
				std::array<NumericType, ArgumentAmount> m_ArgValues;
				std::vector<std::string> m_ArgNames;
				exprtk::symbol_table<NumericType> m_SymbolTable{};
				exprtk::expression<NumericType> m_Expression;
				exprtk::function_compositor<NumericType> m_Compositor;
				uint8_t m_ArgAmount = ArgumentAmount;

				Data() : m_Compositor(m_SymbolTable) {}
			};

			std::unique_ptr<Data> m_Data{ nullptr };
			bool m_Valid{ false };

		};
	}
}
