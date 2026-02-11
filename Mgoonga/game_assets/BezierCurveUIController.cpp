#include "stdafx.h"

#include "BezierCurveUIController.h"
#include "MainContextBase.h"
#include "InputStrategy.h"

#include <opengl_assets/MyMesh.h>
#include <opengl_assets/GUI.h>
#include <math/Rect.h>
#include <math/Utils.h>

//------------------------------------------------------
BezierCurveUIController::BezierCurveUIController(eMainContextBase* _game, shObject _bezier_object, float _control_point_size, const Texture* _window_texture, bool _XZ_plane)
: m_game(_game)
, m_XZ_plane(_XZ_plane)
{
  auto bezier_objects = _bezier_object->GetChildrenObjects();

  for(auto child : bezier_objects)
    child->GetTransform()->setScale({ _control_point_size, _control_point_size, _control_point_size });

  for(auto mesh : _bezier_object->GetModel()->GetMeshes())
    m_bezier_meshs.push_back(dynamic_cast<const BezierCurveMesh*>(mesh));

  for (auto mesh : m_bezier_meshs)
    m_beziers.push_back(&const_cast<BezierCurveMesh*>(mesh)->GetBezier());

  if (!m_beziers.empty())
  {
    for (size_t i = 0; i < m_beziers.size(); ++i)
    {
      bezier_objects[i*4]->GetTransform()->setTranslation(m_beziers[i]->p0);
      bezier_objects[i*4+1]->GetTransform()->setTranslation(m_beziers[i]->p1);
      bezier_objects[i*4+2]->GetTransform()->setTranslation(m_beziers[i]->p2);
      bezier_objects[i*4+3]->GetTransform()->setTranslation(m_beziers[i]->p3);
      if (i > 0)
        bezier_objects[i * 4]->SetVisible(false);
    }

    if (bezier_objects.size() % 4 == 1)
    {
      m_t_index = bezier_objects.size() - 1;
    }
    else if (bezier_objects.size() % 4 == 2)
    {
      m_t_index = bezier_objects.size() - 2;
      m_distance_index = bezier_objects.size() - 1;
    }

    if(m_t_index)
      bezier_objects[m_t_index]->GetTransform()->setTranslation(dbb::GetPoint(*m_beziers[0], m_current_pos_on_curve));
  }

  //for 2D curves
  if (_window_texture)
  {
    dbb::Rect window_rect;
    window_rect.m_top_left = { 50, 50 };
    window_rect.m_size = { 900, 500 };
    m_window = std::make_shared<GUI>(window_rect, m_game->Width(), m_game->Height());
    m_window->SetTexture(*_window_texture, { 0,0 }, { _window_texture->m_width, _window_texture->m_height });
    m_window->SetTransparent(true);
    m_window->SetTakeMouseEvents(true);
    m_window->SetVisible(true);

    glm::vec2 x_restriction = { -0.88f, 0.95f }; // NDC space (y-inverted) of the screen where we can move objects with mouse
    glm::vec2 y_restriction = { -0.8f, 0.73f };

    m_window->SetCommand(std::make_shared<GUICommand>([this, x_restriction, y_restriction]()
      {
        dbb::Rect close_button_rect;
        close_button_rect.m_top_left = { 850 + 50, 550 }; // inverted y check
        close_button_rect.m_size = { 50, 50 };
        if (close_button_rect.IsInside({ m_cursor_x , m_cursor_y }))
        {
          //clean up
          m_game->DeleteInputObserver(m_window.get());
          m_game->DeleteGUI(m_window);

          // map window to NDC
          m_beziers[0]->p0.x = dbb::MapValueLinear(m_beziers[0]->p0.x, x_restriction.x, x_restriction.y, -1.f, 1.f);
          m_beziers[0]->p0.y = dbb::MapValueLinear(m_beziers[0]->p0.y, y_restriction.x, y_restriction.y, -1.f, 1.f);
          m_beziers[0]->p1.x = dbb::MapValueLinear(m_beziers[0]->p1.x, x_restriction.x, x_restriction.y, -1.f, 1.f);
          m_beziers[0]->p1.y = dbb::MapValueLinear(m_beziers[0]->p1.y, y_restriction.x, y_restriction.y, -1.f, 1.f);
          m_beziers[0]->p2.x = dbb::MapValueLinear(m_beziers[0]->p2.x, x_restriction.x, x_restriction.y, -1.f, 1.f);
          m_beziers[0]->p2.y = dbb::MapValueLinear(m_beziers[0]->p2.y, y_restriction.x, y_restriction.y, -1.f, 1.f);
          m_beziers[0]->p3.x = dbb::MapValueLinear(m_beziers[0]->p3.x, x_restriction.x, x_restriction.y, -1.f, 1.f);
          m_beziers[0]->p3.y = dbb::MapValueLinear(m_beziers[0]->p3.y, y_restriction.x, y_restriction.y, -1.f, 1.f);

          ToolFinished.Occur(*m_beziers[0]);
          m_game->SetInputStrategy(nullptr);
          m_game->DeleteInputObserver(this);
          m_closed = true;
        }
      }));

    m_game->AddGUI(m_window);
    m_game->AddInputObserver(m_window.get(), STRONG);
    m_game->AddInputObserver(this, WEAK);
    m_game->SetInputStrategy(new InputStrategy2DMove(m_game, x_restriction, y_restriction)); // to move the 2d objects on the screen
  }
  else
    m_game->SetInputStrategy(new InputStrategyMoveAlongPlane(m_game->GetMainCamera(), 
                                                             _bezier_object->GetChildrenObjects(),
                                                              m_XZ_plane ? InputStrategyMoveAlongPlane::XZ : InputStrategyMoveAlongPlane::XY)); //3d
}

