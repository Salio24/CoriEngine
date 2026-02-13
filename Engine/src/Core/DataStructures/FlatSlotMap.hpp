#pragma once

namespace Cori {
	namespace Core {
		template<typename T>
		concept IsVersionedHandle = requires(const T& a, const T& b) {
			{ a.GetIndex() } -> std::same_as<uint32_t>;
			{ a.GetVersion() } -> std::same_as<uint32_t>;
			{ a == b } -> std::convertible_to<bool>;
			typename T::Hasher;
		};

		struct VersionedHandleBase {
			[[nodiscard]] uint32_t GetIndex() const {
				return index;
			}

			[[nodiscard]] uint32_t GetVersion() const {
				return version;
			}

			[[nodiscard]] bool operator==(const VersionedHandleBase& other) const = default;

			struct Hasher {
				size_t operator()(const VersionedHandleBase& handle) const {
					return std::hash<uint64_t>{}(static_cast<uint64_t>(handle.index) << 32 | handle.version);
				}
			};

		private:
			uint32_t index{ UINT32_MAX };
			uint32_t version{ 0 };
		};

		template<typename T>
		struct Handle : VersionedHandleBase {
			using Type = T;
		};

		template<std::copy_constructible T, IsVersionedHandle HandleT = Handle<T>, uint16_t REUSE_THRESHOLD = 64>
		class FlatSlotMap {
		public:
			using Handle = HandleT;

			using SizeT = uint32_t;

			template<bool IsConst>
			struct IteratorImpl {
				using iterator_category = std::bidirectional_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = T;
				using pointer = typename std::conditional<IsConst, typename std::vector<T>::const_pointer, typename std::vector<T>::pointer>::type;
				using reference = typename std::conditional<IsConst, typename std::vector<T>::const_reference, typename std::vector<T>::reference>::type;
				using MapType = typename std::conditional<IsConst, const FlatSlotMap, FlatSlotMap>::type;

				IteratorImpl& operator++() {
					SkipForward();
					return *this;
				}

				IteratorImpl operator++(int32_t) {
					IteratorImpl tmp = *this;
					++(*this);
					return tmp;
				}

				IteratorImpl& operator--() {
					SkipBackwards();
					return *this;
				}

				IteratorImpl operator--(int32_t) {
					IteratorImpl tmp = *this;
					--(*this);
					return tmp;
				}

				template<bool OtherIsConst>
				[[nodiscard]] bool operator==(const IteratorImpl<OtherIsConst>& other) const {
					return m_Index == other.m_Index && m_Map == other.m_Map;
				}
				template<bool OtherIsConst>
				[[nodiscard]] bool operator!=(const IteratorImpl<OtherIsConst>& other) const {
					return !(*this == other);
				}

				[[nodiscard]] reference operator*() const {
					return (*m_Map)[m_Index];
				}

				[[nodiscard]] pointer operator->() const {
					return &(*m_Map)[m_Index];
				}

				[[nodiscard]] Handle GetHandle() const {
					return m_Map->GetIndexHandle(m_Index);
				}

				MapType* m_Map;

			protected:
				friend FlatSlotMap;
				uint32_t m_Index;
				IteratorImpl(MapType* m, const uint32_t index) : m_Map(m), m_Index(index) {}

			private:
				void SkipForward() {
					sul::dynamic_bitset<>::size_type nextIndex = m_Map->m_SlotStates.find_next(m_Index);

					if (nextIndex == sul::dynamic_bitset<>::npos) {
						m_Index = static_cast<uint32_t>(m_Data.Capacity());
						return;
					}

					m_Index = nextIndex;
				}

				void SkipBackwards() {
					sul::dynamic_bitset<>::size_type prevIndex = m_Map->m_SlotStates.find_prev(m_Index);

					if (prevIndex != sul::dynamic_bitset<>::npos) {
						m_Index = prevIndex;
					}
				}
			};

			using Iterator = IteratorImpl<false>;
			using ConstIterator = IteratorImpl<true>;
			using Reference = typename std::vector<T>::reference;
			using ConstReference = typename std::vector<T>::const_reference;
			using ReverseIterator = std::reverse_iterator<Iterator>;
			using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

			FlatSlotMap() = default;

			explicit FlatSlotMap(const SizeT capacity) {
				Reserve(capacity);
			}

			FlatSlotMap(const FlatSlotMap&) = delete;
			FlatSlotMap& operator=(const FlatSlotMap&) = delete;

			FlatSlotMap(FlatSlotMap&& other) = default;
			FlatSlotMap& operator=(FlatSlotMap&& other) = default;

			void Reserve(SizeT capacity) {
				m_Data.reserve(capacity);
				m_Versions.reserve(capacity);
				m_SlotStates.reserve(capacity);
			}

