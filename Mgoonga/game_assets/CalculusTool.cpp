#include "stdafx.h"

#include "CalculusTool.h"

#include "ObjectFactory.h"
#include "MainContextBase.h"

#include <sdl_assets/ImGuiContext.h>
#include <opengl_assets/ModelManager.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/openglrenderpipeline.h>

//#include <math/Random.h>
//#include <math/BoxCollider.h>

#include "BezierCurveUIController.h"

//--------------------------------------------------------------
CalculusTool::CalculusTool(eMainContextBase* _game,
                           eModelManager* _modelManager,
                           eTextureManager* _texManager, 
                           eOpenGlRenderPipeline& _pipeline, 
                           IWindowImGui* _imgui)
  : m_game(_game)
  , m_modelManager(_modelManager)
  , m_texture_manager(_texManager)
  , m_pipeline(_pipeline)
  , m_imgui(_imgui)
{
}

//--------------------------------------------------------------
CalculusTool::~CalculusTool()
{

}

//--------------------------------------------------------------
void CalculusTool::Update(float _tick)
{
	*m_current_y = m_functions[m_active_function].m_function(m_functions[m_active_function].m_current_x);
	m_x_y_visual_representation_object->GetTransform()->setTranslation({ *m_current_x, *m_current_y, 0 });

	static float last_a = m_functions[m_active_function].m_function_a;
	if (last_a != m_functions[m_active_function].m_function_a)
		_UpdateCurrentFunctionVisual(m_functions[m_active_function]);
	last_a = m_functions[m_active_function].m_function_a;
}

