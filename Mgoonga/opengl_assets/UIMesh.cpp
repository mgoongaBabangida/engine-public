#include "UIMesh.h"
#include "Texture.h"

//-----------------------------------------------------------------------------------------------------------------------
UIMesh::UIMesh()
{
  // Create all slots once; we can use only [0] if not triple
  for (int i = 0; i < kRing; ++i) createSlot_(i);
  ringCount_ = 1; // default Simple
}

//-----------------------------------------------------------------------------------------------------------------------
UIMesh::~UIMesh()
{
  // Ensure GPU finished with our buffers
  for (int i = 0; i < kRing; ++i)
  {
    waitFence_(i);
    destroySlot_(i);
  }
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::SetUploadPolicy(UploadPolicy p, bool useGpuFences)
{
  policy_ = p;
  useFences_ = useGpuFences;
  ringCount_ = (p == UploadPolicy::TripleBuffer) ? 3 : 1;
  // optional: reset cursor to avoid touching a busy slot on switch
  ringCur_ = 0;
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::createSlot_(int i)
{
  glGenVertexArrays(1, &vao_[i]);
  glGenBuffers(1, &vbo_[i]);
  glGenBuffers(1, &ibo_[i]);

  glBindVertexArray(vao_[i]);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_[i]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_[i]);

  const GLsizei stride = sizeof(UIGpuVertex);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIGpuVertex, x));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIGpuVertex, u));
  //glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(UIGpuVertex, rgba));
  glEnableVertexAttribArray(2); glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(UIGpuVertex), (void*)offsetof(UIGpuVertex, rgba)); // NOTE the 'I' function
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIGpuVertex, aux0));

  glBindVertexArray(0);
  fence_[i] = nullptr;
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::destroySlot_(int i)
{
  if (fence_[i]) { glDeleteSync(fence_[i]); fence_[i] = nullptr; }
  if (ibo_[i]) glDeleteBuffers(1, &ibo_[i]);
  if (vbo_[i]) glDeleteBuffers(1, &vbo_[i]);
  if (vao_[i]) glDeleteVertexArrays(1, &vao_[i]);
  vao_[i] = vbo_[i] = ibo_[i] = 0;
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::waitFence_(int slot)
{
  if (!useFences_) return;
  if (fence_[slot]) {
    // Non-blocking fast path, fall back to wait if needed
    GLenum res = glClientWaitSync(fence_[slot], 0, 0);
    if (res == GL_TIMEOUT_EXPIRED) {
      // Give the GPU a little time; you can tune the timeout
      glClientWaitSync(fence_[slot], GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000); // 1 ms
    }
    glDeleteSync(fence_[slot]);
    fence_[slot] = nullptr;
  }
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::setFence_(int slot)
{
  if (!useFences_) return;
  if (fence_[slot]) { glDeleteSync(fence_[slot]); fence_[slot] = nullptr; }
  fence_[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::upload_(int slot, const UIGpuVertex* v, size_t nv, const uint32_t* i, size_t ni)
{
  glBindVertexArray(vao_[slot]);

  // vertices
  glBindBuffer(GL_ARRAY_BUFFER, vbo_[slot]);
  const GLsizeiptr vSize = (GLsizeiptr)(nv * sizeof(UIGpuVertex));

  switch (policy_)
  {
  case UploadPolicy::Simple:
    glBufferData(GL_ARRAY_BUFFER, vSize, v, GL_STREAM_DRAW);
    break;

  case UploadPolicy::Orphan:
  case UploadPolicy::TripleBuffer: // we still use orphaning on ring buffers; it's cheap
    glBufferData(GL_ARRAY_BUFFER, vSize, nullptr, GL_STREAM_DRAW);   // orphan
    if (nv) glBufferSubData(GL_ARRAY_BUFFER, 0, vSize, v);
    break;
  }

  // indices
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_[slot]);
  const GLsizeiptr iSize = (GLsizeiptr)(ni * sizeof(uint32_t));

  switch (policy_)
  {
  case UploadPolicy::Simple:
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, i, GL_STREAM_DRAW);
    break;

  case UploadPolicy::Orphan:
  case UploadPolicy::TripleBuffer:
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, nullptr, GL_STREAM_DRAW); // orphan
    if (ni) glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, iSize, i);
    break;
  }

  glBindVertexArray(0);
}

//-----------------------------------------------------------------------------------------------------------------------
void UIMesh::Draw()
{
  if (!source) return;

  // Pick slot
  int slot = 0;
  if (policy_ == UploadPolicy::TripleBuffer)
  {
    // Next ring slot; make sure GPU finished with it
    slot = (ringCur_ + 1) % ringCount_;
    waitFence_(slot);
  }

  // Snapshot pointers from IUiSource
  size_t nv = 0, ni = 0, nc = 0;
  const auto* vtx = source->vertices(nv);
  const auto* idx = source->indices(ni);
  const auto* cmd = source->commands(nc);
  if (!vtx || !idx || !cmd || nc == 0) return;

  // Upload into chosen slot
  upload_(slot, vtx, nv, idx, ni);

  if (cbs.begin) cbs.begin();

  glBindVertexArray(vao_[slot]);

  uint32_t lastTex[3] = { 0,0,0 };
  bool haveTex = false;
  for (size_t j = 0, indexByte = 0; j < nc; ++j)
  {
    const UiCmdWire& c = cmd[j];

    if (cbs.before)
      cbs.before(c.pipe, c);

    if (!haveTex || c.tex0 != lastTex[0] && c.tex0 != 0) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, c.tex0); lastTex[0] = c.tex0; }
   /* if (!haveTex || c.tex1 != lastTex[1] && c.tex1 != 0) { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, c.tex1); lastTex[1] = c.tex1; }
    if (!haveTex || c.tex2 != lastTex[2] && c.tex2 != 0) { glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, c.tex2); lastTex[2] = c.tex2; }*/
    haveTex = true;

    auto sc = source->toScissorPx(c.clipVirtual);
    glScissor(sc.x, sc.y, sc.z, sc.w);

    // Prefer indexOffset if adopted; else keep contiguous offset
    const void* byteOffset = (void*)(uintptr_t)indexByte;
    glDrawElements(GL_TRIANGLES, (GLsizei)c.indexCount, GL_UNSIGNED_INT, byteOffset);
    indexByte += c.indexCount * sizeof(uint32_t);

    if (cbs.after)
      cbs.after(c);
  }

  glBindVertexArray(0);

  if (cbs.end)
    cbs.end();

  if (policy_ == UploadPolicy::TripleBuffer)
  {
    setFence_(slot);
    ringCur_ = slot;
  }
}