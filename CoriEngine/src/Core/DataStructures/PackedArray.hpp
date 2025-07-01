#pragma once

namespace Cori {
	template<typename T, std::unsigned_integral SizeT, SizeT MaxSize>
		requires std::equality_comparable<T> && std::movable<T>
	class PackedArray {
	public:
		using iterator = typename std::array<T, MaxSize>::iterator;
		using const_iterator = typename std::array<T, MaxSize>::const_iterator;

		PackedArray() = default;

		bool add(const T& value) {
			if (m_Size >= MaxSize) {
				return false;
			}
			m_Data[m_Size] = value;
			m_Size++;
			return true;
		}

		template<typename... Args>
			requires std::constructible_from<T, Args...>
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

			m_Size--;

			if constexpr (!std::is_trivially_destructible_v<T>) {
				std::destroy_at(&m_Data[m_Size]);
			}

			return true;
		}

		iterator begin() { return m_Data.begin(); }
		const_iterator cbegin() const { return m_Data.cbegin(); }

		iterator end() { return m_Data.begin() + m_Size; }
		const_iterator cend() const { return m_Data.cbegin() + m_Size; }

		SizeT size() const {
			return m_Size;
		}

		constexpr SizeT capacity() const {
			return MaxSize;
		}

		bool empty() const {
			return m_Size == 0;
		}

		bool full() const {
			return m_Size >= MaxSize;
		}

		T& operator[](SizeT index) {
			return m_Data[index];
		}
		const T& operator[](SizeT index) const {
			return m_Data[index];
		}

		T& at(SizeT index) {
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