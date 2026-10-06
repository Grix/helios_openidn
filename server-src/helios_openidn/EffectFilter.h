#pragma once
#include <cstdint>
#include "shared/ISPDB25Point.h"
#include <mutex>
#include <atomic>

/// <summary>
/// Applies effects like coloring, transforms, etc, to a stream of graphics data. Used in DMX player etc.
/// </summary>
class EffectFilter
{
public:

	void Apply(ISPDB25Point& point);

	void UpdateParameters(float brightness, float red, float green, float blue, int16_t xShift, int16_t yShift, float xSize, float ySize, float rotation);

private:

	// Parameters
	float brightness = 1;
	float red = 1;
	float green = 1;
	float blue = 1;
	float rotation = 0;
	int16_t xShift = 0;
	int16_t yShift = 0;
	float xSize = 0;
	float ySize = 0;

	bool isActive = false;
	bool holdLast = true;

	std::mutex threadLock;
};

