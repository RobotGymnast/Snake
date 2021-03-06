#pragma once

#include "Point.hpp"

class Direction;
struct Line;

// define the bounds of a rectangle
struct Bounds
{
	Point min, max;

	Bounds();
	Bounds(Point min, Point max);
	Bounds(Line);

	operator Line() const;

	// shift by a vector
	Bounds& operator+=(Vector2D);
	
	// get the _whichSide_ side of this rectangle
	Line GetSide(Direction whichSide) const;
	// set the _whichSide_ side of this rectangle
	void SetSide(Line sideBounds, Direction whichSide);
};
