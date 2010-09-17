#pragma once

#include "Bounds.hpp"
#include "Color24.hpp"
#include "Screen.hpp"

class WorldObject
{
public:
	enum ObjectType
	{
		snake,
		wall,
		food
	};

protected:
	ObjectType type;
	Bounds bounds;
	Color24 color;

public:
	WorldObject(ObjectType);

	ObjectType GetObjectType() const;
	virtual void CollisionHandler(const WorldObject& colidee) = 0;
	void Draw(Screen& target) const;

	Bounds GetBounds() const;
	Color24 GetColor() const;
};
