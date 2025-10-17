#pragma once

namespace Cori {
	namespace Core {
		/**
		 * @brief This is my packed/dense array custom implementation.
		 * @tparam T Type to be stored in an array.
		 * @tparam SizeT Type of size/indexes. Should be an unsigned integer.
		 * @tparam MaxSize Maximum pack array capacity.
		 * @note I tried mimicking the stl API so should be intuitive.
		 */
		template<typename T, std::unsigned_integral SizeT, SizeT MaxSize> requires std::equality_comparable<T> && std::movable<T>
		class PackedArray {
		public:
			using iterator = typename std::array<T, MaxSize>::iterator;
			using const_iterator = typename std::array<T, MaxSize>::const_iterator;

			PackedArray() = default;

			bool push_back(const T& value) {
				if (m_Size >= MaxSize) {
					return false;
				}
				m_Data[m_Size] = value;
				++m_Size;
				return true;
			}

			template<typename... Args> requires std::constructible_from<T, Args...>
			T& emplace(Args&&... args) {
				if (full()) {
					throw std::length_error("Cannot emplace into a full PackedArray");
				}

				std::construct_at(&m_Data[m_Size], std::forward<Args>(args)...);

				return m_Data[m_Size++];
			}

			bool remove(const T& value) {
				if (m_Size == 0) {
					return false;
				}

				auto it = std::find(begin(), end(), value);
				if (it == end()) {
					return false;
				}

				if (it != end() - 1) {
					*it = std::move(m_Data[m_Size - 1]);
				}

				--m_Size;

				if constexpr (!std::is_trivially_destructible_v<T>) {
					std::destroy_at(&m_Data[m_Size]);
				}

				return true;
			}

			bool remove(const SizeT index) {
				if (index <= m_Size) {
					return remove(m_Data[index]);
				}

				return false;
			}

			iterator begin() { return m_Data.begin(); }
			const_iterator cbegin() const { return m_Data.cbegin(); }

			iterator end() { return m_Data.begin() + m_Size; }
			const_iterator cend() const { return m_Data.cbegin() + m_Size; }

			[[nodiscard]] SizeT size() const {
				return m_Size;
			}

			[[nodiscard]] constexpr SizeT capacity() const {
				return MaxSize;
			}

			[[nodiscard]] bool empty() const {
				return m_Size == 0;
			}

			[[nodiscard]] bool full() const {
				return m_Size >= MaxSize;
			}

			[[nodiscard]] T& operator[](SizeT index) {
				return m_Data[index];
			}
			const T& operator[](SizeT index) const {
				return m_Data[index];
			}

			[[nodiscard]] T& at(SizeT index) {
				if (index >= m_Size) {
					throw std::out_of_range("PackedArray index out of range");
				}
				return m_Data[index];
			}

			const T& at(SizeT index) const {
				if (index >= m_Size) {
					throw std::out_of_range("PackedArray index out of range");
				}
				return m_Data[index];
			}

			void clear() {
				m_Size = 0;
			}

		private:
			std::array<T, MaxSize> m_Data{};
			SizeT m_Size{ 0 };
		};
	}
}