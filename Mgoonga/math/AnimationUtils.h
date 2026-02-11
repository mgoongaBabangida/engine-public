#pragma once
#include "math.h"

#include "Bone.h"

#include <string>

//--------------------------------------------------------
std::string NormalizeBoneName(const std::string& name);

//--------------------------------------------------------
bool IsStandardSkeleton(const std::vector<Bone>& skeleton);