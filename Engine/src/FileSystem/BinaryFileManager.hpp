#pragma once
#include "Core/Uuid.hpp"
#include "Utility/AggregateStructUID.hpp"

namespace Cori {
	namespace FileSystem {
		namespace Internal {
			template <typename T>
			struct is_vector : std::false_type {};

			template <typename T, typename Alloc>
			struct is_vector<std::vector<T, Alloc>> : std::true_type {};

			template <typename T>
			concept IsVector = is_vector<T>::value;

			template <typename T>
			concept Trivial = std::is_trivial_v<T> && !IsVector<T>;
		}

		/**
		 * @brief Saves and loads aggregate structs to/from drive.
		 * @details You can load or save aggregate struct that contain trivial types or std::vector of any type.
		 * \n It has some safety mechanisms to prevent file corruption or detect it afterward, also it will not allow you to load the data that came from one aggregate struct type into another.
		 * \n 1: When you create the file it generates 1 128bit UUID and writes it to the header and to the footer of the file,
		 * if during reading these UUIDs in footer and header mismatch, it means that the file is corrupted, it will try to look for a backup (.bak file), if it exists it will load it.
		 * \n 2: During compile time an aggregate struct UID is generated for every specialization of SaveAggregateStruct method, this aggregate struct UID is saved in the header of the file,
		 * later when loading it checks if this saved aggregate struct UID is the same as of the struct type you're trying to load the data in, if it detects missmatch, it will not load the data into that struct and instead look for a backup file.
		 * \n 3: When saving a file it calculates a checksum of the data, when loading the file it checks the saved checksum in the header to the checksum calculated from the read data, if it detects a mismatch it will look for a backup.
		 * \n 4: When a file is saved it saves total data size in the header, when reading the file it compares this saved value to the actual size of read data, if it detects a mismatch it will look for a backup.
		 * @note Aggregate struct UID is sensitive to the order of members in the struct and to the member types.
		 */
		class BinaryFileManager {
		public:
			/**
			 * @brief Saves an instance of an aggregate struct onto the disk.
			 * @tparam T Aggregate struct type to save.
			 * @param data Instance of the aggregate struct to save.
			 * @param file File to save to.
			 * @param safeMode Safe mode selector. When enabled it will create a backup after it writes the target file.
			 */
			template<typename T> requires std::is_aggregate_v<T>
			static void SaveAggregateStruct(const T& data, const std::filesystem::path& file, const bool safeMode = false) {
				auto uuid = Core::UUID().GetRaw();

				Serializer serializer;
				serializer.Write(data);

				uint64_t structUID = Utility::GetAggregateStructUID<T>();
				uint64_t checkSum = Utility::fnv1a64(reinterpret_cast<const char*>(serializer.m_Buffer.data()), serializer.m_Buffer.size());
				size_t size = serializer.m_Buffer.size();

				std::filesystem::path targetFile = std::filesystem::path(file) += ".tmp";

				std::filesystem::create_directories(targetFile.parent_path());

				std::ofstream out(targetFile, std::ios::binary | std::ios::trunc);

				if (!out.good()) {
					CORI_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::BinaryFileManager }, "Failed to open file '{}' for writing, aborting operation.", targetFile.string());
					return;
				}

