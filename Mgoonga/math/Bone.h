#pragma once

#include <base/interfaces.h>

#include "math.h"

//--------------------------------------------------------------------
class DLL_MATH Bone : public IBone
{
public:
	Bone(size_t index, std::string name, glm::mat4 localBindTransform, bool real = true);
	Bone();
	
	Bone(const Bone&) = default;
	Bone& operator=(const Bone&) = default;
	Bone(Bone&&) = default;
	Bone& operator=(Bone&&) = default;

	void												AddChild(Bone* bone)	{ m_children.push_back(bone); }
	
	void												SetAnimatedTransform(const glm::mat4& trans)	{ m_animatedTransform = trans; }
	void												SetGlobalTransform(const glm::mat4& trans) { m_globaltransform = trans; }
	void												SetLocalBindTransform(const glm::mat4& t) { m_localBindTransform = t; }

	void												SetInverseBindTransform(const glm::mat4& t) {m_inverseBindTransform = t; m_has_explicit_inverse_bind= true;}
	bool												HasExplicitInverseBind() const override { return m_has_explicit_inverse_bind; }

	const glm::mat4&						GetAnimatedTransform() const	{ return m_animatedTransform; }
	const glm::mat4&						GetGlobalTransform() const { return m_globaltransform; }

	const glm::mat4&						GetInverseBindTransform() const { return m_inverseBindTransform; }

	virtual size_t							GetID()const				{ return m_index; }
	virtual const std::string&	GetName() const			{ return m_name; }
	virtual bool								IsRealBone() const	{ return m_realBone; }

	void												SetName(const std::string& _name) { m_name = _name; }
	void												SetMTransform(glm::mat4 trans) { m_mTransform = trans; }

	virtual const glm::mat4&		GetLocalBindTransform() const		{ return m_localBindTransform; }
	virtual const glm::mat4&		GetMTransform() const					{ return m_mTransform; }

	virtual  std::vector<const IBone*>	GetChildren() const;
	std::vector<Bone*>&									GetChildren() { return m_children; }
	size_t															NumChildren() const { return m_children.size(); }

	void																CalculateInverseBindTransform(const glm::mat4& ParentBindTransform, const glm::mat4& rootScene);

protected:
	size_t								m_index = -1;
	std::string						m_name;
	std::vector<Bone*>		m_children;
	bool									m_realBone = true;
	bool									m_has_explicit_inverse_bind = false;

	glm::mat4							m_animatedTransform;
	glm::mat4							m_globaltransform;
	glm::mat4							m_localBindTransform;
	glm::mat4							m_inverseBindTransform;
	glm::mat4							m_mTransform = UNIT_MATRIX; //for empty nodes
};