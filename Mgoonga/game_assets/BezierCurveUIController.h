#pragma once

#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Event.h>

#include <math/Bezier.h>
#include <array>

class eObject;
class BezierCurveMesh;
class eMainContextBase;
struct Texture;
class GUI;

//---------------------------------------------------------
class DLL_GAME_ASSETS BezierCurveUIController : public IScript
{
public:
  explicit BezierCurveUIController(eMainContextBase* _game,
                                   std::shared_ptr<eObject> _bezier_object,
                                   float _control_point_size,
                                   const Texture* _window = nullptr,
                                   bool _XZ_plane = true);
  virtual ~BezierCurveUIController();

  base::Event<std::function<void(const dbb::Bezier&)>> CurveChanged;
  base::Event<std::function<void(const dbb::Bezier&)>> ToolFinished;

  virtual void	Update(float _tick) override;
  virtual bool	OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifiers) override;
  virtual bool  OnMouseRelease(KeyModifiers _modifier) override;

  float&  GetCurrentPositionOnCurve() { return m_current_pos_on_curve; }
  bool&   GetTDistance() { return m_t_distance; }
  float&  GetDistanceToBezier() { return m_distance_to_bezier; }

  void    FlipInputStrategy();
  bool    IsXZPlane() const { return m_XZ_plane; }

protected:
  eMainContextBase* m_game = nullptr;
  std::shared_ptr<GUI> m_window;

  std::vector<dbb::Bezier*> m_beziers;
  std::vector<const BezierCurveMesh*> m_bezier_meshs;

  float m_cursor_x = 0;
  float m_cursor_y = 0;
  float m_closed = false;
  float m_current_pos_on_curve = 0.2f;
  bool  m_t_distance = false;
  float m_distance_to_bezier = 0.0f;
  bool  m_XZ_plane = false;
  bool m_curve_changed = false;

  size_t m_t_index = 0;
  size_t m_distance_index = 0;
};