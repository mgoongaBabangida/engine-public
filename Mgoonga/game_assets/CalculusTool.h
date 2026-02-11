#pragma once

#include <base/interfaces.h>
#include <base/Object.h>
#include <math/Bezier.h>
#include <glm/glm/gtc/noise.hpp>
#include <opengl_assets/Texture.h>
#include <opengl_assets/TerrainModel.h>

#include <vector>
//#include <set>
//#include <future>

class IWindowImGui;
class eMainContextBase;
class eModelManager;
class eTextureManager;
class eOpenGlRenderPipeline;
class BezierCurveUIController;

//---------------------------------------------------------------------
class CalculusTool : public IScript
{
public:
  struct Function 
  {
    std::function<float(float)> m_function;
    /*std::function<void(void)>   m_update_function;*/
    float                       m_current_x = 0.;
    float                       m_current_y = 0.;
    size_t                      m_function_draw_decim = 20;
    float                       m_function_a = 2.f;
    float                       m_function_b = 0.f;
    std::string                 m_function_label = "y = x^a";
    dbb::Bezier                 m_bezier_representation;
  };

  CalculusTool(eMainContextBase* _game,
               eModelManager* _modelManager,
               eTextureManager* _texManager,
               eOpenGlRenderPipeline& _pipeline,
               IWindowImGui* _imgui);
  virtual ~CalculusTool();

  virtual void Update(float _tick) override;
  virtual void Initialize() override;

protected:
  void _InitFunctions();
  void _UpdateCurrentFunctionVisual(const Function&);

  eMainContextBase*                             m_game = nullptr;
  eModelManager*                                m_modelManager = nullptr;
  eTextureManager*                              m_texture_manager = nullptr;
  std::reference_wrapper<eOpenGlRenderPipeline> m_pipeline;
  IWindowImGui*                                 m_imgui = nullptr;

  dbb::Bezier m_bezier;
  shObject m_bezier_object;
  BezierCurveUIController* m_bezier_controller = nullptr;

  LineMesh*   m_function_mesh = nullptr;
  shObject    m_function_object;
  shObject    m_x_y_visual_representation_object;

  std::vector<Function> m_functions;
  size_t m_active_function = 0;

  float*  m_current_x = nullptr;
  float*  m_current_y = nullptr;
  float*  m_function_a = nullptr;
  float*  m_function_b = nullptr;
};
