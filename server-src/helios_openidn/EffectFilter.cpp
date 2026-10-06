#include "EffectFilter.h"

void EffectFilter::Apply(ISPDB25Point& point)
{
	if (!isActive)
		return;

	std::lock_guard<std::mutex> atomicSync(threadLock);

	point.r *= (red * brightness);
	point.g *= (green * brightness);
	point.b *= (blue * brightness);
	point.u1 *= brightness;
	if (brightness == 0)
		point.intensity = 0;

	point.x = (point.x - 0x800) * xSize + 0x800;
	point.y = (point.y - 0x800) * ySize + 0x800;

	// Todo rotation

	point.x += (xShift * 2);
	point.y += (yShift * 2);
}

void EffectFilter::UpdateParameters(float _brightness, float _red, float _green, float _blue, int16_t _xShift, int16_t _yShift, float _xSize, float _ySize, float _rotation)
{
	std::lock_guard<std::mutex> atomicSync(threadLock);

	brightness = _brightness;
	red = _red;
	green = _green;
	blue = _blue;
	xShift = _xShift;
	yShift = _yShift;
	xSize = _xSize;
	ySize = _ySize;
	rotation = _rotation;
}

