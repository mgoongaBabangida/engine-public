#pragma once

#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Object.h>

#include <math/Rect.h>

#include <opengl_assets/Texture.h>

class eMainContextBase;
class eOpenGlRenderPipeline;
class RemSnd;

//---------------------------------------------------------
class DLL_GAME_ASSETS GUIControllerBase : public IScript
{
public:
  explicit GUIControllerBase(eMainContextBase*);

  virtual void Initialize() override;
  virtual void	Update(float _tick) override {}

protected:
  eMainContextBase* m_game = nullptr;
  std::vector<std::shared_ptr<eObject>> m_objects;
};

//---------------------------------------------------------
class DLL_GAME_ASSETS GUIControllerMenuWithButtons : public GUIControllerBase
{
public:
  GUIControllerMenuWithButtons(eMainContextBase*, eOpenGlRenderPipeline&, RemSnd*);
  virtual ~GUIControllerMenuWithButtons();

  virtual void Initialize() override;
  virtual void	Update(float _tick) override;
  virtual bool  OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier) override;

protected:
  std::reference_wrapper<eOpenGlRenderPipeline> m_pipeline;
  float m_cursor_x = 0.0f;
  float m_cursor_y = 0.0f;
  bool m_is_menu_active = true;
  bool m_cursor_is_outside_buttons = true;
  std::vector<dbb::Rect> m_buttons;
  RemSnd* m_page_sound = nullptr;
};

//---------------------------------------------------------
class DLL_GAME_ASSETS GUIControllerMenuForStairsScript : public GUIControllerBase
{
public:
  GUIControllerMenuForStairsScript(eMainContextBase*);
  virtual ~GUIControllerMenuForStairsScript();

  virtual void  Initialize() override;
  virtual void	Update(float _tick) override;
  virtual bool  OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier) override;

  bool IsInitialized() const;
  std::function<float()> GetSliderOneCallback();
  std::function<float()> GetSliderTwoCallback();
  bool GetStartCalled();
  bool GetResetCalled();
  bool GetFlipCalled();
  std::shared_ptr<Text> GetPlaneText() const;

protected:
  std::vector<Texture> m_menu_textures;
  std::function<float()> m_get_slider_one_value;
  std::function<float()> m_get_slider_two_value;
  std::shared_ptr<Text> m_plane_t;
  bool m_is_initialized = false;
  bool m_start_called = false;
  bool m_reset_called = false;
  bool m_flip_called = false;
};
