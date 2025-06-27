#pragma once
#include "ShaderProgram.hpp"

namespace Cori {
	class PipelineProgram {
	public:
		virtual ~PipelineProgram() {};
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void BindShaderProgram(const std::shared_ptr<ShaderProgram> shader) const = 0;
		virtual void UnbindShaderProgram() const = 0;

		static PipelineProgram* Create();
	};

}