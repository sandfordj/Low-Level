#pragma once
#include <cmath>
#include "Vec3.h"
#include <sstream>
class Sphere
{
public:
	Vec3f center;                           /// position of the sphere
	float radius, radius2;                  /// sphere radius and radius^2
	Vec3f surfaceColor, emissionColor;      /// surface color and emission (light)
	float transparency, reflection;         /// surface transparency and reflectivity
public:
	Sphere(
		const Vec3f &c,
		const float &r,
		const Vec3f &sc,
		const float &refl = 0,
		const float &transp = 0,
		const Vec3f &ec = 0);
	bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t0, float &t1) const;
	Vec3f getCenter() const;
	float getRadius() const;
	float getRadiusSquare() const;
	Vec3f getSurfaceColor() const;
	Vec3f getEmissionsColor() const;
	float getTransparency() const;
	float getReflection() const;

	void setCenter(Vec3f newPos);
	void Move(float x, float y, float z);
	void Move(Vec3f amount);
	void SetRadius(float r);
	void SetRadius(Vec3f amount);
	void increaseRadius(float r);
	void increaseRadius(Vec3f amount);
	std::string ToString();
};

