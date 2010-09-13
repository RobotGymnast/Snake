#include "World.hpp"
#include "WorldObject.hpp"

#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/random.hpp>
#include <vector>

#include "collision.h"
#include "Common.hpp"
#include "custom_algorithm.hpp"
#include "Food.hpp"
#include "Logger.hpp"
#include "Timer.hpp"
#include "Wall.hpp"

using namespace std;
using namespace boost;

namespace World {
namespace {
typedef vector<WorldObject*> ObjectList;
// TODO: make WallBox a class
typedef array<Wall, 4> WallBox;

WallBox make_walls();

ObjectList objects;
WallBox walls = make_walls();
Logger::Handle logger = Logger::RequestHandle("World");

DEF_CONSTANT(Bounds, worldBounds, Bounds(Point(0, 0), Point(800, 600)))
DEF_CONSTANT(unsigned int, wallThickness, 10)

void add_walls_to_world(WallBox& wallbox)
{
	for_each(wallbox.begin(), wallbox.end(), bind(&Wall::AddToWorld, _1));
}
WallBox make_walls()
{
	WallBox wallbox;
	wallbox[0] = Wall(Point(0, 0), wallThickness(), worldBounds().max.y);
	wallbox[1] = Wall(Point(worldBounds().max.x - wallThickness(), 0), wallThickness(), worldBounds().max.y);
	wallbox[2] = Wall(Point(0, 0), worldBounds().max.x, wallThickness());
	wallbox[3] = Wall(Point(0, worldBounds().max.y - wallThickness()), worldBounds().max.x, wallThickness());

	add_walls_to_world(wallbox);

	return wallbox;
}
inline bool object_exists(const WorldObject* obj)
{
	return in(objects.begin(), objects.end(), obj);
}

void Add(WorldObject& obj)
{
	assert(!object_exists(&obj));

	objects.push_back(&obj);
	logger.Debug(format("Type %1% object %2% added") % obj.GetObjectType() % &obj);
}

void Remove(WorldObject& obj)
{
	if(unordered_find_and_remove(objects, &obj))
		logger.Debug(format("Type %1% object %2% removed") % obj.GetObjectType() % &obj);
	else
		logger.Debug(format("Invalid object (%1%) specified") % &obj);
}

namespace GameWorld {
namespace {
Timer foodTimer;
typedef vector<Food> Menu;
Menu foods;
DEF_CONSTANT(unsigned int, foodAdditionPeriod, 8000)
DEF_CONSTANT(unsigned int, foodSize, 15)
}

void Update(Bounds spawnBounds)
{
	// URGENT URGENT TODO: fix this code!
	bool endReached = false;
	do
	{
		endReached = true;
		for(Menu::iterator i = foods.begin(), end = foods.end(); i != end; ++i)
		{
			if(i->IsEaten())
			{
				unordered_remove(foods, i);
				endReached = false;
				break;
			}
		}
	}
	while(!endReached);

	if(foodTimer.ResetIfHasElapsed(foodAdditionPeriod()))
	{
		// I'm 90% sure I'm doin' it wrong
		int32_t seed(time(NULL));
		Point foodLocation;
		foodLocation.x = minstd_rand0(seed)() % (worldBounds().max.x - foodSize());
		foodLocation.y = minstd_rand0(seed)() % (worldBounds().max.y - foodSize());

		Food newFood(foodLocation, foodSize());
		newFood.AddToWorld();
		foods.push_back(newFood);
	}
}
void Reset()
{
	foods.clear();
	foodTimer.Reset();
}
}

namespace GraphicsWorld {
namespace {
DEF_CONSTANT(Point, screenBounds, Point(800, 600))
Screen screen(screenBounds().x, screenBounds().y);
}
void Update()
{
	screen.Clear();

	for_each(objects.begin(), objects.end(),
		bind(&WorldObject::Draw, _1, boost::ref(screen))
	);

	screen.Update();
}
}

namespace PhysicsWorld {
static inline CollidableObject world_to_collidable_object(const WorldObject* w)
{
	CollidableObject ret;

	Bounds bounds = w->GetBounds();
	ret.Min.x = bounds.min.x;
	ret.Min.y = bounds.min.y;
	ret.Max.x = bounds.max.x;
	ret.Max.y = bounds.max.y;

	return ret;
}

static inline void handle_potential_collision(WorldObject* o1, WorldObject* o2)
{
	CollidableObject c1 = world_to_collidable_object(o1);
	CollidableObject c2 = world_to_collidable_object(o2);

	if(does_collide(&c1, &c2))
	{
		o1->CollisionHandler(*o2);
		o2->CollisionHandler(*o1);
	}
}

static inline void collide_with_subsequent_objects(ObjectList::iterator collider)
{
	for_each(collider + 1, objects.end(), boost::bind(handle_potential_collision, *collider, _1));
}

void Update()
{
	for(ObjectList::iterator collider = objects.begin(), lastGroup = objects.end() - 1; collider != lastGroup; ++collider)
		collide_with_subsequent_objects(collider);
}
}
}

void Update()
{
	PhysicsWorld::Update();
	GraphicsWorld::Update();
	// TODO: reduce spawn area
	GameWorld::Update(GetBounds());
}
void Reset()
{
	bool end_reached = false;
	while(!end_reached)
	{
		end_reached = true;
		for(ObjectList::iterator i = objects.begin(), end = objects.end(); i != end; ++i)
		{
			end_reached = false;
			(*i)->RemoveFromWorld();
			break;
		}
	}

	GameWorld::Reset();
	add_walls_to_world(walls);
}
Bounds GetBounds()
{
	return worldBounds();
}
Point GetCenter()
{
	return Point(worldBounds().max.x / 2, worldBounds().max.y / 2);
}

WorldObject::WorldObject(ObjectType _type) :
	inWorld(false), type(_type)
{
}

WorldObject::WorldObject(const WorldObject& obj) :
	inWorld(obj.inWorld), type(obj.type), bounds(obj.bounds), color(obj.color)
{
	if(inWorld)
		World::Add(*this);
}

WorldObject& WorldObject::operator=(const WorldObject& o)
{
	if(!inWorld && o.inWorld)
		World::Add(*this);

	inWorld = o.inWorld;
	type = o.type;
	bounds = o.bounds;
	color = o.color;

	return *this;
}

WorldObject::~WorldObject()
{
	if(inWorld)
		World::Remove(*this);
}

void WorldObject::AddToWorld()
{
	if(!inWorld)
		World::Add(*this);
	else
		logger.Debug(format("Object %1% already in world!") % this);

	inWorld = true;
}
void WorldObject::RemoveFromWorld()
{
	if(inWorld)
		World::Remove(*this);
	else
		logger.Debug(format("Object %1% not in world!") % this);

	inWorld = false;
}

WorldObject::ObjectType WorldObject::GetObjectType() const
{
	return type;
}
void WorldObject::Draw(Screen& target) const
{
	SDL_Surface* surface = target.GetSurface();

	SDL_Rect rect;
	rect.w = bounds.max.x - bounds.min.x;
	rect.h = bounds.max.y - bounds.min.y;
	rect.x = bounds.min.x;
	rect.y = bounds.min.y;

	if(SDL_FillRect(surface, &rect, color.GetRGBMap(surface)) == -1)
	{
		string error = "Error drawing to screen: ";
		error += SDL_GetError();
		logger.Fatal(error.c_str());
	}
}

Bounds WorldObject::GetBounds() const
{
	return bounds;
}
Color24 WorldObject::GetColor() const
{
	return color;
}
}
