#include "stdafx.h"
#include "BoxColliderDynamic.h"
#include <math/Colliders.h>

#include <glm\glm\gtx\norm.hpp>
#include <glm\glm\gtc\quaternion.hpp>
#include <glm\glm\gtx\quaternion.hpp>
#include <glm\glm\gtx\euler_angles.hpp>

//-----------------------------------------------------------------------
BoxColliderDynamic::BoxColliderDynamic(const BoxColliderDynamic& _other)
	: dbb::OBBCollider(_other)
{
	m_data = _other.m_data;
	m_rigger = _other.m_rigger;
}

//------------------------------------------------------------------
void BoxColliderDynamic::CalculateExtremDots(IRigger* _rigger, IModel* _model)
{
	BaseCollider::CalculateExtremDots(_model);

	m_rigger = _rigger;
	if (m_rigger && m_data.empty())
	{
		for (size_t i = 0; i < m_rigger->GetAnimationCount(); ++i)
		{
			m_data.push_back({ m_rigger->GetAnimationNames()[i] });
			m_rigger->Apply(m_data.back().name, false);
			std::array<glm::mat4, MAX_BONES> gBones; size_t frame_index = 0;
			while (true)
			{
				if (frame_index >= m_rigger->GetCurrentAnimation()->GetNumFrames())
					break;

				gBones = m_rigger->GetMatrices(m_data.back().name, frame_index);
				//calculate extrem dots for frmae j
				extremDots dts;
				for (int k = 0; k < _model->GetMeshCount(); ++k)
				{
					auto vertices = _model->Get3DMeshes()[k]->GetVertexs();
					for (int i = 0; i < vertices.size(); ++i)
					{
						glm::mat4 BoneTransform = gBones[vertices[i].boneIDs[0]] * vertices[i].weights[0];
						BoneTransform += gBones[vertices[i].boneIDs[1]] * vertices[i].weights[1];
						BoneTransform += gBones[vertices[i].boneIDs[2]] * vertices[i].weights[2];
						BoneTransform += gBones[vertices[i].boneIDs[3]] * vertices[i].weights[3];

						glm::vec4 vertexPos = BoneTransform * glm::vec4(vertices[i].Position, 1.0);

						if (vertexPos.x > dts.MaxX)
							dts.MaxX = vertexPos.x;
						if (vertexPos.x < dts.MinX)
							dts.MinX = vertexPos.x;
						if (vertexPos.y > dts.MaxY)
							dts.MaxY = vertexPos.y;
						if (vertexPos.y < dts.MinY)
							dts.MinY = vertexPos.y;
						if (vertexPos.z > dts.MaxZ)
							dts.MaxZ = vertexPos.z;
						if (vertexPos.z < dts.MinZ)
							dts.MinZ = vertexPos.z;
					}
				}
				m_data.back().extremDots.push_back(dts);
				glm::vec3 center = glm::vec3(dts.MaxX - glm::length(dts.MaxX - dts.MinX) / 2,
					dts.MaxY - glm::length(dts.MaxY - dts.MinY) / 2,
					dts.MaxZ - glm::length(dts.MaxZ - dts.MinZ) / 2);
				m_data.back().centers.push_back(center);

				glm::vec3 corner = glm::vec3(dts.MaxX, dts.MaxY, dts.MaxZ);
				m_data.back().radiuses.push_back(glm::length(corner - center));
				frame_index++;
			}
		}
		m_rigger->Apply("Null", false);
	}
}

