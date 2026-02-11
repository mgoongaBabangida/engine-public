#include "stdafx.h"
#include "StairsScript.h"

#include <game_assets/MainContextBase.h>
#include <game_assets/ObjectFactory.h>
#include <game_assets/BezierCurveUIController.h>
#include <game_assets/ModelManagerYAML.h>
#include <game_assets/GUIController.h>

#include <opengl_assets/MyMesh.h>
#include <opengl_assets/MyModel.h>

//------------------------------------------------
StairsScript::StairsScript(eMainContextBase* _game)
  : InteractionScript(_game)
{
}

//-------------------------------------------------------------
void StairsScript::Initialize()
{
}

//-------------------------------------------------------------
void StairsScript::Update(float _tick)
{
  if (m_interaction_data.m_bezier_object.get() == nullptr)
  {
    _CreateBezier();
  }

  if (m_debug_window && m_debug_window->IsInitialized())
  {
  if (m_debug_window->GetFlipCalled())
    m_interaction_data.m_bezier_controller->FlipInputStrategy();

  auto text = m_debug_window->GetPlaneText();
  if (m_interaction_data.m_bezier_controller->IsXZPlane())
    text->content = "XZ";
  else
    text->content = "XY";

  m_interaction_data.m_speed = m_debug_window->GetSliderOneCallback()(); // 0 < value < 1
  m_interaction_data.m_animation_speed = m_debug_window->GetSliderTwoCallback()();

   if (m_debug_window->GetStartCalled())
     Start.Occur();

   if (m_debug_window->GetResetCalled())
     Reset.Occur();
  }
}

//---------------------------------------------------------
void StairsScript::_CreateBezier()
{
  dbb::Bezier b1;
  b1.p0 = { 10.16f,  -2.f,  -0.52f };
  b1.p1 = { 9.92f,  -0.18f,  -2.89f };
  b1.p2 = { 7.84f,  -0.05f, -2.58f };
  b1.p3 = { 7.90f,  1.17f, -0.82f };

  dbb::Bezier b2;
  b2.p0 = { 7.90f,  1.17f, -0.82f };
  b2.p1 = { 7.83f,  1.89f, -0.04f };
  b2.p2 = { 9.27f,  2.31f, 0.93f };
  b2.p3 = { 10.11f, 3.6f, -0.896f };

  ConnectBackwardsWithC1(b1, b2);

  ObjectFactoryBase factory;
  //Bezier
  BezierCurveMesh* bezier_mesh = new BezierCurveMesh(b1, /*2d*/false);
  BezierCurveMesh* bezier_mesh2 = new BezierCurveMesh(b2, /*2d*/false);
  m_interaction_data.m_bezier_object = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{ bezier_mesh, bezier_mesh2 }), eObject::RenderType::BEZIER_CURVE);
  m_interaction_data.m_bezier_object->SetName("UpStairsBezier");

  for (int i = 0; i < 8; ++i)
  {
    shObject pbr_sphere = factory.CreateObject(m_game->GetModelManager()->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
    m_interaction_data.m_bezier_object->GetChildrenObjects().push_back(pbr_sphere);
  }

  m_interaction_data.m_bezier_controller = new BezierCurveUIController(m_game, m_interaction_data.m_bezier_object, 0.05f, nullptr, false);
  m_interaction_data.m_bezier_controller->ToolFinished.Subscribe([this](const dbb::Bezier& _bezier) { Update(0); });
  m_interaction_data.m_bezier_object->SetScript(m_interaction_data.m_bezier_controller);
  m_game->AddObject(m_interaction_data.m_bezier_object);

  m_interaction_data.m_beziers[0] = &bezier_mesh->GetBezier();
  m_interaction_data.m_beziers[1] = &bezier_mesh2->GetBezier();

  m_interaction_data.m_speed = 0.33f;
  m_interaction_data.m_animation_speed = 0.68f;
  m_interaction_data.m_name = "upstairs";

  m_volume.origin = b1.p0;
  m_volume.size = {1.f, 1.f, 1.f};
  m_volume.orientation = { {1,0,0}, {0,1,0}, {0,0,1} };
}
