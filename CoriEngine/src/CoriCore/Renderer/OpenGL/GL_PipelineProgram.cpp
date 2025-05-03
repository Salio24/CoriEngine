// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_PipelineProgram.hpp"
#include <glad/gl.h>

namespace Cori {
	OpenGLPipelineProgram::OpenGLPipelineProgram() {
		glGenProgramPipelines(1, &m_ID);
	}

	OpenGLPipelineProgram::~OpenGLPipelineProgram() {
		glDeleteProgramPipelines(1, &m_ID);
	}

	void OpenGLPipelineProgram::Bind() const {
		glBindProgramPipeline(m_ID);
	}

	void OpenGLPipelineProgram::Unbind() const {
		glBindProgramPipeline(0);
	}

	void OpenGLPipelineProgram::BindShaderProgram(const std::shared_ptr<ShaderProgram> shader) const {
		glUseProgramStages(m_ID, GL_ALL_SHADER_BITS, shader->GetID());
	}

	void OpenGLPipelineProgram::UnbindShaderProgram() const {
		glUseProgramStages(m_ID, GL_ALL_SHADER_BITS, 0);
	}
}