//------------------------------------------------------------------
void BoxColliderDynamic::CalculateExtremDots(const eObject* _object)
{
  BaseCollider::CalculateExtremDots(_object);

  m_rigger = _object->GetRigger();
	if (m_rigger && m_data.empty())
	{
		for (size_t i = 0; i < m_rigger->GetAnimationCount(); ++i)
		{
			m_data.push_back({ m_rigger->GetAnimationNames()[i] });
			m_rigger->Apply(m_data.back().name, false);
			std::array<glm::mat4, MAX_BONES> gBones; size_t frame_index = 0;
			while(true)
			{
				if (frame_index >= m_rigger->GetCurrentAnimation()->GetNumFrames())
					break;

				gBones = m_rigger->GetMatrices(m_data.back().name, frame_index);
				//calculate extrem dots for frmae j
				extremDots dts;
				for (int k = 0; k < _object->GetModel()->GetMeshCount(); ++k)
				{
					auto vertices = _object->GetModel()->Get3DMeshes()[k]->GetVertexs();
					for (int i = 0; i < vertices.size(); ++i)
					{
						glm::mat4 BoneTransform  = gBones[vertices[i].boneIDs[0]] * vertices[i].weights[0];
											BoneTransform += gBones[vertices[i].boneIDs[1]] * vertices[i].weights[1];
											BoneTransform += gBones[vertices[i].boneIDs[2]] * vertices[i].weights[2];
											BoneTransform += gBones[vertices[i].boneIDs[3]] * vertices[i].weights[3];

						glm::vec4 vertexPos = BoneTransform * glm::vec4(vertices[i].Position, 1.0);

						if (vertexPos.x > dts.MaxX)
							dts.MaxX = vertexPos.x;
						if (vertexPos.x < dts.MinX)
							dts.MinX = vertexPos.x;
						if (vertexPos.y > dts.MaxY)
							dts.MaxY = vertexPos.y;
						if (vertexPos.y < dts.MinY)
							dts.MinY = vertexPos.y;
						if (vertexPos.z > dts.MaxZ)
							dts.MaxZ = vertexPos.z;
						if (vertexPos.z < dts.MinZ)
							dts.MinZ = vertexPos.z;
					}
				}
				m_data.back().extremDots.push_back(dts);
				glm::vec3 center = glm::vec3(dts.MaxX - glm::length(dts.MaxX - dts.MinX) / 2,
																		 dts.MaxY - glm::length(dts.MaxY - dts.MinY) / 2,
																		 dts.MaxZ - glm::length(dts.MaxZ - dts.MinZ) / 2);
				m_data.back().centers.push_back(center);

				glm::vec3 corner = glm::vec3(dts.MaxX, dts.MaxY, dts.MaxZ);
				m_data.back().radiuses.push_back(glm::length(corner - center));
				frame_index++;
			}
		}
		m_rigger->Apply("Null", false);
	}
}

//------------------------------------------------------------------
std::vector<glm::mat3> BoxColliderDynamic::GetBoundingTriangles(const ITransform& _trans) const
{
	if (!m_rigger || !m_rigger->GetCurrentAnimation())
		return BaseCollider::GetBoundingTriangles(_trans);
	else
	{
		auto it = std::find_if(m_data.begin(), m_data.end(), [this](const AnimationData& _data) { return _data.name == m_rigger->GetCurrentAnimationName(); });
		if (it != m_data.end())
		{
			size_t frame = m_rigger->GetCurrentAnimationFrameIndex();
			if (frame == -1)
				return BaseCollider::GetBoundingTriangles(_trans);
			else
			{
				if (it->extremDots.size() <= frame) //@todo better thread-sync instead of this
					frame = it->extremDots.size() - 1;
				auto dots = it->extremDots[frame];
				glm::mat4 transform = _trans.getModelMatrix();
				std::vector<glm::mat3> ret; // Getting 12 triangles of the bouning cube
				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MaxZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MaxZ, 1.0f))));

				ret.push_back(glm::mat3(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MinZ, 1.0f)),
					glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f))));
				return ret;
			}
		}
		else
			return BaseCollider::GetBoundingTriangles(_trans);
	}
}

//------------------------------------------------------------------
std::vector<glm::vec3> BoxColliderDynamic::GetExtrems(const ITransform& _trans) const
{
	if (!m_rigger || !m_rigger->GetCurrentAnimation())
		return BaseCollider::GetExtrems(_trans);
	else
	{
		auto it = std::find_if(m_data.begin(), m_data.end(), [this](const AnimationData& _data) { return _data.name == m_rigger->GetCurrentAnimationName(); });
		if (it != m_data.end())
		{
			size_t frame = m_rigger->GetCurrentAnimationFrameIndex();
			if(frame < 0)
				return BaseCollider::GetExtrems(_trans);
			else
			{
				extremDots dots;
				if(frame < it->extremDots.size()) //@todo better thread-sync instead of this
					dots = it->extremDots[frame];
				else
					dots = it->extremDots[0];
				glm::mat4 transform = _trans.getModelMatrix();
				std::vector<glm::vec3> ret;
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MaxZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MaxY, dots.MinZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MinZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MinX, dots.MaxY, dots.MaxZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MaxZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MaxX, dots.MinY, dots.MinZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MinZ, 1.0f)));
				ret.push_back(glm::vec3(transform * glm::vec4(dots.MinX, dots.MinY, dots.MaxZ, 1.0f)));
				return ret;
			}
		}
		else
			return BaseCollider::GetExtrems(_trans);
	}
}

