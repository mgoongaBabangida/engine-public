#include "stdafx.h"
#include "GUI.h"

#include <memory>
#include "SimpleColorFbo.h"

//-------------------------------------------------------
GUI::GUI()
{
}

//-------------------------------------------------------
GUI::GUI(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight)
	: m_topleftX(topleftX),
		m_topleftY(topleftY),
		m_width(Width),
		m_height(Height),
		m_screen_width(scWidth),
		m_screen_height(scHeight)
	{}

//-------------------------------------------------------
GUI::GUI(const dbb::Rect& _rect, int scWidth, int scHeight)
	: m_topleftX(_rect.m_top_left.x),
		m_topleftY(_rect.m_top_left.y),
		m_width(_rect.m_size.x),
		m_height(_rect.m_size.y),
		m_screen_width(scWidth),
		m_screen_height(scHeight)
{}

//-------------------------------------------------------
GUI::GUI(const GUI& _other)
	: m_screen_width(_other.m_screen_width)
  , m_screen_height(_other.m_screen_height)
  , m_texture(_other.m_texture)
	, m_texture_mask(_other.m_texture_mask)
  , m_topleftX(_other.m_topleftX)
  , m_topleftY(_other.m_topleftY)
  , m_width(_other.m_width)
  , m_height(_other.m_height)
  , m_cmd(_other.m_cmd)
  , m_is_visible(_other.m_is_visible)
{
}

//-------------------------------------------------------
GUI::~GUI()
{
}

//-------------------------------------------------------
bool	GUI::IsVisible() const { return m_is_visible; }

//-------------------------------------------------------
void	GUI::SetVisible(bool _isVisible) 
{ 
	m_is_visible = _isVisible;
	for (auto& child : m_children)
		child->SetVisible(_isVisible);
	for (auto& text : m_children_text)
		text->visible = _isVisible;
}

//-------------------------------------------------------
bool	GUI::IsTransparent() const { return m_is_transparent; }
void	GUI::SetTransparent(bool _isTransparent) { m_is_transparent = _isTransparent; }

//-------------------------------------------------------
bool	GUI::IsMovable2D() const { return m_is_moveble2d; }
void	GUI::SetMovable2D(bool _isMovable2D) { m_is_moveble2d = _isMovable2D; }

//-------------------------------------------------------
bool	GUI::IsExecuteOnRelease() const { return m_is_execute_on_release; }
void	GUI::SetExecuteOnRelease(bool _is_execute_on_release) { m_is_execute_on_release = _is_execute_on_release; }

//-------------------------------------------------------
bool	GUI::IsTakingMouseEvents() const { return m_take_mouse_moves; }
void	GUI::SetTakeMouseEvents(bool _take_mouse_moves) { m_take_mouse_moves = _take_mouse_moves; }

//-------------------------------------------------------
GUI::RenderFunc GUI::GetRenderingFunc() const { return m_render_func; }
void GUI::SetRenderingFunc(RenderFunc _func) { m_render_func = _func; }

//-------------------------------------------------------
float GUI::GetRotationAngle() const
{
	return m_rotation_angle;
}

//-------------------------------------------------------
void GUI::SetRotationAngle(float _rotation_angle)
{
	m_rotation_angle = _rotation_angle;
}

//-------------------------------------------------------
void GUI::Move(glm::ivec2 _newTopLeft)
{
	m_topleftX = _newTopLeft.x;
	m_topleftY = _newTopLeft.y;
}

//-------------------------------------------------------
bool GUI::OnMousePress(int32_t _x, int32_t _y, bool left, KeyModifiers _modifier)
{
	if(left && m_is_visible && m_take_mouse_moves &&  IsPressed(_x, _y))
	{
		for (auto& child : m_children)
		{
			if (child->OnMousePress(_x, _y, left, _modifier))
				return true;
		}
		// if none of children has taken the event than the parrent takes it
		m_is_pressed = true;
		m_press_coords = { _x - m_topleftX , _y - m_topleftY };
		Perssed();
		return true;
	}
	return false;
}

//-------------------------------------------------------------
bool GUI::OnMouseRelease(KeyModifiers _modifier)
{
	for (auto& child : m_children)
	{
		if (child->OnMouseRelease(_modifier))
			return true;
	}
	// if none of children has taken the event than the parrent takes it

	if (m_is_pressed)
	{
		if (m_cmd && m_is_execute_on_release)
			m_cmd->Execute();
		m_is_pressed = false;
		return true;
	}
	return false;
}

