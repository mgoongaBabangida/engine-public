#include"stdafx.h"
#include"SATTestScript.h"

#include "MainContextBase.h"
#include "ObjectFactory.h"
#include "ModelManagerYAML.h"

//-------------------------------------------------------
SATTestScript::SATTestScript(eMainContextBase* _game)
  : m_game(_game)
{
}

//-------------------------------------------------------
SATTestScript::~SATTestScript()
{
}

//-------------------------------------------------------
void SATTestScript::Update(float _tick)
{
  std::vector<dbb::lineSegment> segments = m_obb.GetEdges();
  std::vector<dbb::point> linePoints;
  std::vector <unsigned int> lineIndexes;
  size_t index = 0;
  for (const dbb::lineSegment& segment : segments)
  {
    linePoints.push_back(segment.start);
    lineIndexes.push_back(index++);
    linePoints.push_back(segment.end);
    lineIndexes.push_back(index++);
  }
  m_obb_mesh->UpdateData(linePoints, lineIndexes, { 0.0f, 0.0f , 1.0f, 1.0f });

  m_triangle_mesh->UpdateData({m_triangle[0],m_triangle[1],m_triangle[2] }, { 0,1,1,2,2,0 }, { 1.0f, 1.0f , 0.0f, 1.0f });

  std::vector<dbb::point> axislinePoints;
  std::vector <unsigned int> axislineIndexes;
  axislinePoints.push_back({0,0,0});
  index = 0;
  for (const glm::vec3& axis : m_info.axis)
  {
    axislineIndexes.push_back(0);
    axislinePoints.push_back(axis);
    if(index<12)
      axislineIndexes.push_back(++index);
  }
  m_asix_mesh->UpdateData(axislinePoints, axislineIndexes, { 1.0f, 0.0f , 0.0f, 1.0f });
}

//-------------------------------------------------------
void SATTestScript::Initialize()
{
  m_obb.origin = { 0,0,0 };
  m_obb.orientation = { 1,0,0,  0,1,0,   0,0,1 };
  m_obb.size = { 1, 1 ,1 };

  m_triangle = { {1 , 0.5, 1},{1 , 0.5, -1},{-1, 0.5, 1} };

  ObjectFactoryBase factory;
  m_triangle_mesh = new LineMesh({}, {}, glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f });
  shObject tri = factory.CreateObject(std::make_shared<SimpleModel>(m_triangle_mesh), eObject::RenderType::LINES, "Triangle");
  m_game->AddObject(tri);

  m_obb_mesh = new LineMesh({}, {}, glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });
  shObject o = factory.CreateObject(std::make_shared<SimpleModel>(m_obb_mesh), eObject::RenderType::LINES, "OBB");
  m_game->AddObject(o);

  m_asix_mesh = new LineMesh({}, {}, glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f });
  shObject a = factory.CreateObject(std::make_shared<SimpleModel>(m_asix_mesh), eObject::RenderType::LINES, "ASIX");
  m_game->AddObject(a);
}

//-----------------------------------------------------------------------------------
bool SATTestScript::OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier)
{
  switch (_asci)
  {
   case ASCII_J:
   {
     CollisionManifold collision = TriangleOBB(m_triangle, m_obb);
     m_info = collision.info;
     break;
   }
   case ASCII_K:
   {
     m_obb.origin.z += 0.1f;
     break;
   }
   case ASCII_V:
   {
     m_obb.origin.z -= 0.1f;
     break;
   }
   case ASCII_X:
   {
     m_obb.origin.x -= 0.1f;
     break;
   }
  }
  return false;
}
