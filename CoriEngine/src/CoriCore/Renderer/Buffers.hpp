#pragma once
#include "GraphicsAPIs.hpp"

namespace Cori {
	enum class DRAW_TYPE {
		STATIC,
		DYNAMIC
	};

	enum class ShaderDataType {
		None = 0,
		Float, Vec2, Vec3, Vec4,
		Mat3, Mat4,
		Int, Int2, Int3, Int4,
		UInt, UInt2, UInt3, UInt4,
		Bool
	};

	static constexpr size_t ShaderDataTypeSize(ShaderDataType type) {
		constexpr size_t sizes[] = {
			sizeof(void*),                   // None
			sizeof(float),                   // Float
			sizeof(float) * 2,               // Vec2
			sizeof(float) * 3,               // Vec3
			sizeof(float) * 4,               // Vec4
			sizeof(float) * 3 * 3,           // Mat3
			sizeof(float) * 4 * 4,           // Mat4
			sizeof(int),                     // Int 
			sizeof(int) * 2,                 // Int2
			sizeof(int) * 3,                 // Int3
			sizeof(int) * 4,                 // Int4
			sizeof(uint32_t),                // UInt
			sizeof(uint32_t) * 2,            // UInt2
			sizeof(uint32_t) * 3,            // UInt3
			sizeof(uint32_t) * 4,            // UInt4
			sizeof(bool),                    // Bool
		};

		static_assert(sizeof(sizes) / sizeof(size_t) == (size_t)ShaderDataType::Bool + 1, "ShaderDataTypeSize: Size array is out of sync with ShaderDataType enum");

		if ((size_t)type >= (sizeof(sizes) / sizeof(size_t)))
		{
			CORI_CORE_ASSERT_ERROR(false, "Unknown shader data type");
			return 0;
		}

		return sizes[(size_t)type];
	}

	static constexpr size_t ShaderDataTypeComponentCount(ShaderDataType type) {
		constexpr size_t sizes[] = {
			0,            // None
			1,            // Float
			2,            // Vec2
			3,            // Vec3
			4,            // Vec4
			3 * 3,        // Mat3
			4 * 4,        // Mat4
			1,            // Int
			2,            // Int2
			3,            // Int3
			4,            // Int4
			1,            // UInt
			2,            // UInt2
			3,            // UInt3
			4,            // UInt4
			1,            // Bool
		};

		static_assert(sizeof(sizes) / sizeof(size_t) == (size_t)ShaderDataType::Bool + 1, "ShaderDataTypeSize: Size array is out of sync with ShaderDataType enum");

		if ((size_t)type >= (sizeof(sizes) / sizeof(size_t)))
		{
			CORI_CORE_ASSERT_ERROR(false, "Unknown shader data type");
			return 0;
		}

		return sizes[(size_t)type];
	}

	struct VBElement { 
		VBElement(ShaderDataType type, const std::string& name, const bool normalized = false) : m_Name(name), m_Type(type), m_Normalized(normalized), m_Size(ShaderDataTypeSize(type)) {}

		std::string m_Name;
		ShaderDataType m_Type;
		bool m_Normalized{ false };
		uint32_t m_Size;
		uint32_t m_Offset{ 0 };

		size_t GetComponentCount() const { return ShaderDataTypeComponentCount(m_Type); }

	};

	class VBLayout {
	public:
		VBLayout() = default;

		VBLayout(const std::initializer_list<VBElement>& elements) : m_Elements(elements) {
			for (auto& element : m_Elements) {
				element.m_Offset = m_Stride;
				m_Stride += element.m_Size;
			}
		}

		inline uint32_t GetStrinde() const { return m_Stride; }
		inline const std::vector<VBElement>& GetElements() const { return m_Elements; }
		
		inline std::vector<VBElement>::iterator begin() { return m_Elements.begin(); }
		inline std::vector<VBElement>::iterator end() { return m_Elements.end(); }
		inline std::vector<VBElement>::const_iterator begin() const { return m_Elements.begin(); }
		inline std::vector<VBElement>::const_iterator end() const { return m_Elements.end(); }
	private:
		std::vector<VBElement> m_Elements;
		uint32_t m_Stride{ 0 }; 
	};

	class VertexBuffer {
	public:
		virtual ~VertexBuffer() = default;
		virtual void Init(float* vertices, uint32_t size, DRAW_TYPE drawtype) = 0;
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetLayout(const VBLayout& layout) = 0;
		virtual const VBLayout& GetLayout() const = 0;

		virtual void SetData(const void* data, uint32_t size) const = 0;

		static std::shared_ptr<VertexBuffer> Create();
	};

	class IndexBuffer {
	public:
		virtual ~IndexBuffer() = default;
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static std::shared_ptr<IndexBuffer> Create(uint32_t* indices, uint32_t count);

	};
}