			template<typename... Args>
			[[nodiscard]] Handle Emplace(Args&&... args) {
				SizeT index;

				const bool thresholdMet = m_Holes.size() > REUSE_THRESHOLD;
				const bool clearInProgress = m_ReusedIndexCounter > 0;

				if (thresholdMet || clearInProgress) {
					if (thresholdMet && !clearInProgress) {
						m_ReusedIndexCounter = m_Holes.size();
					}
					else {
						m_ReusedIndexCounter--;
					}

					index = m_Holes.front();
					m_Holes.pop_front();

					m_Data[index] = T(std::forward<Args>(args)...);

					m_Versions[index]++;
					m_SlotStates[index] = true;
				} else {
					index = m_Data.size();
					m_Data.emplace_back(std::forward<Args>(args)...);
					m_SlotStates.push_back(true);
					m_Versions.emplace_back(1);
				}

				return { index, m_Versions[index] };
			}

			void Remove(const Handle handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				SizeT index = handle.GetIndex();

				m_Data[index] = T{};

				m_SlotStates[index] = false;
				m_Holes.emplace_back(index);
			}

			[[nodiscard]] std::optional<std::reference_wrapper<T>> TryGet(const Handle handle) {
				if (IsHandleValid(handle)) {
					return std::cref(m_Data[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] std::optional<std::reference_wrapper<const T>> TryGet(const Handle handle) const {
				if (IsHandleValid(handle)) {
					return std::cref(m_Data[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] Reference operator[](const Handle handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] ConstReference operator[](const Handle handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] Iterator begin() {
				sul::dynamic_bitset<>::size_type firstIndex = m_SlotStates.find_first();

				if (firstIndex == sul::dynamic_bitset<>::npos) {
					return end();
				}

				return Iterator(this, firstIndex);
			}

			[[nodiscard]] Iterator end() {
				return Iterator(this, static_cast<SizeT>(m_Data.capacity()));
			}

			[[nodiscard]] ReverseIterator rbegin() {
				return ReverseIterator(end());
			}

			[[nodiscard]] ReverseIterator rend() {
				return ReverseIterator(begin());
			}

			[[nodiscard]] ConstIterator begin() const {
				sul::dynamic_bitset<>::size_type firstIndex = m_SlotStates.find_first();

				if (firstIndex == sul::dynamic_bitset<>::npos) {
					return end();
				}

				return ConstIterator(this, firstIndex);
			}

			[[nodiscard]] ConstIterator end() const {
				return ConstIterator(this, static_cast<SizeT>(m_Data.capacity()));;
			}

			[[nodiscard]] ConstIterator cbegin() const {
				return begin();
			}

			[[nodiscard]] ConstIterator cend() const {
				return end();
			}

			[[nodiscard]] ConstReverseIterator rbegin() const {
				return ConstReverseIterator(end());
			}

			[[nodiscard]] ConstReverseIterator rend() const {
				return ConstReverseIterator(begin());
			}

			[[nodiscard]] ConstReverseIterator crbegin() const {
				return ConstReverseIterator(end());
			}

			[[nodiscard]] ConstReverseIterator crend() const {
				return ConstReverseIterator(begin());
			}

			[[nodiscard]] SizeT Size() const {
				return m_Data.size() - m_Holes.size();
			}

			[[nodiscard]] SizeT RawSize() const {
				return m_Data.size();
			}

			[[nodiscard]] SizeT Capacity() const {
				return m_Data.capacity();
			}

			[[nodiscard]] bool Empty() const {
				return Size() == 0;
			}

			[[nodiscard]] bool IsHandleValid(const Handle handle) const {
				return handle.GetIndex() < RawSize() && m_Versions[handle.GetIndex()] == handle.GetVersion();
			}

			[[nodiscard]] bool IsIndexValid(const SizeT index) const {
				return index < RawSize() && m_SlotStates[index];
			}

			[[nodiscard]] Handle GetIndexHandle(const SizeT index) const {
				CORI_CORE_ASSERT(IsIndexValid(index), "Invalid index passed to FlatSlotMap::GetIndexHandle.");
				return { index, m_Versions[index] };
			}

			void Clear() {
				m_Data.clear();
				m_Versions.clear();
				m_Holes.clear();
				m_SlotStates.clear();
			}

		protected:
			friend Iterator;
			friend ConstIterator;

			[[nodiscard]] Reference operator[](const SizeT index) {
				CORI_CORE_ASSERT(index < m_Data.Size(), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[index];
			}

			[[nodiscard]] ConstReference operator[](const SizeT index) const {
				CORI_CORE_ASSERT(index < m_Data.Size(), "Accessed FlatSlotMap with an invalid handle.");
				return m_Data[index];
			}

			std::vector<T> m_Data{};
			sul::dynamic_bitset<> m_SlotStates{};
		private:
			std::vector<uint32_t> m_Versions{};
			std::deque<SizeT> m_Holes{};
			uint32_t m_ReusedIndexCounter{ 0 };
		};

	}
}


