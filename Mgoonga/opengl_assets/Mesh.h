#pragma once

#include "opengl_assets.h"
#include <base/interfaces.h>

#include "MeshBase.h"
#include "Texture.h"

DLL_OPENGL_ASSETS I3DMesh* MakeMesh(std::vector<Vertex> vertices,
                                    std::vector<GLuint> indices,
                                    std::vector<TextureInfo> textures,
                                    const Material& material,
                                    const std::string& name = "Default",
                                    bool _calculate_tangent = false);

//-----------------------------------------------------------------
class /*DLL_OPENGL_ASSETS*/ eMesh : public MeshBase
{
public:
  friend class eModel;

	eMesh(std::vector<Vertex> vertices,
        std::vector<GLuint> indices,
        std::vector<Texture> textures,
        const Material& material,
        const std::string& name = "Default",
        bool _calculate_tangent = false,
        bool _reload_textures = false);
  virtual ~eMesh();

  eMesh(const eMesh&) = delete;
  eMesh& operator=(const eMesh&) = delete;

  eMesh(eMesh&&) noexcept = default;
  eMesh& operator=(eMesh&&) noexcept = default;

  void Draw() override;
  void DrawInstanced(int32_t instances) override;

  virtual void  ReloadVertexBuffer() override;
  virtual void	calculatedTangent() override;

  virtual const std::string& Name() const override { return m_name; }

  virtual size_t                            GetVertexCount() const override { return m_vertices.size(); }
  virtual const std::vector<Vertex>&        GetVertexs() const override { return m_vertices; }
  virtual const std::vector<unsigned int>&  GetIndices() const override { return m_indices; }
  virtual std::vector<TextureInfo>					GetTextures() const override;
  virtual void                              AddTexture(Texture*) override;

  virtual void                              BindVAO() const override { glBindVertexArray(this->VAO); }
  virtual void                              UnbindVAO() const override { glBindVertexArray(0); }

  virtual bool											HasMaterial() const override { return true; }
  virtual void											SetMaterial(const Material&) override;
  virtual std::optional<Material>		GetMaterial() const override;

  void MergeMesh(eMesh&&);

  void SetupMesh();
  void ReloadTextures();
  void FreeTextures();

  void              SetBindCorrection (const glm::mat4& c) const { m_bindCorrection = c; }
  const glm::mat4&  GetBindCorrection() const { return m_bindCorrection; }

protected:
  void _BindRawTextures();
  void _BindMaterialTextures();

  /*  Mesh Data  */
  std::vector<Texture>		m_textures;
  std::string             m_name;
  Material                m_material;
  mutable glm::mat4       m_bindCorrection = glm::mat4(1.0f);
  float                   m_handedness = 1.0f;
};

//----------------------------------------------------------------------
void SaveBinary(const MeshBase& mesh, const std::string& path);

//--------------------------------------------------------------------------------
void LoadBinary(MeshBase& mesh, const std::string& path);
