#ifndef CORI_PCH
#define CORI_PCH

#include "Global_pch.hpp"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC 1
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_ASSERT_ON_RESULT
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_to_string.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#endif CORI_PCH