#pragma once

float4 ConvertColor(uint32_t rgba) {
	float4 result;
	result.x = ((rgba >> 24) & 0xFF) / 255.0;
	result.y = ((rgba >> 16) & 0xFF) / 255.0;
	result.z = ((rgba >> 8) & 0xFF) / 255.0;
	result.w = (rgba & 0xFF) / 255.0;
	return result;
};