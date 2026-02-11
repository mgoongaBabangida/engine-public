#pragma once

#include "stdafx.h"

#include <base/Event.h>

#include "opengl_assets.h"
#include "MyModel.h"
#include "TerrainModel.h"

#include <map>

enum class Primitive
{
  SPHERE = 0
};

//----------------------------------------------------------------------
class DLL_OPENGL_ASSETS eModelManager
{
  public:
  	eModelManager();
    ~eModelManager();

    base::Event<std::function<void(const std::map<std::string, std::shared_ptr<IModel> >&)>> ModelsUpdated;

  	void							            InitializePrimitives();
  	std::shared_ptr<IModel>			  Find(const std::string& name) const;

  	virtual IModel*               Add(const std::string& name, char* path, bool invert_y_uv = false);
    virtual void                  Add(const std::string& _name, Primitive _type, Material&& _material);
    virtual void							    Add(const std::string&, std::shared_ptr<MyModel>);

  	std::unique_ptr<MyModel>		  CreateFromBrush(const std::string& name);
    std::shared_ptr<Brush>			  FindBrush(const std::string&) const;

  	std::unique_ptr<TerrainModel>	CloneTerrain(const std::string& name);
  
    void                          ReloadTextures();

    bool                                  ConvertMeshToBrush(const std::string& name);
    std::vector<std::shared_ptr<IModel>>  Split(const std::string&);

  protected:
  	std::map<std::string, std::shared_ptr<Brush> >		m_brushes;
  	std::map<std::string, std::shared_ptr<IModel> >   m_models;
  	std::unique_ptr<TerrainModel>						          m_terrain;
    std::atomic<bool>                                 m_container_flag;
};
