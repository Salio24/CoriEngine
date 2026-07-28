#pragma once

namespace Cori {
	namespace Core {
		template<typename S>
		concept IsSequentialStorage = requires(S& storage, const S& constStorage, typename S::SizeType count, uint64_t byteOffset, uint64_t byteSize) {
			typename S::ValueType;
			typename S::SizeType;

			{ storage[count] } -> std::same_as<typename S::ValueType&>;
			{ constStorage[count] } -> std::same_as<const typename S::ValueType&>;
			{ storage.Data() } -> std::same_as<typename S::ValueType*>;
			{ constStorage.Data() } -> std::same_as<const typename S::ValueType*>;
			{ constStorage.Capacity() } -> std::same_as<typename S::SizeType>;

			storage.Reallocate(count);
			storage.ReallocateNonReporting(count);
			storage.ReportChange(byteOffset, byteSize);
			storage.PoisonRange(count, count);
			storage.Sync();
		} && std::constructible_from<S, typename S::SizeType> && std::movable<S>;

		template<typename T, typename Allocator = std::allocator<T>>
		class GenericSequentialStorage {
		public:
			using ValueType = T;
			using SizeType = std::allocator_traits<Allocator>::size_type;

			using Reference = T&;
			using ConstReference = const T&;

			GenericSequentialStorage() = default;

			explicit GenericSequentialStorage(const SizeType capacity) {
				ReallocateNonReporting(capacity);
			}

			~GenericSequentialStorage() {
				ReleaseResources();
			}

			GenericSequentialStorage(const GenericSequentialStorage&) = delete;
			GenericSequentialStorage& operator=(const GenericSequentialStorage&) = delete;

			GenericSequentialStorage(GenericSequentialStorage&& other) noexcept {
				Steal(std::move(other));
			}

			GenericSequentialStorage& operator=(GenericSequentialStorage&& other) noexcept {
				if (this == &other) {
					return *this;
				}

				ReleaseResources();
				Steal(std::move(other));

				return *this;
			}

			[[nodiscard]] ConstReference operator[](const SizeType index) const {
				CORI_CORE_ASSERT(index < m_Capacity, "GenericSequentialStorage index '{}' out of bounds of '{}' elements.", index, m_Capacity);
				return m_Data[index];
			}

			[[nodiscard]] Reference operator[](const SizeType index) {
				CORI_CORE_ASSERT(index < m_Capacity, "GenericSequentialStorage index '{}' out of bounds of '{}' elements.", index, m_Capacity);
				return m_Data[index];
			}

			[[nodiscard]] T* Data() {
				return m_Data;
			}

			[[nodiscard]] const T* Data() const {
				return m_Data;
			}

			[[nodiscard]] SizeType Capacity() const {
				return m_Capacity;
			}

			void Sync() {}

			void ReportChange([[maybe_unused]] const uint64_t startOffset, [[maybe_unused]] const uint64_t size) {}

			void Reallocate(const SizeType newCapacity) {
				ReallocateNonReporting(newCapacity);
			}

			void ReallocateNonReporting(const SizeType newCapacity) {
				if (newCapacity == m_Capacity) {
					return;
				}

				T* newData = newCapacity != 0 ? std::allocator_traits<Allocator>::allocate(m_Allocator, newCapacity) : nullptr;

				const SizeType carriedOverCount = std::min(m_Capacity, newCapacity);

				if (m_Data) {
					if (carriedOverCount != 0) {
						std::memcpy(newData, m_Data, carriedOverCount * sizeof(T));
					}

					std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
				}

				m_Data = newData;
				m_Capacity = newCapacity;

				PoisonRange(carriedOverCount, newCapacity - carriedOverCount);
			}

			void PoisonRange([[maybe_unused]] const SizeType start, [[maybe_unused]] const SizeType count) {
				#ifdef DEBUG_BUILD
				if (count == 0) {
					return;
				}

				CORI_CORE_ASSERT(start + count <= m_Capacity, "Range '{}' to '{}' passed to PoisonRange of GenericSequentialStorage is out of the storage bounds of '{}' elements.", start, start + count, m_Capacity);

				const uint64_t startByte = start * sizeof(T);
				const uint64_t byteCount = count * sizeof(T);

				auto* bytes = reinterpret_cast<Byte*>(m_Data);
				const auto* pattern = reinterpret_cast<const Byte*>(&s_PoisonValue);

				for (uint64_t i = startByte; i < startByte + byteCount; i++) {
					bytes[i] = pattern[i % sizeof(s_PoisonValue)];
				}
				#endif
			}

		private:
			void ReleaseResources() {
				if (m_Data) {
					std::allocator_traits<Allocator>::deallocate(m_Allocator, m_Data, m_Capacity);
				}
			}

			void Steal(GenericSequentialStorage&& other) noexcept {
				m_Allocator = std::move(other.m_Allocator);
				m_Data = other.m_Data;
				other.m_Data = nullptr;
				m_Capacity = other.m_Capacity;
				other.m_Capacity = 0;
			}

			[[no_unique_address]] Allocator m_Allocator{};
			T* m_Data{ nullptr };
			SizeType m_Capacity{ 0 };
		};
	}
}