//--------------------------------------------------------------
void CalculusTool::Initialize()
{
	m_bezier.p0 = { 0.0f,  0.0f,  0.0f };
	m_bezier.p1 = { 0.25f,  0.43f, 0.0f };
	m_bezier.p2 = { 0.37f,  0.61f, 0.0f };
	m_bezier.p3 = { 1.f,  1.f, 0.0f };

	ObjectFactoryBase factory;
	//Bezier
	BezierCurveMesh* bezier_mesh = new BezierCurveMesh(m_bezier, /*2d*/false);
	m_bezier_object = factory.CreateObject(std::make_shared<BezierCurveModel>(std::vector<BezierCurveMesh*>{bezier_mesh}), eObject::RenderType::BEZIER_CURVE);

	for (int i = 0; i < 4; ++i)
	{
		shObject pbr_sphere = factory.CreateObject(m_modelManager->Find("sphere_red"), eObject::RenderType::PBR, "SphereBezierPBR " + std::to_string(i));
		m_bezier_object->GetChildrenObjects().push_back(pbr_sphere);
	}
	// 5th point for t
	shObject pbr_sphere = factory.CreateObject(m_modelManager->Find("sphere_blue"), eObject::RenderType::PBR, "SphereBezierPBR Result");
	m_bezier_object->GetChildrenObjects().push_back(pbr_sphere);

	// 6th point for distance to Bezier
	shObject pbr_sphere_distance = factory.CreateObject(m_modelManager->Find("sphere_green"), eObject::RenderType::PBR, "SphereBezierPBR Distance");
	pbr_sphere_distance->GetTransform()->getTranslationRef().x = 0.5f;
	pbr_sphere_distance->GetTransform()->getTranslationRef().y = 1.0f;
	m_bezier_object->GetChildrenObjects().push_back(pbr_sphere_distance);

	m_bezier_controller = new BezierCurveUIController(m_game, m_bezier_object, 0.02f, nullptr, false);
	m_bezier_controller->ToolFinished.Subscribe([this](const dbb::Bezier& _bezier) { m_bezier = _bezier; Update(0); });
	m_bezier_object->SetScript(m_bezier_controller);

	std::function<void()> create_bezier_callback = [this]()
	{
		m_game->AddObject(m_bezier_object);
	};

	//GUI bezier
	m_imgui->Add(BUTTON, "Bezier Curve", (void*)&create_bezier_callback);
	m_imgui->Add(CHECKBOX, "Use T as Distance", &m_bezier_controller->GetTDistance());
	m_imgui->Add(SLIDER_FLOAT_NERROW, "T on Curve", &m_bezier_controller->GetCurrentPositionOnCurve());
	m_imgui->Add(TEXT_VEC, "p0 = " , (void*)&bezier_mesh->GetBezier().p0);
	m_imgui->Add(TEXT_VEC, "p1 = ", (void*)&bezier_mesh->GetBezier().p1);
	m_imgui->Add(TEXT_VEC, "p2 = ", (void*)&bezier_mesh->GetBezier().p2);
	m_imgui->Add(TEXT_VEC, "p3 = ", (void*)&bezier_mesh->GetBezier().p3);
	m_imgui->Add(TEXT_VEC, "t = ", (void*)&pbr_sphere->GetTransform()->getTranslationRef());
	m_imgui->Add(TEXT_FLOAT, "Distance to Bezier = ", (void*)&m_bezier_controller->GetDistanceToBezier());

	// Function
	m_function_mesh = new LineMesh({}, {}, glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f });
	m_function_object = factory.CreateObject(std::make_shared<SimpleModel>(m_function_mesh), eObject::RenderType::LINES, "Normal mesh");
	_InitFunctions();

	m_x_y_visual_representation_object = factory.CreateObject(m_modelManager->Find("sphere_green"), eObject::RenderType::PBR, "SphereBezierPBR Distance");
	m_x_y_visual_representation_object->GetTransform()->setScale({ 0.05f, 0.05f, 0.05f });

	//GUI function
	m_current_x = &m_functions[m_active_function].m_current_x;
	m_current_y = &m_functions[m_active_function].m_current_y;
	m_function_a = &m_functions[m_active_function].m_function_a;
	m_function_b = &m_functions[m_active_function].m_function_b;

	static std::function<size_t(size_t)> functions_callback = [this](size_t _index)
	{
		size_t prev = m_active_function;
		if (_index != MAXSIZE_T)
		{
			m_active_function = _index;

			m_current_x = &m_functions[m_active_function].m_current_x;
			m_current_y = &m_functions[m_active_function].m_current_y;
			m_function_a = &m_functions[m_active_function].m_function_a;
			m_function_b = &m_functions[m_active_function].m_function_b;
			_UpdateCurrentFunctionVisual(m_functions[m_active_function]);
		}
		return prev;
	};

	static eVectorStringsCallback watcher{ {}, functions_callback };
	for (const auto function : m_functions)
		watcher.data.push_back(function.m_function_label);

	m_imgui->Add(COMBO_BOX, "Functions", &watcher);

	std::function<void()> create_function_callback = [this]()
	{
		_UpdateCurrentFunctionVisual(m_functions[m_active_function]);
		m_game->AddObject(m_function_object);
		m_game->AddObject(m_x_y_visual_representation_object);
	};

	m_imgui->Add(BUTTON, "Function", (void*)&create_function_callback);
	m_imgui->Add(SLIDER_FLOAT_NERROW_P, "X = ", &m_current_x);
	m_imgui->Add(SLIDER_FLOAT_P, "A = ", &m_function_a);
	m_imgui->Add(SLIDER_FLOAT_P, "B = ", &m_function_b);
	m_imgui->Add(TEXT_FLOAT_P, "Y = ", &m_current_y);
}