//-------------------------------------------------------
bool GUI::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
	m_mouse_pose = glm::ivec2{ _x,  _y };
	if (m_take_mouse_moves && m_is_visible)
	{
		for (auto& child : m_children)
		{
			if (child->OnMouseMove(_x, _y, _modifier))
				return true;
		}
	}

	if (m_is_pressed && m_take_mouse_moves && m_is_moveble2d && m_is_visible)
	{
		// if none of children has taken the event than the parrent takes it
		for (auto& child : m_children)
		{
			int offsetX = child->m_topleftX - m_topleftX;
			int offsetY = child->m_topleftY - m_topleftY;
			child->Move({ _x - m_press_coords.first + offsetX, _y - m_press_coords.second + offsetY });
		}
		for (auto& text : m_children_text)
		{
			int offsetX = text->pos_x - m_topleftX;
			int offsetY = (m_screen_height - text->pos_y) - m_topleftY; //flip Y
			text->pos_x = _x - m_press_coords.first + offsetX;
			text->pos_y = m_screen_height - (_y - m_press_coords.second + offsetY);
		}
		Move({ _x - m_press_coords.first, _y - m_press_coords.second });
	}

	if (IsHover() && m_is_visible)
		return m_take_mouse_moves;
	else
		return false;
}

//-------------------------------------------------------
void GUI::UpdateSync()
{
	if (IsHover() && m_hover_cmd.get())
		m_hover_cmd->Execute();
}

//-------------------------------------------------------
void GUI::SetCommand(std::shared_ptr<ICommand> com)
{
	m_cmd = com;
}

//-------------------------------------------------------
void GUI::SetHoverCommand(std::shared_ptr<ICommand> com)
{
	m_hover_cmd = com;
}

//-------------------------------------------------------
void GUI::SetTexture(const Texture& t, glm::ivec2 topLeft, glm::ivec2 bottomRight)
{
	m_texture = t;
	m_tex_topleftX = topLeft.x;
	m_tex_topleftY = topLeft.y;
	m_tex_width = bottomRight.x - topLeft.x;
	m_tex_height = bottomRight.y - topLeft.y;
}

//-------------------------------------------------------
void GUI::SetTextureMask(const Texture& t)
{
	m_texture_mask = t;
}

//-------------------------------------------------------
Texture * GUI::GetTexture()
{
	 return &m_texture;
}

//-------------------------------------------------------
Texture* GUI::GetTextureMask()
{
	return &m_texture_mask;
}

//-------------------------------------------------------
void GUI::Perssed()
{
  if(m_cmd && !m_is_execute_on_release)
		m_cmd->Execute();
}

//-------------------------------------------------------
bool GUI::IsHover()
{
	return m_mouse_pose.x > m_topleftX && m_mouse_pose.y > m_topleftY && m_mouse_pose.x < (m_topleftX + m_width) && m_mouse_pose.y < (m_topleftY + m_height);
}

//-------------------------------------------------------
bool GUI::IsPressed(int x, int y)
{
	return x > m_topleftX && y > m_topleftY && x < (m_topleftX + m_width) && y < (m_topleftY + m_height);
}

//-------------------------------------------------------
glm::ivec4 GUI::GetViewPort() const
{
	return glm::ivec4(m_topleftX, m_screen_height - m_topleftY - m_height, m_width, m_height);
}

//-------------------------------------------------------
glm::ivec2 GUI::GetTopLeft() const
{
	return glm::ivec2(m_topleftX, m_topleftY);
}

//-------------------------------------------------------
glm::ivec2 GUI::GetBottomRight() const
{
	return glm::ivec2(m_topleftX + m_width, m_topleftY + m_height);
}

//-------------------------------------------------------
glm::ivec2 GUI::GetSize() const
{
	return glm::ivec2(m_width, m_height);
}

//-------------------------------------------------------
glm::ivec2 GUI::GetTopLeftTexture() const
{
	return glm::ivec2(m_tex_topleftX, m_tex_topleftY);
}

//-------------------------------------------------------
glm::ivec2 GUI::GetBottomRightTexture() const
{
	return glm::ivec2(m_tex_topleftX + m_tex_width, m_tex_topleftY + m_tex_height);
}

//-------------------------------------------------------
std::pair<uint32_t, uint32_t> GUI::PointOnGUI(uint32_t x_window, uint32_t y_window)
{
	//works if it is inside
	return std::pair<uint32_t, uint32_t>(x_window - m_topleftX, y_window- m_topleftY);
}

//---------------------------------------------------------------------------------
GUIWithAlpha::GUIWithAlpha(int topleftX, int topleftY, int Width, int Height, int scWidth, int scHeight)
: GUI(topleftX, topleftY, Width, Height, scWidth, scHeight)
{
}

//---------------------------------------------------------------------------------
GUIWithAlpha::~GUIWithAlpha()
{
}

