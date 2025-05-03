#pragma once
#include "../PipelineProgram.hpp"

namespace Cori {
	class OpenGLPipelineProgram : public PipelineProgram {
	public:
		OpenGLPipelineProgram();
		virtual ~OpenGLPipelineProgram();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void BindShaderProgram(const std::shared_ptr<ShaderProgram> shader) const override;
		virtual void UnbindShaderProgram() const override;

	private:
		uint32_t m_ID;
	};
}