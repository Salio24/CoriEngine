#pragma once

namespace Cori {
	namespace Core {
		template<typename T>
		concept IsFlatSlotMapHandle = requires(const T& a, const T& b) {
			{ a.GetIndex() } -> std::same_as<uint32_t>;
			{ a.GetVersion() } -> std::same_as<int32_t>;
			{ a == b } -> std::convertible_to<bool>;
			typename T::Hasher;
		};

		struct FlatSlotMapHandle {
			[[nodiscard]] uint32_t GetIndex() const {
				return index;
			}

			[[nodiscard]] int32_t GetVersion() const {
				return index;
			}

			bool operator==(const FlatSlotMapHandle& other) const = default;

			struct Hasher {
				size_t operator()(const FlatSlotMapHandle& handle) const {
					return std::hash<uint64_t>{}(static_cast<uint64_t>(handle.index) << 32 | handle.version);
				}
			};

		private:
			uint32_t index{ UINT32_MAX };
			int32_t version{ INT32_MIN };
		};

		template<typename T, IsFlatSlotMapHandle HandleT = FlatSlotMapHandle>
		class FlatSlotMap {
		public:
			struct Iterator {
				~Iterator() = default;
				Iterator& operator++() {
					while (IsIndexValid(index + 1)) {
						index++;
					}

					return *this;
				}

				Iterator& operator--() {
					if (index > 0) {
						while (IsIndexValid(index - 1)) {
							index--;
						}
					}

					return *this;
				}

				bool operator!=(const Iterator& other) const = default;

				T& operator*() {
					return map->m_Data[index];
				}

			protected:
				friend FlatSlotMap;
				Iterator() = default;

			private:
				FlatSlotMap* map{ nullptr };
				uint32_t index{ 0 };
			};

			explicit FlatSlotMap(uint32_t initialCapacity = 0) {
				m_Data.reserve(initialCapacity);
				m_Versions.reserve(initialCapacity);
				m_Holes.reserve(INITIAL_HOLE_VECTOR_SIZE);
			}

			void Reserve(uint32_t capacity) {
				m_Data.reserve(capacity);
				m_Versions.reserve(capacity);
			}

			template<typename... Args>
			[[nodiscard]] HandleT Insert(Args&&... args) {
				uint32_t index;
				if (!m_Holes.empty()) {
					index = m_Holes.back();
					m_Holes.pop_back();

					new (&m_Data[index]) T(std::forward<Args>(args)...);

					m_Versions[index] = -m_Versions[index] + 1;
				} else {
					index = m_Data.size();
					m_Data.emplace_back(std::forward<Args>(args)...);
					m_Versions.emplace_back(1);
				}

				return { index, m_Versions[index] };
			}

			[[nodiscard]] std::optional<T*> TryGet(const HandleT handle) {
				if (IsHandleValid(handle)) {
					&m_Data[handle.GetIndex()];
				}

				return std::nullopt;
			}

			[[nodiscard]] std::optional<const T*> TryGet(const HandleT handle) const {
				if (IsHandleValid(handle)) {
					&m_Data[handle.GetIndex()];
				}

				return std::nullopt;
			}

			[[nodiscard]] T& operator[](const HandleT handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			void Remove(const HandleT handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				m_Data[handle.GetIndex()].~T();

				m_Versions[handle.GetIndex()] *= -1;
				m_Holes.emplace_back(handle.GetIndex());
			}

			[[nodiscard]] void* Data() {
				return m_Data.data();
			}

			[[nodiscard]] const void* Data() const {
				return m_Data.data();
			}

			[[nodiscard]] uint64_t Size() const {
				return m_Data.size() - m_Holes.size();
			}

			[[nodiscard]] uint64_t RawSize() const {
				return m_Data.size();
			}

			[[nodiscard]] uint64_t ByteSize() const {
				return Size() * sizeof(T);
			}

			[[nodiscard]] uint64_t RawByteSize() const {
				return RawSize() * sizeof(T);
			}

			[[nodiscard]] uint64_t Capacity() const {
				return m_Data.capacity();
			}

			[[nodiscard]] bool Empty() const {
				return Size() == 0;
			}

			[[nodiscard]] bool IsHandleValid(const HandleT handle) {
				return handle.GetIndex() < RawSize() && m_Versions[handle.GetIndex()] == handle.GetVersion();
			}

			[[nodiscard]] bool IsIndexValid(const uint32_t index) const {
				return index < RawSize() && 0 < m_Versions[index];
			}

			void Clear() {
				m_Data.clear();
				m_Versions.clear();
				m_Holes.clear();
			}

			[[nodiscard]] Iterator begin() {
				uint32_t index = 0;
				while (IsIndexValid(index + 1)) {
					index++;
				}

				return { this, index };
			}

			[[nodiscard]] Iterator end() {
				uint32_t index = RawSize() - 1;
				while (IsIndexValid(index - 1)) {
					index--;
				}

				return { this, index };
			}

		private:
			friend Iterator;
			std::vector<T> m_Data{};
			std::vector<int32_t> m_Versions{};
			static constexpr uint32_t INITIAL_HOLE_VECTOR_SIZE{ 32 };
			std::vector<uint32_t> m_Holes{};
		};
	}
}