				out.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));
				out.write(reinterpret_cast<const char*>(&structUID), sizeof(structUID));
				out.write(reinterpret_cast<const char*>(&checkSum), sizeof(checkSum));
				out.write(reinterpret_cast<const char*>(&size), sizeof(size));

				out.write(reinterpret_cast<const char*>(serializer.m_Buffer.data()), serializer.m_Buffer.size());

				out.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));

				out.close();

				std::filesystem::rename(targetFile, file);

				if (safeMode) {
					std::filesystem::path bakPath = std::filesystem::path(file) += ".bak";

					std::filesystem::create_directories(bakPath.parent_path());

					std::ofstream outBak(bakPath, std::ios::binary | std::ios::trunc);

					if (!outBak.good()) {
						CORI_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::BinaryFileManager }, "Failed to open file '{}' to creating backup for '{}', aborting operation, backup will not be created!", bakPath.string(), file.string());
						return;
					}

					outBak.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));
					outBak.write(reinterpret_cast<const char*>(&structUID), sizeof(structUID));
					outBak.write(reinterpret_cast<const char*>(&checkSum), sizeof(checkSum));
					outBak.write(reinterpret_cast<const char*>(&size), sizeof(size));

					outBak.write(reinterpret_cast<const char*>(serializer.m_Buffer.data()), serializer.m_Buffer.size());

					outBak.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));

					outBak.close();
				}
			}


			/**
			 * @brief Loads an aggregate struct from the file on the disk.
			 * @tparam T Aggregate struct type to load into. Should be the same type you provided in SaveAggregateStruct when saving it.
			 * @param file File to read from.
			 * @param backupFallback Whether to look for a backup, or no.
			 * @return Expected object with aggregate struct instance with loaded data, or CoriError on faliure.
			 */
			template<typename T> requires std::is_aggregate_v<T>
			static std::expected<T, Core::CoriError<>> LoadAggregateStruct(const std::filesystem::path& file, const bool backupFallback = false) {
				if (backupFallback) {
					return LoadAggregateStructImpl<T>(file).or_else([file](const Core::CoriError<>& error) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::BinaryFileManager }, "Failed to load file. Error: {}", error.what());
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::FileSystem::Self, Logger::Tags::FileSystem::BinaryFileManager }, "Attempting to load backup.");
						return LoadAggregateStructImpl<T>(std::filesystem::path(file) += ".bak");
					});
				}

				return LoadAggregateStructImpl<T>(file);
			}


		private:
			template<typename T> requires std::is_aggregate_v<T>
			static std::expected<T, Core::CoriError<>> LoadAggregateStructImpl(const std::filesystem::path& file) {
				T data;

				std::ifstream in(file, std::ios::binary);

				if (!in.good()) {
					return std::unexpected(Core::CoriError(std::format("Failed to open file '{}' for reading.", file.string())));
				}

				in.seekg(0, std::ios::end);
				size_t fileSize = in.tellg();

				if (fileSize < m_HeaderSize + m_FooterSize) {
					return std::unexpected(Core::CoriError(std::format("Failed to read file '{}'. Impossible file size '{}', file is corrupted.", file.string(), fileSize)));
				}

				in.seekg(0, std::ios::beg);

				std::vector<std::byte> buffer(fileSize);
				try {
					in.read(reinterpret_cast<char*>(buffer.data()), fileSize);
				}
				catch (const std::exception& e) {
					return std::unexpected(Core::CoriError(std::format("Failed to read from file '{}', error '{}'.", file.string(), e.what())));
				}

				auto headerUUID = *reinterpret_cast<std::pair<uint64_t, uint64_t>*>(&buffer[0]);
				auto footerUUID = *reinterpret_cast<std::pair<uint64_t, uint64_t>*>(&buffer[fileSize - m_FooterSize]);
				auto readStructUID = *reinterpret_cast<uint64_t*>(&buffer[16]);
				auto readCheckSum = *reinterpret_cast<uint64_t*>(&buffer[24]);
				auto readDataBlockSize = *reinterpret_cast<size_t*>(&buffer[32]);

				if (readDataBlockSize + m_HeaderSize + m_FooterSize != fileSize) {
					return std::unexpected(Core::CoriError(std::format("Failed to load file '{}'. Expected (cached) file size '{}' does not match the actual file size '{}'. File is corrupted.", file.string(), readDataBlockSize + m_HeaderSize + m_FooterSize, fileSize)));
				}

				if (headerUUID.first != footerUUID.first || headerUUID.second != footerUUID.second) {
					return std::unexpected(Core::CoriError(std::format("Failed to load file '{}'. UUID in the header doesn't match UUID in the footer, usually happens when program began writing a file, but then crashed/got closen without finishing properly. File is corrupted.", file.string())));
				}

				uint64_t structUID = Utility::GetAggregateStructUID<T>();
				if (readStructUID != structUID) {
					return std::unexpected(Core::CoriError(std::format("Failed to load file '{}'. Expected (cached) struct UID '{}' doesn't match the struct UID '{}' (typename <{}>) you're trying to write the contents to. You either trying to load into a incorrect struct type, or the file is corrupted.", file.string(), readStructUID, structUID, CORI_CLEAN_TYPE_NAME(T))));
				}

				uint64_t checkSum = Utility::fnv1a64(reinterpret_cast<const char*>(buffer.data() + m_HeaderSize), readDataBlockSize);

				if (readCheckSum != checkSum) {
					return std::unexpected(Core::CoriError(std::format("Failed to load file '{}'. Expected (cached) check sum '{}' doesn't match the check sum '{}' of the loaded data block. File is corrupted.", file.string(), readCheckSum, checkSum)));
				}

				Deserializer deserializer(buffer, m_HeaderSize);
				deserializer.Read(data);
				return data;
			}

			class Serializer {
			public:
				template <typename T>
				void Write(const T& value) {
					if constexpr (Internal::Trivial<T>) {
						const auto* ptr = reinterpret_cast<const std::byte*>(&value);
						m_Buffer.insert(m_Buffer.end(), ptr, ptr + sizeof(T));
					}
					else if constexpr (Internal::IsVector<T>) {
						Write(value.size());
						const auto* ptr = reinterpret_cast<const std::byte*>(value.data());
						m_Buffer.insert(m_Buffer.end(), ptr, ptr + value.size() * sizeof(typename T::value_type));
					}
					else if constexpr (std::is_aggregate_v<T>) {
						boost::pfr::for_each_field(value, [this](const auto& field) {
							this->Write(field);
						});
					} else {
						static_assert(!sizeof(T), "Unsupported type for serialization");
					}
				}

				std::vector<std::byte> m_Buffer;
			};

			class Deserializer {
			public:
				explicit Deserializer(const std::vector<std::byte>& buffer, const size_t startingOffset) : m_Buffer(buffer), m_Offset(startingOffset) {}

				template <typename T>
				void Read(T& value) {
					if (m_Offset >= m_Buffer.size()) {
						throw std::runtime_error("Read past end of buffer.");
					}

					if constexpr (Internal::Trivial<T>) {
						std::memcpy(&value, m_Buffer.data() + m_Offset, sizeof(T));
						m_Offset += sizeof(T);
					}
					else if constexpr (Internal::IsVector<T>) {
						size_t size;
						Read(size);
						value.resize(size);
						const size_t dataBlockSize = size * sizeof(typename T::value_type);
						std::memcpy(value.data(), m_Buffer.data() + m_Offset, dataBlockSize);
						m_Offset += dataBlockSize;
					}
					else if constexpr (std::is_aggregate_v<T>) {
						boost::pfr::for_each_field(value, [this](auto& field) {
							this->Read(field);
						});
					} else {
						static_assert(!sizeof(T), "Unsupported type for deserialization");
					}
				}

			private:
				const std::vector<std::byte>& m_Buffer;
				size_t m_Offset;
			};

			static constexpr size_t m_HeaderSize = 40;
			static constexpr size_t m_FooterSize = 16;

		};
	}
}