//-------------------------------------------------------------------------------------
void CalculusTool::_InitFunctions()
{
	m_functions.reserve(16);

	m_functions.push_back(Function{});
	Function& parabolic_ease_in = m_functions.back();
	parabolic_ease_in.m_function_label = "y = x^a";
	parabolic_ease_in.m_function = [this, &parabolic_ease_in](float _x)->float { return glm::pow(_x, parabolic_ease_in.m_function_a); };

	m_functions.push_back(Function{});
	Function& parabolic_ease_out = m_functions.back();
	parabolic_ease_out.m_function_label = "y =1-(x^a)";
	parabolic_ease_out.m_function = [this, &parabolic_ease_out](float _x)->float { return 1.f - glm::pow(_x, parabolic_ease_out.m_function_a); };

	m_functions.push_back(Function{});
	Function& ease_in_out_parabolic = m_functions.back();
	ease_in_out_parabolic.m_function_label = "y=x<0.5 ? 4*x*x*x : 1 - glm::pow(-2*x + 2, 3)/2";
	ease_in_out_parabolic.m_function = [this, &ease_in_out_parabolic](float _x)->float { return _x < 0.5f ? 4.f *_x * _x * _x : 1 - glm::pow(-2 * _x + 2, 3) / 2; };
	ease_in_out_parabolic.m_bezier_representation = dbb::MakeCubic2DBezier(0.65f, 0.f, 0.35f, 1.f);

	m_functions.push_back(Function{});
	Function& ease_in_exponent = m_functions.back();
	ease_in_exponent.m_function_label = "y = glm::pow(a,10*x-10)";
	ease_in_exponent.m_function_a = 2.;
	ease_in_exponent.m_function = [this, &ease_in_exponent](float _x)->float { return _x == 0 ? 0 : glm::pow(ease_in_exponent.m_function_a, 10 * _x - 10); };
	ease_in_exponent.m_bezier_representation = dbb::MakeCubic2DBezier(0.7, 0, 0.84, 0);

	m_functions.push_back(Function{});
	Function& ease_out_exponent = m_functions.back();
	ease_out_exponent.m_function_label = "y = 1-glm::pow(a,-10*x)";
	ease_out_exponent.m_function_a = 2.;
	ease_out_exponent.m_function = [this, &ease_out_exponent](float _x)->float { return _x == 1 ? 1 : 1 - glm::pow(ease_out_exponent.m_function_a, -10 * _x); };
	ease_out_exponent.m_bezier_representation = dbb::MakeCubic2DBezier(0.16, 1, 0.3, 1);

	m_functions.push_back(Function{});
	Function& ease_in_out_exponent = m_functions.back();
	ease_in_out_exponent.m_function_label = "x<0.5 ? glm::pow(a, 20*x-10)/2 : (2-pow(a,-20*x+10))/2";
	ease_in_out_exponent.m_function_a = 2.;
	ease_in_out_exponent.m_function = [this, &ease_in_out_exponent](float _x)->float { return _x == 0
																																													  ? 0
																																													  : _x == 1
																																													  ? 1
																																													  : _x < 0.5 ? glm::pow(ease_in_out_exponent.m_function_a, 20 * _x - 10) / 2
																																													  : (2 - glm::pow(ease_in_out_exponent.m_function_a, -20 * _x + 10)) / 2; };
	ease_in_out_exponent.m_bezier_representation = dbb::MakeCubic2DBezier(0.87, 0, 0.13, 1);

	m_functions.push_back(Function{});
	Function& ease_in_sine = m_functions.back();
	ease_in_sine.m_function_label = "y=1 - cos((_x * pi)/2)";
	ease_in_sine.m_function = [this, &ease_in_sine](float _x)->float { return 1 - glm::cos((_x * glm::pi<float>()) / 2); };
	ease_in_sine.m_bezier_representation = dbb::MakeCubic2DBezier(0.12f, 0.f, 0.39f, 0.f);

	m_functions.push_back(Function{});
	Function& ease_out_sine = m_functions.back();
	ease_out_sine.m_function_label = "y=sin((x* pi)/2)";
	ease_out_sine.m_function = [this, &ease_out_sine](float _x)->float { return glm::sin((_x * glm::pi<float>()) / 2); };
	ease_out_sine.m_bezier_representation = dbb::MakeCubic2DBezier(0.61, 1, 0.88, 1);

	m_functions.push_back(Function{});
	Function& ease_in_out_sine = m_functions.back();
	ease_in_out_sine.m_function_label = "y= -(cos(pi*x)-1)/2;";
	ease_in_out_sine.m_function = [this, &ease_in_out_sine](float _x)->float { return -(glm::cos(glm::pi<float>()*_x)-1.f)/2.f; };
	ease_in_out_sine.m_bezier_representation = dbb::MakeCubic2DBezier(0.37, 0, 0.63, 1);
}

//------------------------------------------------------------------------
void CalculusTool::_UpdateCurrentFunctionVisual(const Function& _function)
{
	std::vector<glm::vec3> points;
	std::vector<GLuint> indices;
	float cur_x = 0;
	for (int i = 0; i <= _function.m_function_draw_decim; ++i)
	{
		points.emplace_back(cur_x, _function.m_function(cur_x), 0);
		indices.push_back(i);
		if (i > 0 && i < _function.m_function_draw_decim)
			indices.push_back(i);
		cur_x += 1.0f / (float)_function.m_function_draw_decim;
	}
	m_function_mesh->UpdateData(points, indices, { 1,1,0,1 });
}
