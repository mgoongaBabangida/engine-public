#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

#include <base/Object.h>
#include <math/Camera.h>

//------------------------------------------------------------------------------------
class StableObjectSorter
{
public:
	StableObjectSorter(const Camera& camera, const std::vector<shObject>& objects)
	{
		// Freeze the camera position for consistent comparator behavior
		_cameraPosition = camera.getPosition();
		_objectPositions.reserve(objects.size());

		// Snapshot each object's world position
		for (const auto& obj : objects)
		{
			if (obj && obj->GetTransform())
				_objectPositions[obj] = obj->GetTransform()->getTranslation();
		}
	}

	void Sort(std::vector<shObject>& objects) const
	{
		std::sort(objects.begin(), objects.end(), [this](const shObject& a, const shObject& b) {
			return Compare(a, b);
			});
	}

	auto GetComparator() const
	{
		return [this](const shObject& a, const shObject& b) {
			return Compare(a, b);
		};
	}

private:
	glm::vec3																_cameraPosition;
	std::unordered_map<shObject, glm::vec3> _objectPositions;

	bool Compare(const shObject& obj1, const shObject& obj2) const
	{
		bool t1 = obj1->IsTransparent();
		bool t2 = obj2->IsTransparent();

		if (t1 != t2)
			return t1 < t2;

		auto pos1_it = _objectPositions.find(obj1);
		auto pos2_it = _objectPositions.find(obj2);

		if (pos1_it != _objectPositions.end() && pos2_it != _objectPositions.end())
		{
			float dist1 = glm::length2(_cameraPosition - pos1_it->second);
			float dist2 = glm::length2(_cameraPosition - pos2_it->second);

			if (dist1 != dist2)
			{
				if (t1 && t2) // both transparent
					return dist1 > dist2;
				else
					return dist1 < dist2;
			}
		}
		return obj1->Name() < obj2->Name(); // tie-breaker
	}
};