//------------------------------------------------------
BezierCurveUIController::~BezierCurveUIController()
{
  m_game->SetInputStrategy(nullptr);
}

//------------------------------------------------------
void BezierCurveUIController::Update(float _tick)
{
  if (!m_beziers.empty())
  {
    if (auto obj = m_object.lock(); obj)
    {
      if (!m_closed)
      {
        auto bezier_objects = obj->GetChildrenObjects();
        for (size_t i = 0; i < m_beziers.size(); ++i)
        {
          dbb::Bezier bezier = *m_beziers[i];
          m_beziers[i]->p0 = bezier_objects[i*4+0]->GetTransform()->getTranslation();
          m_beziers[i]->p1 = bezier_objects[i*4+1]->GetTransform()->getTranslation();
          m_beziers[i]->p2 = bezier_objects[i*4+2]->GetTransform()->getTranslation();
          m_beziers[i]->p3 = bezier_objects[i*4+3]->GetTransform()->getTranslation();

          if (i > 0)
            m_beziers[i]->p0 = m_beziers[i - 1]->p3;

          if (!dbb::Equal(bezier, *m_beziers[i]))
            m_curve_changed = true;
        }

        if (m_t_index != 0)
        {
          if (m_t_distance)
          {
            float arcLength = dbb::GetArcLength(*m_beziers[0]);
            auto luts = CreateCumulativeDistanceLUT(*m_beziers[0], 100);
            bezier_objects[m_t_index]->GetTransform()->setTranslation(dbb::GetPointByDistance(*m_beziers[0], m_current_pos_on_curve * arcLength, luts));
          }
          else
            bezier_objects[m_t_index]->GetTransform()->setTranslation(dbb::GetPoint(*m_beziers[0], m_current_pos_on_curve));
        }
        if (m_distance_index != 0)
        {
          m_distance_to_bezier = glm::length(bezier_objects[m_distance_index]->GetTransform()->getTranslation() -
                                             dbb::ClosestPointOnBezier(*m_beziers[0], bezier_objects[m_distance_index]->GetTransform()->getTranslation()));
        }

        for(auto m : m_bezier_meshs)
          const_cast<BezierCurveMesh*>(m)->Update();
      }
      else
      {
        m_game->DeleteObject(obj);
      }
    }
  }
}

//------------------------------------------------------
bool BezierCurveUIController::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifiers)
{
  m_cursor_x = (float)_x;
  m_cursor_y = (float)(m_game->Height() - _y);
  return false;
}

//----------------------------------------------------
bool BezierCurveUIController::OnMouseRelease(KeyModifiers _modifier)
{
  if (m_curve_changed)
    CurveChanged.Occur(*m_beziers[0]); // @todo only 0, need all
  m_curve_changed = false;
  return false;
}

//----------------------------------------------------
void BezierCurveUIController::FlipInputStrategy()
{
  if (auto obj = m_object.lock(); obj)
  {
    m_XZ_plane = !m_XZ_plane;
    m_game->SetInputStrategy(new InputStrategyMoveAlongPlane(m_game->GetMainCamera(),
                                                             obj->GetChildrenObjects(),
                                                             m_XZ_plane ? InputStrategyMoveAlongPlane::XZ : InputStrategyMoveAlongPlane::XY));
  }
}
