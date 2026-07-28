#pragma once
#include "FlatSlotMap.hpp"
#include "SequentialStorage.hpp"

namespace Cori {
	namespace Core {
		template<typename T, uint16_t REUSE_THRESHOLD = 64, bool ENABLE_VERSIONING = true, template<typename> typename StorageT = GenericSequentialStorage, IsVersionedHandle HandleT = Handle<T>, typename ConstHandleT = ConstHandle<T>>
			requires std::derived_from<HandleT, ConstHandleT> && IsSequentialStorage<StorageT<T>>
		class SparseFlatSlotMap {
		public:
			using Storage = StorageT<T>;

			using Handle = HandleT;
			using ConstHandle = ConstHandleT;

			using SizeT = uint32_t;

			template<bool IsConst>
			struct IteratorImpl {
				using iterator_category = std::bidirectional_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = T;
				using pointer = typename std::conditional<IsConst, const T*, T*>::type;
				using reference = typename std::conditional<IsConst, const T&, T&>::type;
				using MapType = typename std::conditional<IsConst, const SparseFlatSlotMap, SparseFlatSlotMap>::type;

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

				[[nodiscard]] SizeT GetIndex() const {
					return m_Index;
				}

				MapType* m_Map;

			protected:
				friend SparseFlatSlotMap;
				SizeT m_Index;
				IteratorImpl(MapType* m, const SizeT index) : m_Map(m), m_Index(index) {}

