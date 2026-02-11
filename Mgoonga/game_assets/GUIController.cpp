#include "stdafx.h"

#include "GUIController.h"
#include "MainContextBase.h"

#include <math/Rect.h>

#include <opengl_assets/Sound.h>
#include <opengl_assets/GUI.h>
#include <opengl_assets/openglrenderpipeline.h>

#include <glm\glm\gtx\transform.hpp>

//-------------------------------------------------------------
GUIControllerBase::GUIControllerBase(eMainContextBase* _game)
	: m_game(_game)
{
}

//-------------------------------------------------------------
void GUIControllerBase::Initialize()
{
	const Texture* cursor_tex = m_game->GetTexture("cursor1");
	std::shared_ptr<GUI> cursor = std::make_shared<Cursor>(0, 0, 30, 30, m_game->Width(), m_game->Height());
	cursor->SetTexture(*cursor_tex, { 0,0 }, { cursor_tex->m_width, cursor_tex->m_height });
	cursor->SetTransparent(true);
	m_game->AddGUI(cursor);
	m_game->AddInputObserver(cursor.get(), ALWAYS);
	m_game->AddInputObserver(this, ALWAYS);
}

//-------------------------------------------------------------
void GUIControllerMenuWithButtons::Initialize()
{
	// init gui
	glm::vec2 menu_size = { 600 , 400 };
	float pos_x = m_game->Width() / 2 - menu_size.x / 2;
	float pos_y = m_game->Height() / 2 - menu_size.y / 2;

	const Texture* menu_tex = m_game->GetTexture("menu3");
	const Texture* mask_tex = m_game->GetTexture("menu2");
	std::shared_ptr<GUI> menu = std::make_shared<GUI>(pos_x, pos_y, menu_size.x, menu_size.y, m_game->Width(), m_game->Height());
	menu->SetTexture(*menu_tex, { 0,0 }, { menu_tex->m_width, menu_tex->m_height });
	menu->SetTextureMask(*mask_tex);
	menu->SetRenderingFunc(GUI::RenderFunc::CursorFollow);
	menu->SetTakeMouseEvents(true);
	m_game->AddGUI(menu);
	m_game->AddInputObserver(menu.get(), MONOPOLY);

	//@todo should NOT!!! be hardcoded
	//@todo inverted logic for y-axis makes it confusing
	m_buttons.push_back(dbb::Rect{ glm::vec2{260 + pos_x, 710-50 - pos_y}, glm::vec2{90, 35} });
	m_buttons.push_back(dbb::Rect{ glm::vec2{260 + pos_x, 650-50 - pos_y}, glm::vec2{95, 40} });
	m_buttons.push_back(dbb::Rect{ glm::vec2{260 + pos_x, 580-50 - pos_y}, glm::vec2{90, 35} });
	m_buttons.push_back(dbb::Rect{ glm::vec2{260 + pos_x, 525-50 - pos_y}, glm::vec2{85, 30} });

	menu->SetCommand(std::make_shared<GUICommand>([this, menu]()
		{
			for (auto& rect : m_buttons)
			{
				if (rect.IsInside({ m_cursor_x , m_cursor_y }))
				{
					menu->SetVisible(false); // just close the menu if any button is pressed for now
					m_is_menu_active = false;
				}
			}
		}));

	GUIControllerBase::Initialize();
}

//-------------------------------------------------------------
GUIControllerMenuWithButtons::GUIControllerMenuWithButtons(eMainContextBase* _game, eOpenGlRenderPipeline& _pipeline, RemSnd* _page_sound)
	: GUIControllerBase(_game),
  m_pipeline(_pipeline),
	m_page_sound(_page_sound)
{
}

//-------------------------------------------------------------
GUIControllerMenuWithButtons::~GUIControllerMenuWithButtons()
{
}

//-------------------------------------------------------------
void GUIControllerMenuWithButtons::Update(float _tick)
{
	if(m_is_menu_active)
		m_pipeline.get().SetUniformData("class eScreenRender", "CursorPos", glm::vec2(m_cursor_x, m_cursor_y));
}

//----------------------------------------------------------------
bool GUIControllerMenuWithButtons::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
	// it is probably safer to pass in relative coordinates
	/*float curPosX = (float(_x) / m_game->Width());
	float curPosY = (float(_y) / m_game->Height());*/
	m_cursor_x = _x;
	m_cursor_y = m_game->Height() - _y;

	if (m_is_menu_active)
	{
		for (auto& rect : m_buttons)
		{
			if (rect.IsInside({ m_cursor_x , m_cursor_y }))
			{
				if (m_cursor_is_outside_buttons)
				{
					m_cursor_is_outside_buttons = false;
					m_page_sound->Play();
				}
				return false;
			}
		}

		if (!m_cursor_is_outside_buttons)
			m_cursor_is_outside_buttons = true;
	}

	return false;
}

