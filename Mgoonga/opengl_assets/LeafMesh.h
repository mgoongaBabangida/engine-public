#pragma once
#include "opengl_assets.h"

#include <base/interfaces.h>

#include "MeshBase.h"
#include "Texture.h"

//----------------------------------------------------------------
class LeafMesh : public I3DMesh
{
public:
	LeafMesh() {}
  LeafMesh(std::vector<glm::mat4>&& _transforms, const Material&);
  LeafMesh(const LeafMesh&) = delete;
	LeafMesh& operator=(const LeafMesh&) = delete;

  virtual ~LeafMesh();

	virtual void								Draw() override;
	virtual const std::string&	Name() const override { return name; }

	virtual size_t														GetVertexCount() const override { return m_vertices.size(); }
	virtual const std::vector<Vertex>&				GetVertexs() const override { return m_vertices; }
	virtual const std::vector<unsigned int>&	GetIndices() const override { return m_indices; }

	virtual void      BindVAO() const override { glBindVertexArray(this->VAO); }
	virtual void      UnbindVAO() const override { glBindVertexArray(0); }

	virtual std::vector<TextureInfo>	GetTextures() const;
	virtual void											AddTexture(Texture* _t) override { m_textures.push_back(*_t); }

	virtual void											calculatedTangent()  override;
	virtual void											ReloadVertexBuffer() override {}//@todo ?

public:
	/*  Mesh Data  */
	std::vector<Vertex>		m_vertices;
	std::vector<GLuint>		m_indices;
	std::vector<Texture>	m_textures;

protected:
	/*  Render data  */
	GLuint VAO = -1, VBO = -1, EBO = -1;
	GLuint VBOinstanced = -1;
	std::string name = "LeafMesh";

	std::vector<glm::mat4>	m_transforms;
	Material								m_material;
	/*  Functions    */
	virtual void		setupMesh();
};