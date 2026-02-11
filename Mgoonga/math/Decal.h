#pragma once
#include "Geometry.h"

//-------------------------------------------------------
struct Decal
{
	unsigned int decalTextureID;
	dbb::OBB box;
	glm::vec4 uvTransform;
};