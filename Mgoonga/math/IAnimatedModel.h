#pragma once

#include "stdafx.h"
#include <base/interfaces.h>

#include "Bone.h"
#include "SkeletalAnimation.h"

//------------------------------------------------
class DLL_MATH IAnimatedModel : public IModel
{
public:
	virtual std::string															RootBoneName() = 0;
	virtual std::vector<Bone>												Bones()	const = 0;
	virtual const std::vector<SkeletalAnimation>&		Animations()const = 0;
	virtual glm::mat4																GlobalTransform() const = 0;
};
