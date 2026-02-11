#include "stdafx.h"
#include "Object.h"

//----------------------------------------------------------------------------
eObject::~eObject()
{
	script.release();
}

//----------------------------------------------------------------------------
bool eObject::operator==(const eObject& _other)
{
	return GetTransform()->getTranslation() == _other.GetTransform()->getTranslation() 
			&& GetModel() == _other.GetModel();
}

//----------------------------------------------------------------------------
bool eObject::operator!=(const eObject& _other)
{
	return !operator==(_other);
}

//---------------------------------------------------------------------------
void eObject::SetVisible(bool _visible)
{
	m_is_visible = _visible;
}

//---------------------------------------------------------------------------
void eObject::SetBackfaceCull(bool _is_backface_cull)
{
	m_is_backface_cull = _is_backface_cull;
}

//---------------------------------------------------------------------------
void eObject::SetInstancingTag(const std::string& _tag)
{
	m_instancing_tag = _tag;
}

//-------------------------------------------------------------------------
const glm::mat4& eObject::GetInstancedInfo() const
{
	return m_instanced_info;
}

//---------------------------------------------------------------------------
void eObject::SetInstancedInfo(const glm::mat4& _info)
{
	m_instanced_info = _info;
	m_has_instanced_info = m_instanced_info != UNIT_MATRIX ? true : false;
}

//---------------------------------------------------------------------------
void eObject::SetRigger(IRigger * _r)
{
	rigger.reset(_r);
}

//----------------------------------------------------------------------------
void eObject::SetScript(IScript* _script, std::function<void(IScript*)> _deleter)
{
	script = ScriptPtr(_script, _deleter);
	if (_script != nullptr)
		_script->SetObject(shared_from_this());
}
//-----------------------------------------------------------------------------
void eObject::SetTransform(ITransform* _t)
{
	transform.reset(_t);
}

//-----------------------------------------------------------------------------
void eObject::SetCollider(ICollider* _c)
{
	collider.reset(_c);
	if(rigid_body)
		rigid_body->SetCollider(collider.get());
}

//-----------------------------------------------------------------------------
void eObject::SetModel(IModel* _m)
{
	model.reset(_m);
}

//-----------------------------------------------------------------------------
void eObject::SetModel(std::shared_ptr<IModel> _m)
{
	model = _m;
}

//-----------------------------------------------------------------------------
void eObject::SetRigidBody(IRigidBody* _rb)
{
	rigid_body.reset(_rb);
}

//------------------------------------------------------------------------------
void eObject::SetTransparent(bool _is_transparent)
{
	if (_is_transparent)
		m_render_type = RenderType::PBR_TRANSPARENT;
	else if (!_is_transparent && m_is_transparent)
		m_render_type = RenderType::PBR;
	m_is_transparent = _is_transparent;
}

IScript*			eObject::GetScript()	const { return script.get(); }
ITransform*			eObject::GetTransform()	const { return transform.get();	}
ICollider*			eObject::GetCollider()	const { return collider.get(); }
IModel*				eObject::GetModel()		const { return model.get(); }
IRigger*			eObject::GetRigger()	const { return rigger.get(); }
std::shared_ptr<IRigidBody> eObject::GetRigidBody() const { return rigid_body; }


std::vector<std::shared_ptr<eObject>> GetObjectsWithChildren(std::vector<std::shared_ptr<eObject>> _objects)
{
	auto all_objects = _objects;
	for (auto& obj : _objects)
		for (auto& child : obj->GetChildrenObjects())
			all_objects.push_back(child);
	return all_objects;
}
