/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BodyShapes.h"

#include <cmath>

#include <algorithm>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

using namespace openblack::physics;

Shape shapes::FromModel(std::span<const glm::vec3> positions, std::span<const std::array<uint32_t, 3>> triangles, float scale)
{
	Shape shape;
	shape.points.assign(positions.size(), glm::vec3 {});
	shape.centreOfMass = glm::vec3(0.0f);
	for (const auto& p : positions)
	{
		shape.centreOfMass += p;
	}
	if (!positions.empty())
	{
		shape.centreOfMass /= static_cast<float>(positions.size());
	}
	shape.radius = 0.0f;
	for (size_t i = 0; i < positions.size(); ++i)
	{
		shape.points[i] = (positions[i] - shape.centreOfMass) * scale;
		shape.radius = std::max(shape.radius, glm::length(shape.points[i]));
	}
	shape.faces.reserve(triangles.size());
	for (const auto& t : triangles)
	{
		if (t[0] < shape.points.size() && t[1] < shape.points.size() && t[2] < shape.points.size())
		{
			shape.faces.push_back(t);
		}
	}
	return shape;
}

Shape shapes::Tree(float height, float radius, float scale, bool rooted)
{
	const float h = (rooted ? 0.6f : 0.5f) * height;
	const glm::vec3 com(0.0f, (rooted ? 0.4f : 0.5f) * height / scale, 0.0f);
	Shape shape;
	auto& v = shape.points;
	for (const auto& [r, y] :
	     {std::pair {0.65f * radius, -0.6f * h}, std::pair {radius, 0.0f}, std::pair {0.65f * radius, 0.6f * h}})
	{
		v.emplace_back(r, y, 0.0f);
		v.emplace_back(0.0f, y, -r);
		v.emplace_back(-r, y, 0.0f);
		v.emplace_back(0.0f, y, r);
	}
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.0f, h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	auto& f = shape.faces;
	for (const uint32_t b : {0u, 4u})
	{
		f.push_back({b, b + 1, b + 5});
		f.push_back({b, b + 5, b + 4});
		f.push_back({b + 1, b + 2, b + 6});
		f.push_back({b + 1, b + 6, b + 5});
		f.push_back({b + 2, b + 3, b + 7});
		f.push_back({b + 2, b + 7, b + 6});
		f.push_back({b + 3, b, b + 4});
		f.push_back({b + 3, b + 4, b + 7});
	}
	for (const std::array<uint32_t, 3> t : {std::array<uint32_t, 3> {12, 1, 0},
	                                        {12, 2, 1},
	                                        {12, 3, 2},
	                                        {12, 0, 3},
	                                        {13, 8, 9},
	                                        {13, 9, 10},
	                                        {13, 10, 11},
	                                        {13, 11, 8}})
	{
		f.push_back(t);
	}
	shape.centreOfMass = com;
	shape.radius = std::max(h, radius);
	shape.dragFactor = 0.3f;
	return shape;
}

Shape shapes::LivingBody(Living kind, float height, float radius, float scale)
{
	const bool animal = kind == Living::Animal;
	const float y = 0.5f * height;
	const float a = (animal ? 0.25f : 0.8f) * radius;
	const float z = (animal ? 0.8f : 0.32f) * radius;
	const float b = (animal ? 0.3f : 0.9f) * radius;
	const float c = (animal ? 0.9f : 0.32f) * radius;
	const std::array<glm::vec3, 12> raw = {{{a, y, z},
	                                        {-a, y, -z},
	                                        {-a, y, z},
	                                        {a, y, -z},
	                                        {b, 0.0f, c},
	                                        {-b, 0.0f, -c},
	                                        {-b, 0.0f, c},
	                                        {b, 0.0f, -c},
	                                        {a, -y, z},
	                                        {-a, -y, -z},
	                                        {-a, -y, z},
	                                        {a, -y, -z}}};
	// the vertices turned by RotY((float)pi / 2) with c stored as a float, -4.371139e-8f, and s = 1.0f:
	// x' = fl(-z + fl(c x)), z' = fl(x + fl(c z)); the radius is sqrt((z z + y y) + x x) of the turned vertex
	constexpr float k_C = -4.371139e-8f;
	Shape shape;
	auto& v = shape.points;
	float farthest = 0.0f;
	for (const auto& p : raw)
	{
		v.emplace_back(-p.z + k_C * p.x, p.y, p.x + k_C * p.z);
		const auto& q = v.back();
		farthest = std::max(farthest, std::sqrt((q.z * q.z + q.y * q.y) + q.x * q.x));
	}
	const std::vector<std::array<uint32_t, 3>> f = {{1, 2, 0},  {0, 3, 1},  {5, 1, 3},  {7, 5, 3},   {7, 3, 0},
	                                                {4, 7, 0},  {6, 4, 0},  {2, 6, 0},  {6, 2, 1},   {5, 6, 1},
	                                                {9, 5, 7},  {11, 9, 7}, {11, 7, 4}, {8, 11, 4},  {10, 8, 4},
	                                                {6, 10, 4}, {10, 6, 5}, {9, 10, 5}, {11, 10, 9}, {8, 10, 11}};
	shape.faces = f;
	shape.centreOfMass = glm::vec3(0.0f, y / scale, 0.0f);
	shape.radius = farthest;
	shape.dragFactor = 2.0f;
	return shape;
}

Shape shapes::Fragment(std::span<const glm::vec3> points, std::span<const glm::vec3> normals)
{
	Shape shape;
	auto& local = shape.points;
	float r = 0.0f;
	for (size_t i = 0; i < points.size(); ++i)
	{
		local.push_back(points[i]);
		local.push_back(points[i] - 0.45f * normals[i]);
		r = std::max({r, glm::length(local[local.size() - 2]), glm::length(local.back())});
	}
	shape.centreOfMass = glm::vec3(0.0f);
	shape.radius = r;
	shape.dragFactor = 2.0f;
	return shape;
}