//------------------------------------------------------------------
extremDots BoxColliderDynamic::GetExtremDotsLocalSpace() const
{
	if (!m_rigger || !m_rigger->GetCurrentAnimation())
		return BaseCollider::GetExtremDotsLocalSpace();
	else
	{
		auto it = std::find_if(m_data.begin(), m_data.end(), [this](const AnimationData& _data) { return _data.name == m_rigger->GetCurrentAnimationName(); });
		if (it != m_data.end())
		{
			size_t frame = m_rigger->GetCurrentAnimationFrameIndex();
			if (frame == -1)
				return BaseCollider::GetExtremDotsLocalSpace();
			else
			{
				if(frame < it->extremDots.size())
					return it->extremDots[frame];
				else
					return it->extremDots[0]; //@todo better thread-sync instead of this
			}
		}
		else
			return BaseCollider::GetExtremDotsLocalSpace();
	}
}

//------------------------------------------------------------------
glm::vec3 BoxColliderDynamic::GetCenterLocalSpace() const
{
	if (!m_rigger || m_rigger->GetCurrentAnimation() == nullptr || m_rigger->GetCurrentAnimationFrameIndex() == -1)
		return BaseCollider::GetCenterLocalSpace();
	else
	{
		auto it = std::find_if(m_data.begin(), m_data.end(), [this](const AnimationData& _data) { return _data.name == m_rigger->GetCurrentAnimationName(); });
		if (it != m_data.end())
		{
			auto index = m_rigger->GetCurrentAnimationFrameIndex();
			if(index < it->centers.size()) //@todo better thread-sync instead of this
				return it->centers[index];
			else
				return it->centers[0];
		}
		else
			return BaseCollider::GetCenterLocalSpace();
	}
}

//------------------------------------------------------------------
float BoxColliderDynamic::GetRadius() const
{
	if (!m_rigger || m_rigger->GetCurrentAnimation() == nullptr)
		return BaseCollider::GetRadius();
	else
	{
		auto it = std::find_if(m_data.begin(), m_data.end(), [this](const AnimationData& _data) { return _data.name == m_rigger->GetCurrentAnimationName(); });
		if (it != m_data.end())
		{
			if(auto frame_index = m_rigger->GetCurrentAnimationFrameIndex(); frame_index < it->extremDots.size() && frame_index >= 0)
				return it->radiuses[m_rigger->GetCurrentAnimationFrameIndex()];
			else
				return BaseCollider::GetRadius();
		}
		else
			return BaseCollider::GetRadius();
	}
}

//-----------------------------------------------------------------------
void BoxColliderDynamic::SetFrom(const ITransform& _trans)
{
	box = _GetOBB(_trans);
}

//-----------------------------------------------------------------------
void BoxColliderDynamic::SetTo(ITransform& _trans) const
{
	_trans.setRotation(glm::toQuat(box.orientation));
	_trans.setTranslation(box.origin - (_trans.getRotation() * BaseCollider::GetCenterLocalSpace()));
}

//-----------------------------------------------------------------------
dbb::OBB BoxColliderDynamic::_GetOBB(const ITransform& _trans) const
{
	dbb::OBB obb;
	obb.origin = _trans.getModelMatrix() * glm::vec4(BoxColliderDynamic::GetCenterLocalSpace(), 1.0f);
	obb.size = { ((this->GetExtremDotsLocalSpace().MaxX - this->GetExtremDotsLocalSpace().MinX) * _trans.getScaleAsVector().x) / 2,
							 ((this->GetExtremDotsLocalSpace().MaxY - this->GetExtremDotsLocalSpace().MinY) * _trans.getScaleAsVector().y) / 2,
							 ((this->GetExtremDotsLocalSpace().MaxZ - this->GetExtremDotsLocalSpace().MinZ) * _trans.getScaleAsVector().z) / 2 };
	obb.orientation = glm::toMat4(_trans.getRotation());
	return obb;
}

//-----------------------------------------------------------------------
dbb::sphere BoxColliderDynamic::_GetSphere(const ITransform& _trans)
{
	dbb::sphere sphere;
	sphere.position = _trans.getTranslation();
	sphere.radius = ((this->GetExtremDotsLocalSpace().MaxX - this->GetExtremDotsLocalSpace().MinX) * _trans.getScaleAsVector().x) / 2;
	return sphere;
}
