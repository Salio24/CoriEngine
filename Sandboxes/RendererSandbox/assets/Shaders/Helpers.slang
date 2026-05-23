#pragma once

float4 ConvertColor(uint32_t rgba) {
	float4 result;
	result.x = rgba >> 24 & 0x000000FF / 255;
	result.y = rgba >> 16 & 0x000000FF / 255;
	result.z = rgba >> 8 & 0x000000FF / 255;
	result.w = rgba & 0x000000FF / 255;
	return result;
};