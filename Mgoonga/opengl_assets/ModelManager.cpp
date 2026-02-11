#include "stdafx.h"

#include "ModelManager.h"

#include "ShapeGenerator.h"
#include "ShapeData.h"
#include "MyMesh.h"
#include "Model.h"
#include "AssimpLoader.h"
#include "ShpereTexturedModel.h"
#include "mesh_system_utils.h"

#include "TextureManager.h"

//-----------------------------------------------------------------
void eModelManager::InitializePrimitives()
{
	ShapeData cube = ShapeGenerator::makeCube();
	ShapeData arrow = ShapeGenerator::makeArrow();
	ShapeData quad = ShapeGenerator::makeQuad();
	ShapeData sphere = ShapeGenerator::makeSphere(40);
	ShapeData square = ShapeGenerator::makeSquare(5.0f, 5.0f); //(Width() / Height(), 1.0f )
	ShapeData ellipse = ShapeGenerator::makeEllipse(2.0f, 4.0f, 2.0f, 10);
	ShapeData capsule = ShapeGenerator::makeCapsule();

	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush>>("cube", new Brush("cube", cube)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("plane", gl_utils::CreatePlane(10,10)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("arrow", new Brush("arrow", arrow)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("quad", new Brush("quad", quad)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("sphere", new Brush("sphere", sphere)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("square", new Brush("square", square)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("ellipse", new Brush("ellipse", ellipse)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("capsule", new Brush("capsule", capsule)));
	m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush> >("hex", gl_utils::CreateHex(0.3406f)));

	cube.cleanup();
	arrow.cleanup();
	quad.cleanup();
	sphere.cleanup();
	square.cleanup();
	ellipse.cleanup();
	capsule.cleanup();
}

//-----------------------------------------------------------------
eModelManager::eModelManager()
	: m_terrain(new TerrainModel)
{}

//-----------------------------------------------------------------
eModelManager::~eModelManager()
{
}

//-----------------------------------------------------------------
std::shared_ptr<IModel> eModelManager::Find(const std::string& name) const
{
	if (m_models.find(name) != m_models.end()) //@todo needs sync with Add
		return  m_models.find(name)->second;
	else return {};
}

//-----------------------------------------------------------------
IModel* eModelManager::Add(const std::string& name, char* path, bool invert_y_uv)
{
	AssimpLoader loader;
	IModel* model = loader.LoadModel(path, name, invert_y_uv);
	if (model)
	{
		bool fls = false;
		while (!m_container_flag.compare_exchange_weak(fls, true)) { fls = false; }
		m_models.insert(std::pair<std::string, std::shared_ptr<IModel> >(name, model));
		ModelsUpdated.Occur(m_models);
		m_container_flag.store(false);
	}
	return model;
}

//-----------------------------------------------------------------
void eModelManager::Add(const std::string& _name, Primitive _type, Material&& _material)
{
	if (_type == Primitive::SPHERE)
	{
		bool fls = false;
		while (!m_container_flag.compare_exchange_weak(fls, true)) { fls = false; }
		SphereTexturedMesh* mesh = new SphereTexturedMesh();
		mesh->SetMaterial(std::move(_material)); //needs move ctr
		m_models.insert(std::pair<std::string, std::shared_ptr<IModel>>{ _name, new SphereTexturedModel(mesh) });
		ModelsUpdated.Occur(m_models);
		m_container_flag.store(false);
	}
}

//-----------------------------------------------------------------
void eModelManager::Add(const std::string& name, std::shared_ptr<MyModel> model)
{
	bool fls = false;
	while (!m_container_flag.compare_exchange_weak(fls, true)) { fls = false; }
	m_models.insert(std::pair<std::string, std::shared_ptr<IModel> >(name, model));
	ModelsUpdated.Occur(m_models);
	m_container_flag.store(false);
}

//-----------------------------------------------------------------
std::unique_ptr<MyModel> eModelManager::CreateFromBrush(const std::string& _name)
{
	if (auto it_brush = m_brushes.find(_name); it_brush != m_brushes.end())
		return std::unique_ptr<MyModel>(new MyModel(it_brush->second, _name));
	else
		return std::unique_ptr<MyModel>();
}

//-----------------------------------------------------------------
std::unique_ptr<TerrainModel> eModelManager::CloneTerrain(const std::string& _name)
{
	return std::make_unique<TerrainModel>(*(m_terrain));
}

//-----------------------------------------------------------------
std::shared_ptr<Brush> eModelManager::FindBrush(const std::string& _name) const
{
	return  m_brushes.find(_name)->second;
}

//-----------------------------------------------------------------
void eModelManager::ReloadTextures()
{
	for (auto& m : m_models)
	{
		m.second->SetUpMeshes();
		m.second->ReloadTextures();
	}
}

//-----------------------------------------------------------------
bool eModelManager::ConvertMeshToBrush(const std::string& _name)
{
	 auto it = m_models.find(_name);
	 std::shared_ptr<IModel> model = it->second;

	if (model.get() == nullptr)
		return false;

	if (eModel* e_model = dynamic_cast<eModel*>(model.get()); e_model)
	{
		eMesh* mesh = const_cast<eMesh*>(e_model->GetMeshByIndex(0));
		if (model->GetMeshes().size() > 1)
		{
			for (size_t i = 1; i < model->GetMeshes().size(); ++i)
			{
				eMesh* mesh_other = const_cast<eMesh*>(e_model->GetMeshByIndex(i));
				mesh->MergeMesh(std::move(*mesh_other));
			}
		}
		Brush* brush = gl_utils::MeshToBrush(std::move(*mesh), {});
		brush->calculatedTangent();
		m_models.erase(it); // erase old model though there might be stil shared_ptr of it somewhere

		auto it = m_brushes.insert(std::pair<std::string, std::shared_ptr<Brush>>(_name, brush));
		m_models.insert(std::pair<std::string, std::shared_ptr<IModel> >(_name, new MyModel(it.first->second, _name))); // insert new model with brush
		return true;
	}
	return false;
}

//-----------------------------------------------------------------
std::vector<std::shared_ptr<IModel>> eModelManager::Split(const std::string& _name)
{
	std::vector<std::shared_ptr<IModel>> ret;
	auto it = m_models.find(_name);
	std::shared_ptr<IModel> model = it->second;

	if (model.get() == nullptr)
		return ret;

	std::vector<std::unique_ptr<IModel>> models = it->second->Split();
	for (size_t i = 0; i< models.size(); ++i)
	{
		auto it = m_models.insert({model->GetName(), std::move(models[i])}); // insert new model and own it
		ret.push_back(it.first->second);
	}
	m_models.erase(it); // delete old model, it does not extist any more, animations and all shared data is lost

	return ret;
}