//------------------------------------------------------
bool GUIWithAlpha::IsPressed(int _x, int _y)
{
	m_check_if_pressed = GUI::IsPressed(_x, _y);
	m_press_coords = { _x ,  _y };
	return m_check_if_pressed;
}

//------------------------------------------------------
void GUIWithAlpha::Perssed()
{
	if (!m_check_if_pressed)
		GUI::Perssed();
}

//--------------------------------------------------
void GUIWithAlpha::UpdateSync()
{
	GUI::UpdateSync();
	if (m_check_if_pressed)
	{
		uint8_t* imData = m_texture.getPixelBuffer();
		GLubyte r, g, b, a; // or GLubyte r, g, b, a;
		
		auto[x, y] = PointOnGUI(m_press_coords.first, m_press_coords.second);// line and column of the pixel
		y = m_height - y; // invert y to bottom left coord system
		int32_t elmes_per_line = m_texture.m_width * 4; // elements per line = 256 * "RGBA"

		float x_coef = static_cast<float>(x) / static_cast<float>(m_width);
		float y_coef = static_cast<float>(y) / static_cast<float>(m_height);

		int32_t tex_x = m_tex_topleftX + (x_coef * m_tex_width);
		int32_t tex_y = m_tex_topleftY + (y_coef * m_tex_height);

		int32_t row = tex_y * elmes_per_line;
		int32_t col = tex_x * 4;

		r = imData[row + col];
		g = imData[row + col + 1];
		b = imData[row + col + 2];
		a = imData[row + col + 3];

		m_check_if_pressed = false;
		if (a > 0)
			Perssed();

		free(imData);
	}
}

//-------------------------------------------------------------
bool Cursor::OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifier)
{
	Move({ x,y });
	return false;
}

//--------------------------------------------------
AnimStart::AnimStart(shObject obj)
	:m_obj(obj)
{
}

AnimStart::AnimStart(const AnimStart& _other)
	:m_obj(_other.m_obj)
{
}

void AnimStart::Execute()
{
	m_obj->GetRigger()->Apply("Running", false);
}

AnimStop::AnimStop(shObject obj)
	:m_obj(obj)
{
}

AnimStop::AnimStop(const AnimStop& _other)
	: m_obj(_other.m_obj)
{
}

void AnimStop::Execute()
{
	m_obj->GetRigger()->Stop();
}

//--------------------------------------
CommandTest::CommandTest()
{
}

void CommandTest::Execute()
{
}

//--------------------------------------
void MenuBehavior::Execute()
{
	gui->SetVisible(!gui->IsVisible());
}

//--------------------------------------
void MenuBehaviorLeanerMove::Execute()
{
	if (!anim.IsOn())
	{
		anim.Start();
		timer.reset(new math::Timer([this]()->bool
			{
				std::vector<glm::vec3> frame = anim.getCurrentFrame();
				gui->Move({ frame[0].x, frame[0].y });
				gui->SetVisible(true);
				return true;
			}));
		timer->start(10);
	}
	else
	{
		gui->SetVisible(false);
		timer->stop();
		anim.Reset();
	}
}

MenuBehaviorLeanerMove::~MenuBehaviorLeanerMove()
{
	if (anim.IsOn())
	{
		gui->SetVisible(false);
		anim.Reset();
		if(timer.get())
			timer->stop();
	}
}

//-------------------------------------------------------------------------------------
bool SliderHorizontal::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
	if (m_is_pressed && m_take_mouse_moves && m_is_moveble2d && m_is_visible)
	{
		int32_t min_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_min : m_width_min;
		int32_t max_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_max : m_width_max;
		if (_x - m_press_coords.first > min_width && _x - m_press_coords.first < max_width)
		{
			Move({ _x - m_press_coords.first, m_topleftY }); // move only over X-axis
			return true;
		}
	}
		return false;
}

//----------------------------------------------------------------------
void SliderHorizontal::SetMinMaxParentSpace(int32_t _min, int32_t _max)
{
	m_width_min = _min;
	m_width_max = _max;
}

//----------------------------------------------------------------------
float SliderHorizontal::GetValue()
{
	int32_t min_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_min : m_width_min;
	int32_t max_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_max : m_width_max;
	int32_t value_x = m_topleftX - min_width;
	return (float)value_x  / float(max_width - min_width);
}

//----------------------------------------------------------------------
void SliderHorizontal::SetValue(float _val)
{
	float val = glm::clamp(_val, 0.f , 1.f);
	int32_t min_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_min : m_width_min;
	int32_t max_width = m_parent.get() ? m_parent->GetTopLeft().x + m_width_max : m_width_max;
	m_topleftX = min_width + (max_width - min_width) * val;
}
