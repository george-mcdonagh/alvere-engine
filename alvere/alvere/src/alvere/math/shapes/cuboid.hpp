/**
 * \file
 * \author \link https://georgemcdonagh.co.uk George McDonagh
 */

#pragma once

#include "alvere/math/vector/vector_3.hpp"

namespace alvere
{
	struct Point3D;
	struct Line3D;
	struct Sphere;

	struct Cuboid
	{
		static Cuboid unit;

		Vector3 center;
		Vector3 halfExtents;
		float& x = center.x;
		float& y = center.y;
		float& z = center.z;
		float& halfWidth = halfExtents.x;
		float& halfHeight = halfExtents.y;
		float& halfDepth = halfExtents.z;

		float getWidth() const;
		float getHeight() const;
		float getDepth() const;

		float getLeft() const;
		float getRight() const;
		float getBottom() const;
		float getTop() const;
		float getBack() const;
		float getFront() const;

		float getSurfaceArea() const;

		float getVolume() const;

		bool intersects(const Point3D& p) const;
		bool intersects(const Line3D& l) const;
		bool intersects(const Sphere& s) const;
		bool intersects(const Cuboid& c) const;

		bool operator==(const Cuboid& c) const;
		bool operator!=(const Cuboid& c) const;
	};
}