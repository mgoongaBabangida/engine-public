#pragma once

#include "interfaces.h"

//---------------------------------------------------------------------------------
class DLL_BASE eObject: public std::enable_shared_from_this<eObject>
{
public:

	enum class RenderType : uint8_t
	{
		PHONG,
		PBR,
		FLAG,
		OUTLINED,
		GEOMETRY,
		BEZIER_CURVE,
		LINES,
		AREA_LIGHT_ONLY,
		TERRAIN_TESSELLATION,
		VOLUMETRIC,
		ENVIRONMENT_PROBE,
		LEAF,
		UI_SYSTEM,
		PBR_TRANSPARENT,
		COUNT
	};

	explicit eObject() {} //@todo
	virtual ~eObject();

	bool operator==(const eObject&);
	bool operator!=(const eObject&);

	//Properties
	bool IsVisible() const { return m_is_visible; }
	void SetVisible(bool _visible);

	bool IsPickable() const { return m_is_pickable; }
	void SetPickable(bool _pickable) { m_is_pickable = _pickable; }

	bool IsTransparent() const { return m_is_transparent; }
	void SetTransparent(bool _is_transparent);

	bool IsBackfaceCull() const { return m_is_backface_cull; }
	void SetBackfaceCull(bool _is_backface_cull);

	bool Is2DScreenSpace() const { return m_is_2d_screen_space; }
	void Set2DScreenSpace(bool _2d) { m_is_2d_screen_space = _2d; }

	bool IsTextureBlending() const { return m_is_texture_blending; }
	void SetTextureBlending(bool _tex_blending) { m_is_texture_blending = _tex_blending; }

	bool IsFadeAlpha() const { return m_fade_alpha; }
	void SetFadeAlpha(bool _fade_alpha) { m_fade_alpha = _fade_alpha; }

	void SetInstancingTag(const std::string& _tag);
	const std::string& GetInstancingTag() const { return m_instancing_tag; }

	RenderType GetRenderType() const { return m_render_type; }
	void SetRenderType(RenderType _render_type) { m_render_type = _render_type; }

	std::vector<std::shared_ptr<eObject>>&	GetChildrenObjects() { return m_children; }
	void AddChildObject(std::shared_ptr<eObject> _obj) { m_children.push_back(_obj); }

	bool& GetTransparent() { return m_is_transparent; }

	bool							HasInstancedInfo() const { return m_has_instanced_info; }
	const glm::mat4&	GetInstancedInfo() const;
	void							SetInstancedInfo(const glm::mat4&);

	using ScriptPtr = std::unique_ptr<IScript, std::function<void(IScript*)>>;

	//Setters
	void				SetRigger(IRigger* r);
	void				SetScript(IScript* script, std::function<void(IScript*)> deleter = [](IScript* p) { delete p; });
	void				SetTransform(ITransform*);
	void				SetCollider(ICollider*);
	void				SetModel(IModel*);
	void				SetModel(std::shared_ptr<IModel>);
	void 				SetName(const std::string& _name) { name = _name; }
	void				SetRigidBody(IRigidBody*);

	//Getters
	IScript*										GetScript()		const;
	ITransform*									GetTransform()	const;
	ICollider*									GetCollider()	const;
	IModel*											GetModel()		const;
	IRigger*										GetRigger()		const;
	std::shared_ptr<IRigidBody>	GetRigidBody() const;
	const std::string&					Name()			const	{ return name; }

protected:
	std::shared_ptr<IModel>				model;
	ScriptPtr											script;
	std::unique_ptr<ITransform>		transform;
	std::unique_ptr<ICollider>		collider;
	std::unique_ptr<IRigger>			rigger;
	std::shared_ptr<IRigidBody>		rigid_body;
	std::string										name;
	std::vector<std::shared_ptr<eObject>> m_children;

	bool m_is_visible = true;
	bool m_is_pickable = true;
	bool m_is_transparent = false;
	bool m_fade_alpha = false;
	bool m_is_backface_cull = true;
	bool m_is_2d_screen_space = false;
	bool m_is_texture_blending = false;
	RenderType m_render_type = RenderType::PHONG;
	std::string m_instancing_tag;
	bool m_has_instanced_info = false;
	glm::mat4 m_instanced_info;
};

using shObject = std::shared_ptr<eObject>;

// Compile-time safe indexer
constexpr size_t to_index(eObject::RenderType t) {
	return static_cast<size_t>(t);
}

using RenderBuckets = std::array<std::vector<shObject>, to_index(eObject::RenderType::COUNT)>;

std::vector<shObject> DLL_BASE GetObjectsWithChildren(std::vector<shObject>);