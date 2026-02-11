#ifndef GUI_H
#define GUI_H

#include "opengl_assets.h"

#include <base/interfaces.h>
#include <base/Object.h>

#include <math/Timer.h>
#include <math/AnimationLinear.h>
#include <math/Rect.h>

#include "Texture.h"

//-------------------------------------------------------------------------------------------------
class DLL_OPENGL_ASSETS GUI : public IInputObserver
{
public:
	enum RenderFunc
	{
		Default = 0,
		CursorFollow = 1,
		GreyKernel = 2,
		MaskBlend = 3,
		Solid = 4,
		Gradient = 5
	};

	GUI();
	GUI(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight);
	GUI(const dbb::Rect&, int scWidth, int scHeight);
	GUI(const GUI&);
	virtual ~GUI();

	virtual bool	OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier) override;
	virtual bool	OnMouseRelease(KeyModifiers _modifier) override;
	virtual bool	OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier)override;

	virtual void UpdateSync();

	void			  SetCommand(std::shared_ptr<ICommand> com);
	void			  SetHoverCommand(std::shared_ptr<ICommand> com);
	void			  SetTexture(const Texture& t,
												glm::ivec2 topLeft,
												glm::ivec2 bottomRight);
	void			  SetTextureMask(const Texture& t); //should it have its topLeft & bottomRight?

	Texture*		GetTexture();
	Texture*		GetTextureMask();

	void															SetChild(std::shared_ptr<GUI>_child) { m_children.push_back(_child); }
	std::vector<std::shared_ptr<GUI>> GetChildren() const { return m_children; }

	void									SetParent(std::shared_ptr<GUI> _parent) { m_parent = _parent; }
	std::shared_ptr<GUI>	GetParent() const { return m_parent; }

	void																SetChildText(std::shared_ptr<Text>_child) { m_children_text.push_back(_child); }
	std::vector<std::shared_ptr<Text>>	GetChildrenText() const { return m_children_text; }

	void					virtual Perssed();

	bool									IsHover();
	bool					virtual IsPressed(int x, int y);

	bool					IsVisible() const;
	void					SetVisible(bool _isVisible);

	bool					IsTransparent() const;
	void					SetTransparent(bool _isTransparent);

	bool					IsMovable2D() const;
	void					SetMovable2D(bool _isMovable2D);

	bool					IsExecuteOnRelease() const;
	void					SetExecuteOnRelease(bool _is_execute_on_release);

	bool					IsTakingMouseEvents() const;
	void					SetTakeMouseEvents(bool _take_mouse_moves);

	RenderFunc		GetRenderingFunc() const;
	void					SetRenderingFunc(RenderFunc _func);

	float					GetRotationAngle() const;
	void					SetRotationAngle(float);

	void					Move(glm::ivec2 _newTopLeft);

	glm::ivec4		GetViewPort() const;
	glm::ivec2		GetTopLeft() const;
	glm::ivec2		GetBottomRight() const;
	glm::ivec2		GetSize() const;

	glm::ivec2		GetTopLeftTexture() const;
	glm::ivec2		GetBottomRightTexture() const;

	std::pair<uint32_t, uint32_t> PointOnGUI(uint32_t x_window, uint32_t y_window);

protected:
	int32_t						m_screen_width;
	int32_t						m_screen_height;

	Texture						m_texture;
	Texture						m_texture_mask;

	int32_t						m_topleftX;
	int32_t						m_topleftY;
	int32_t						m_width;
	int32_t						m_height;

	int32_t						m_tex_topleftX;
	int32_t						m_tex_topleftY;
	int32_t						m_tex_width;
	int32_t						m_tex_height;
	glm::ivec2				m_mouse_pose;

	std::shared_ptr<GUI>								m_parent;
	std::vector<std::shared_ptr<GUI>>		m_children;
	std::vector<std::shared_ptr<Text>>	m_children_text;
	std::shared_ptr<ICommand>						m_cmd;
	std::shared_ptr<ICommand>						m_hover_cmd;

	bool											m_is_visible = true;
	bool											m_is_transparent = false;
	bool											m_take_mouse_moves = false;
	bool											m_is_moveble2d = false;
	RenderFunc								m_render_func = RenderFunc::Default;
	std::pair<size_t, size_t> m_press_coords;
	bool											m_is_pressed = false;
	bool											m_is_execute_on_release = false;
	float											m_rotation_angle = 0.f;
};

//-------------------------------------------------------------------------------------------------
class DLL_OPENGL_ASSETS SliderHorizontal : public GUI
{
public:
	SliderHorizontal(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight) 
		:GUI(topleftX, topleftY, Width, Height, scWidth, scHeight) {}

	virtual bool	OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier) override;

	void	SetMinMaxParentSpace(int32_t _min, int32_t _max);
	float GetValue();
	void SetValue(float _val);

protected:
	int32_t m_width_min = 0;
	int32_t m_width_max = 0;
};

//-------------------------------------------------------------------------------------------------
class DLL_OPENGL_ASSETS GUIWithAlpha : public GUI
{
public:
	GUIWithAlpha(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight);
	virtual ~GUIWithAlpha();

	virtual bool IsPressed(int x, int y) override;
	virtual void Perssed() override;
	virtual void UpdateSync() override;
protected:
	bool m_check_if_pressed = false;
};

//----------------------------------------------
class DLL_OPENGL_ASSETS Cursor : public GUIWithAlpha
{
public:
	Cursor(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight)
		: GUIWithAlpha(topleftX, topleftY, Width, Height, scWidth, scHeight) {}

	virtual bool	OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifier) override;
};

//------------------------------------------------------
struct GUICommand : public ICommand
{
	GUICommand(std::function<void()> _func)
		:m_func(_func)
	{}
	virtual void Execute()
	{
		m_func();
	}
	std::function<void()> m_func;
};

//----------------------------------------------
class DLL_OPENGL_ASSETS MenuBehavior : public ICommand
{
public:
	MenuBehavior(GUI* _g) :gui(_g) { _g->SetVisible(false); }
	MenuBehavior(const MenuBehavior&) = delete;
	virtual void Execute() override;
protected:
	GUI* gui = nullptr;
};

//----------------------------------------------
class DLL_OPENGL_ASSETS MenuBehaviorLeanerMove : public ICommand
{
public:
	MenuBehaviorLeanerMove(GUI* _g, math::AnimationLinear<glm::vec3>&& _anim) :gui(_g), anim(std::move(_anim)) {}
	MenuBehaviorLeanerMove(const MenuBehaviorLeanerMove&) = delete;
	virtual void Execute() override;
	~MenuBehaviorLeanerMove();

protected:
	GUI* gui = nullptr;
	std::unique_ptr<math::Timer> timer;
	math::AnimationLinear<glm::vec3> anim;
};

//----------------------------------------------
class DLL_OPENGL_ASSETS CommandTest : public ICommand
{
public:
	CommandTest(/*Game* g*/) /*:invoker(g)*/;
	CommandTest(const CommandTest&) = delete;
	virtual void Execute() override;
};

//-----------------------------------------------
class DLL_OPENGL_ASSETS AnimStart : public ICommand
{
	shObject m_obj;
public:
	AnimStart(shObject obj);
	AnimStart(const AnimStart&);
	virtual void Execute() override;
};

//-----------------------------------------------
class DLL_OPENGL_ASSETS AnimStop : public ICommand
{
	shObject m_obj;
public:
	AnimStop(shObject obj);
	AnimStop(const AnimStop&);
	virtual void Execute() override;
};

#endif