//-----------------------------------------------------------------------
GUIControllerMenuForStairsScript::GUIControllerMenuForStairsScript(eMainContextBase* _game)
	: GUIControllerBase(_game)
{
}

//-----------------------------------------------------------------------
GUIControllerMenuForStairsScript::~GUIControllerMenuForStairsScript()
{
	for (auto& t : m_menu_textures)
		t.freeTexture();
}

//-----------------------------------------------------------------------
void GUIControllerMenuForStairsScript::Initialize()
{
	
}

//-----------------------------------------------------------------------
void GUIControllerMenuForStairsScript::Update(float _tick)
{
	if (m_is_initialized == false)
	{
		// load nec textures
		glm::vec2 menu_size = { 400 , 400 };
		float pos_x = m_game->Width() / 2 - menu_size.x / 2;
		float pos_y = m_game->Height() / 2 - menu_size.y / 2;
		float content_margin = 32.f;
		float top_margin = 0.f;

		// window
		Texture menu_texture;
		menu_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Panel/panel-015.png");
		std::shared_ptr<GUI> menu = std::make_shared<GUI>(pos_x, pos_y, menu_size.x, menu_size.y, m_game->Width(), m_game->Height());
		menu->SetTakeMouseEvents(true);
		menu->SetMovable2D(true);
		menu->SetExecuteOnRelease(true);
		menu->SetTransparent(true);
		menu->SetTexture(menu_texture, { 0,0 }, { menu_texture.m_width, menu_texture.m_height });
		menu->SetRenderingFunc(GUI::RenderFunc::Solid);
		m_game->AddGUI(menu);
		m_game->AddInputObserver(menu.get(), MONOPOLY);

		m_menu_textures.push_back(menu_texture);

		{
			//devider
			Texture devider_texture;
			devider_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Divider/divider-001.png");
			std::shared_ptr<GUI> devider = std::make_shared<GUI>(pos_x + content_margin, pos_y + content_margin,
				menu_size.x - content_margin * 2, menu_size.y / 6,
				m_game->Width(), m_game->Height());
			devider->SetExecuteOnRelease(true);
			devider->SetTransparent(true);
			devider->SetTexture(devider_texture, { 0,0 }, { devider_texture.m_width, devider_texture.m_height });
			devider->SetRenderingFunc(GUI::RenderFunc::Default);
			m_game->AddGUI(devider);

			m_menu_textures.push_back(devider_texture);
			menu->SetChild(devider);

			// slider
			Texture slider_texture;
			slider_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Panel/panel-015.png");
			std::shared_ptr<SliderHorizontal> slider = std::make_shared<SliderHorizontal>(pos_x + content_margin, pos_y + content_margin + 10.f, menu_size.x / 10, menu_size.y / 10, m_game->Width(), m_game->Height());
			slider->SetTakeMouseEvents(true);
			slider->SetMovable2D(true);
			slider->SetExecuteOnRelease(true);
			slider->SetTransparent(true);
			slider->SetTexture(slider_texture, { 0,0 }, { slider_texture.m_width, slider_texture.m_height });
			slider->SetRenderingFunc(GUI::RenderFunc::Default);
			slider->SetMinMaxParentSpace(content_margin, menu_size.x - slider_texture.m_width - content_margin);
			slider->SetParent(menu);
			slider->SetValue(0.33f);
			m_get_slider_one_value = [slider]()->float { return slider->GetValue(); };
			m_game->AddGUI(slider);

			m_menu_textures.push_back(slider_texture);
			menu->SetChild(slider);

			top_margin += (devider_texture.m_height * 2);
		}

		{
			//devider
			Texture devider_texture;
			devider_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Divider/divider-001.png");
			std::shared_ptr<GUI> devider = std::make_shared<GUI>(pos_x + content_margin, pos_y + content_margin + top_margin,
				menu_size.x - content_margin * 2, menu_size.y / 6,
				m_game->Width(), m_game->Height());
			devider->SetExecuteOnRelease(true);
			devider->SetTransparent(true);
			devider->SetTexture(devider_texture, { 0,0 }, { devider_texture.m_width, devider_texture.m_height });
			devider->SetRenderingFunc(GUI::RenderFunc::Default);
			m_game->AddGUI(devider);

			m_menu_textures.push_back(devider_texture);
			menu->SetChild(devider);

			// slider
			Texture slider_texture;
			slider_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Panel/panel-015.png");
			std::shared_ptr<SliderHorizontal> slider = std::make_shared<SliderHorizontal>(pos_x + content_margin, pos_y + content_margin + 10.f + top_margin,
																																										menu_size.x / 10, menu_size.y / 10, 
																																										m_game->Width(), m_game->Height());
			slider->SetTakeMouseEvents(true);
			slider->SetMovable2D(true);
			slider->SetExecuteOnRelease(true);
			slider->SetTransparent(true);
			slider->SetTexture(slider_texture, { 0,0 }, { slider_texture.m_width, slider_texture.m_height });
			slider->SetRenderingFunc(GUI::RenderFunc::Default);
			slider->SetMinMaxParentSpace(content_margin, menu_size.x - slider_texture.m_width - content_margin);
			slider->SetParent(menu);
			slider->SetValue(0.68f);
			m_get_slider_two_value = [slider]()->float { return slider->GetValue(); };
			m_game->AddGUI(slider);

			m_menu_textures.push_back(slider_texture);
			menu->SetChild(slider);

			top_margin += (devider_texture.m_height*2) + 20;
		}

		{
			//button start
			Texture button_texture;
			button_texture.loadTextureFromFile("../game_assets/assets/Kenny/kenney_fantasy-ui-borders/PNG/Default/Panel/panel-015.png");

			std::shared_ptr<GUI> button_start = std::make_shared<GUI>(pos_x + content_margin, pos_y + content_margin + top_margin,
																																menu_size.x / 3, menu_size.y / 10,
																																m_game->Width(), m_game->Height());
			button_start->SetTransparent(true);
			button_start->SetTexture(button_texture, { 0,0 }, { button_texture.m_width, button_texture.m_height });
			button_start->SetRenderingFunc(GUI::RenderFunc::Default);
			button_start->SetTakeMouseEvents(true);
			button_start->SetCommand(std::make_shared<GUICommand>([this]() 
				{ 
					this->m_start_called = true;
				}));
			m_game->AddGUI(button_start);

			m_menu_textures.push_back(button_texture);
			menu->SetChild(button_start);

			std::shared_ptr<Text> start_t = std::make_shared<Text>();
			start_t->font = "ARIALN";
			start_t->pos_x = button_start->GetTopLeft().x + 20.f;
			start_t->pos_y = m_game->Height() - button_start->GetTopLeft().y - button_start->GetSize().y + 6.f;
			start_t->scale = 0.9f;
			start_t->color = glm::vec3(0.0, 0.0f, 0.5f);
			start_t->mvp = glm::ortho(0.0f, (float)m_game->Width(), 0.0f, (float)m_game->Height());
			start_t->content = "start";
			menu->SetChildText(start_t);
			m_game->AddText(start_t);

			//button reset
			std::shared_ptr<GUI> button_reset = std::make_shared<GUI>(pos_x + content_margin + menu_size.x/2, pos_y + content_margin + top_margin,
																																menu_size.x / 3, menu_size.y / 10,
																																m_game->Width(), m_game->Height());
			button_reset->SetTransparent(true);
			button_reset->SetTexture(button_texture, { 0,0 }, { button_texture.m_width, button_texture.m_height });
			button_reset->SetRenderingFunc(GUI::RenderFunc::Default);
			button_reset->SetTakeMouseEvents(true);
			button_reset->SetCommand(std::make_shared<GUICommand>([this]()
				{
					this->m_reset_called = true;
				}));
			m_game->AddGUI(button_reset);
			menu->SetChild(button_reset);

			std::shared_ptr<Text> reset_t = std::make_shared<Text>();
			reset_t->font = "ARIALN";
			reset_t->pos_x = button_start->GetTopLeft().x + 20.f + (menu->GetSize().x / 2);
			reset_t->pos_y = m_game->Height() - button_start->GetTopLeft().y - button_start->GetSize().y + 6.f;
			reset_t->scale = 0.9f;
			reset_t->color = glm::vec3(0.0, 0.0f, 0.5f);
			reset_t->mvp = glm::ortho(0.0f, (float)m_game->Width(), 0.0f, (float)m_game->Height());
			reset_t->content = "reset";
			menu->SetChildText(reset_t);
			m_game->AddText(reset_t);

			top_margin += (button_texture.m_height * 2); // menu_size.y / 10 * 2 ?

			//button flip
			std::shared_ptr<GUI> button_flip = std::make_shared<GUI>(pos_x + content_margin + menu_size.x / 2, pos_y + content_margin + top_margin,
																															 menu_size.x / 3, menu_size.y / 10,
																															 m_game->Width(), m_game->Height());
			button_flip->SetTransparent(true);
			button_flip->SetTexture(button_texture, { 0,0 }, { button_texture.m_width, button_texture.m_height });
			button_flip->SetRenderingFunc(GUI::RenderFunc::Default);
			button_flip->SetTakeMouseEvents(true);
			button_flip->SetCommand(std::make_shared<GUICommand>([this]()
				{
					this->m_flip_called = true;
				}));
			m_game->AddGUI(button_flip);
			menu->SetChild(button_flip);

			std::shared_ptr<Text> flip_t = std::make_shared<Text>();
			flip_t->font = "ARIALN";
			flip_t->pos_x = button_flip->GetTopLeft().x + 40.f;
			flip_t->pos_y = m_game->Height() - button_flip->GetTopLeft().y - button_flip->GetSize().y + 6.f;
			flip_t->scale = 0.9f;
			flip_t->color = glm::vec3(0.0, 0.0f, 0.5f);
			flip_t->mvp = glm::ortho(0.0f, (float)m_game->Width(), 0.0f, (float)m_game->Height());
			flip_t->content = "flip";
			menu->SetChildText(flip_t);
			m_game->AddText(flip_t);

			m_plane_t = std::make_shared<Text>();
			m_plane_t->font = "ARIALN";
			m_plane_t->pos_x = button_start->GetTopLeft().x + 20.f;
			m_plane_t->pos_y = m_game->Height() - button_flip->GetTopLeft().y - button_flip->GetSize().y + 6.f;
			m_plane_t->scale = 0.9f;
			m_plane_t->color = glm::vec3(1.0, 1.0f, 1.0f);
			m_plane_t->mvp = glm::ortho(0.0f, (float)m_game->Width(), 0.0f, (float)m_game->Height());
			m_plane_t->content = "AX";
			menu->SetChildText(m_plane_t);
			m_game->AddText(m_plane_t);

			top_margin += (button_texture.m_height * 2); // // menu_size.y / 10 * 2 ?

			//button close		
			std::shared_ptr<GUI> button_close = std::make_shared<GUI>(pos_x + content_margin, pos_y + content_margin + top_margin,
																																menu_size.x - content_margin * 2, menu_size.y / 10,
																																m_game->Width(), m_game->Height());
			button_close->SetTransparent(true);
			button_close->SetTexture(button_texture, { 0,0 }, { button_texture.m_width, button_texture.m_height });
			button_close->SetRenderingFunc(GUI::RenderFunc::Default);
			button_close->SetTakeMouseEvents(true);
			button_close->SetCommand(std::make_shared<GUICommand>([menu]()
				{
					menu->SetVisible(false);
				}));
			m_game->AddGUI(button_close);
			menu->SetChild(button_close);

			std::shared_ptr<Text> close_t = std::make_shared<Text>();
			close_t->font = "ARIALN";
			close_t->pos_x = button_close->GetTopLeft().x + (button_close->GetSize().x/ 2) - 30.f;
			close_t->pos_y = m_game->Height() - button_close->GetTopLeft().y - button_close->GetSize().y + 6.f;
			close_t->scale = 0.9f;
			close_t->color = glm::vec3(0.0, 0.0f, 0.5f);
			close_t->mvp = glm::ortho(0.0f, (float)m_game->Width(), 0.0f, (float)m_game->Height());
			close_t->content = "close";
			menu->SetChildText(close_t);
			m_game->AddText(close_t);
		}

		GUIControllerBase::Initialize();

		m_is_initialized = true;
	}
}

//-----------------------------------------------------------------------
bool GUIControllerMenuForStairsScript::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
	return false;
}

bool GUIControllerMenuForStairsScript::IsInitialized() const
{
	return m_is_initialized;
}

std::function<float()> GUIControllerMenuForStairsScript::GetSliderOneCallback()
{
	return m_get_slider_one_value;
}

std::function<float()> GUIControllerMenuForStairsScript::GetSliderTwoCallback()
{
	return m_get_slider_two_value;
}

//-----------------------------------------------------------------------
bool GUIControllerMenuForStairsScript::GetStartCalled()
{
	if (m_start_called)
	{
		m_start_called = false;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------
bool GUIControllerMenuForStairsScript::GetResetCalled()
{
	if (m_reset_called)
	{
		m_reset_called = false;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------
bool GUIControllerMenuForStairsScript::GetFlipCalled()
{
	if (m_flip_called)
	{
		m_flip_called = false;
		return true;
	}
	return false;
}

std::shared_ptr<Text> GUIControllerMenuForStairsScript::GetPlaneText() const
{
	return m_plane_t;
}