#pragma once

#include "stdafx.h"
#include <base/interfaces.h>

#include "Transform.h"
#include "Bone.h"

#include <map>

//-------------------------------------------------------------------
struct DLL_MATH Frame
{
	int64_t															m_timeStamp;
	std::map<std::string, Transform>		m_pose;

	Frame(int64_t timeStamp, std::map<std::string, Transform> pose)
		: m_timeStamp(timeStamp), m_pose(pose)
	{}

	Frame() {} //@todo improve

	void AddTimeStamp(int stamp) { m_timeStamp = stamp; }
	void AddTrnasform(std::string name, Transform trs) { m_pose.insert(std::pair<std::string, Transform>(name, trs)); }
	bool Exists(const std::string& name) const { return m_pose.find(name) != m_pose.end(); }

	void SaveToFile(const std::string& filepath) const;
};