			private:
				void SkipForward() {
					sul::dynamic_bitset<>::size_type nextIndex = m_Map->m_SlotStates.find_next(m_Index);

					if (nextIndex == sul::dynamic_bitset<>::npos) {
						m_Index = m_Map->Capacity();
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
			using Reference = T&;
			using ConstReference = const T&;
			using ReverseIterator = std::reverse_iterator<Iterator>;
			using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

			SparseFlatSlotMap() = default;

			explicit SparseFlatSlotMap(const SizeT capacity) {
				Reserve(capacity);
			}

			~SparseFlatSlotMap() {
				DestroyAll();
			}

			SparseFlatSlotMap(const SparseFlatSlotMap&) = delete;
			SparseFlatSlotMap& operator=(const SparseFlatSlotMap&) = delete;

			SparseFlatSlotMap(SparseFlatSlotMap&& other) noexcept {
				Steal(std::move(other));
			}

			SparseFlatSlotMap& operator=(SparseFlatSlotMap&& other) noexcept {
				if (this == &other) {
					return *this;
				}

				DestroyAll();
				Steal(std::move(other));

				return *this;
			}

			void Reserve(const SizeT capacity) {
				if (capacity <= Capacity()) {
					return;
				}

				Relocate(capacity);

				if constexpr (ENABLE_VERSIONING) {
					m_Versions.resize(capacity, 0);
				}

				m_SlotStates.resize(capacity, false);
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
				} else {
					index = m_RawSize;

					if (index >= Capacity()) {
						Grow(index + 1);
					}

					m_RawSize++;
				}

				if constexpr (ENABLE_VERSIONING) {
					m_Versions[index]++;
				}

				Construct(index, std::forward<Args>(args)...);

				if constexpr (requires { m_Data[index].version = uint32_t{}; } && ENABLE_VERSIONING) {
					m_Data[index].version = m_Versions[index];
				}

				if constexpr (requires { m_Data[index].valid = bool{}; }) {
					m_Data[index].valid = true;
				}

				if constexpr (ENABLE_VERSIONING) {
					return Handle{ index, m_Versions[index] };
				}

				return Handle{ index, 1 };
			}

			template<typename... Args>
			bool EmplaceAt(const SizeT index, Args&&... args) {
				static_assert(ENABLE_VERSIONING == false, "Index version of EmplaceAt should only be used with versioning turned off.");

				if (index >= Capacity()) {
					Grow(index + 1);
				}

				CORI_CORE_ASSERT(!m_SlotStates[index], "Double emplace at the same index {}", index);

				Construct(index, std::forward<Args>(args)...);

				if (index >= m_RawSize) {
					m_RawSize = index + 1;
				}

				if constexpr (requires { m_Data[index].valid = bool{}; }) {
					m_Data[index].valid = true;
				}

				return true;
			}

			void Remove(const Handle handle) {
				if (!IsHandleValid(handle)) {
					return;
				}

				const SizeT index = handle.GetIndex();

				Destroy(index);
				m_Holes.emplace_back(index);
			}

			void RemoveAt(const SizeT index) {
				static_assert(ENABLE_VERSIONING == false, "Index version of RemoveAt should only be used with versioning turned off.");

				if (!IsIndexValid(index)) {
					return;
				}

				Destroy(index);
			}

			[[nodiscard]] std::optional<std::reference_wrapper<T>> TryGet(const Handle handle) {
				if (IsHandleValid(handle)) {
					return std::ref(m_Data[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] std::optional<std::reference_wrapper<const T>> TryGet(const ConstHandle handle) const {
				if (IsHandleValid(handle)) {
					return std::cref(std::as_const(m_Data)[handle.GetIndex()]);
				}

				return std::nullopt;
			}

			[[nodiscard]] Reference operator[](const Handle handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed SparseFlatSlotMap with an invalid handle.");
				return m_Data[handle.GetIndex()];
			}

			[[nodiscard]] ConstReference operator[](const ConstHandle handle) const {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Accessed SparseFlatSlotMap with an invalid handle.");
				return std::as_const(m_Data)[handle.GetIndex()];
			}

			[[nodiscard]] Reference operator[](const SizeT index) {
				CORI_CORE_ASSERT(IsIndexValid(index), "Accessed SparseFlatSlotMap with an invalid index.");
				return m_Data[index];
			}

			[[nodiscard]] ConstReference operator[](const SizeT index) const {
				CORI_CORE_ASSERT(IsIndexValid(index), "Accessed SparseFlatSlotMap with an invalid index.");
				return std::as_const(m_Data)[index];
			}

			[[nodiscard]] Iterator begin() {
				sul::dynamic_bitset<>::size_type firstIndex = m_SlotStates.find_first();

				if (firstIndex == sul::dynamic_bitset<>::npos) {
					return end();
				}

				return Iterator(this, firstIndex);
			}

			[[nodiscard]] Iterator end() {
				return Iterator(this, Capacity());
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
				return ConstIterator(this, Capacity());
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
				return m_OccupancyCounter;
			}

			[[nodiscard]] SizeT RawSize() const {
				return m_RawSize;
			}

			[[nodiscard]] SizeT Capacity() const {
				return static_cast<SizeT>(m_Data.Capacity());
			}

			[[nodiscard]] bool Empty() const {
				return Size() == 0;
			}

			void Sync() {
				m_Data.Sync();
			}

			[[nodiscard]] decltype(auto) GetVulkanBuffer() requires requires(Storage& storage) { storage.GetVulkanBuffer(); } {
				return m_Data.GetVulkanBuffer();
			}

			[[nodiscard]] decltype(auto) GetVulkanBuffer() const requires requires(const Storage& storage) { storage.GetVulkanBuffer(); } {
				return m_Data.GetVulkanBuffer();
			}

			[[nodiscard]] bool IsHandleValid(const ConstHandle handle) const {
				if constexpr (ENABLE_VERSIONING) {
					return IsIndexValid(handle.GetIndex()) && m_Versions[handle.GetIndex()] == handle.GetVersion();
				}

				return IsIndexValid(handle.GetIndex());
			}

			[[nodiscard]] bool IsIndexValid(const SizeT index) const {
				return index < m_RawSize && m_SlotStates[index];
			}

			[[nodiscard]] bool IsIndexOccupied(const SizeT index) const {
				return IsIndexValid(index);
			}

			[[nodiscard]] Handle GetIndexHandle(const SizeT index) const {
				static_assert(ENABLE_VERSIONING == true, "GetIndexHandle should only be used with versioning turned on.");
				CORI_CORE_ASSERT(IsIndexValid(index), "Invalid index passed to SparseFlatSlotMap::GetIndexHandle.");
				return { index, m_Versions[index] };
			}

			void Clear() {
				DestroyAll();

				m_SlotStates.reset();
				m_Holes.clear();
				m_OccupancyCounter = 0;
				m_RawSize = 0;
				m_ReusedIndexCounter = 0;
			}

		protected:
			friend Iterator;
			friend ConstIterator;

			void Relocate(const SizeT newCapacity) {
				if constexpr (std::is_trivially_copyable_v<T>) {
					m_Data.ReallocateNonReporting(newCapacity);
				} else {
					Storage grown(newCapacity);

					m_SlotStates.iterate_bits_on([this, &grown](const size_t index) {
						new (grown.Data() + index) T(std::move(m_Data.Data()[index]));
						m_Data.Data()[index].~T();
					});

					m_Data = std::move(grown);
					m_Data.ReportChange(0, static_cast<uint64_t>(newCapacity) * sizeof(T));
				}
			}

			void Grow(const SizeT minCapacity) {
				const SizeT currentCapacity = Capacity();
				Reserve(std::max<SizeT>(minCapacity, currentCapacity == 0 ? 4 : static_cast<SizeT>(currentCapacity * GROWTH_FACTOR)));
			}

			template<typename... Args>
			void Construct(const SizeT index, Args&&... args) {
				new (m_Data.Data() + index) T(std::forward<Args>(args)...);

				m_Data.ReportChange(index * sizeof(T), sizeof(T));
				m_SlotStates[index] = true;
				m_OccupancyCounter++;
			}

			void Destroy(const SizeT index) {
				T* slot = m_Data.Data() + index;

				slot->~T();
				std::memset(slot, s_PoisonValue, sizeof(T));

				m_Data.ReportChange(index * sizeof(T), sizeof(T));
				m_SlotStates[index] = false;
				m_OccupancyCounter--;
			}

			void DestroyAll() {
				if constexpr (!std::is_trivially_destructible_v<T>) {
					m_SlotStates.iterate_bits_on([this](const size_t index) {
						(m_Data.Data() + index)->~T();
					});
				}
			}

			void Steal(SparseFlatSlotMap&& other) noexcept {
				m_Data = std::move(other.m_Data);
				m_SlotStates = std::move(other.m_SlotStates);
				m_Holes = std::move(other.m_Holes);
				m_Versions = std::move(other.m_Versions);

				other.m_SlotStates.clear();
				other.m_Holes.clear();

				m_OccupancyCounter = other.m_OccupancyCounter;
				other.m_OccupancyCounter = 0;
				m_RawSize = other.m_RawSize;
				other.m_RawSize = 0;
				m_ReusedIndexCounter = other.m_ReusedIndexCounter;
				other.m_ReusedIndexCounter = 0;
			}

			Storage m_Data{};
			sul::dynamic_bitset<> m_SlotStates{};
			SizeT m_OccupancyCounter{ 0 };
			SizeT m_RawSize{ 0 };

		private:
			std::deque<SizeT> m_Holes{};
			std::conditional_t<ENABLE_VERSIONING, std::vector<uint32_t>, uint8_t> m_Versions{};
			uint32_t m_ReusedIndexCounter{ 0 };

			static constexpr float GROWTH_FACTOR{ 2.0f };
		};
	}
}
