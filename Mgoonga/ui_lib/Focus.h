#pragma once
#include "Widget.h"
#include <algorithm>

#include <glm/glm/glm.hpp>

namespace UI_lib
{
  // Build linear tab order (DFS; respects tabIndex>0 by sorting)
  //--------------------------------------------------------------------------
  inline void buildTabOrder(Widget* root, std::vector<Widget*>& out)
  {
    if (!root || !root->visible) return;
    if (root->wantsFocus()) out.push_back(root);
    // first collect children with tabIndex>0 separately to sort
    std::vector<Widget*> normal, custom;
    for (auto& c : root->children) {
      if (!c->visible) continue;
      if (c->tabIndex > 0) custom.push_back(c.get()); else normal.push_back(c.get());
    }
    std::sort(custom.begin(), custom.end(), [](Widget* a, Widget* b) { return a->tabIndex < b->tabIndex; });
    for (auto* c : custom) buildTabOrder(c, out);
    for (auto* c : normal) buildTabOrder(c, out);
  }

  //--------------------------------------------------------------------------
  inline Widget* nextByTab(Widget* root, Widget* cur)
  {
    std::vector<Widget*> order; buildTabOrder(root, order);
    if (order.empty()) return nullptr;
    if (!cur) return order.front();
    auto it = std::find(order.begin(), order.end(), cur);
    if (it == order.end() || ++it == order.end()) return order.front();
    return *it;
  }

  //--------------------------------------------------------------------------
  inline Widget* prevByTab(Widget* root, Widget* cur)
  {
    std::vector<Widget*> order; buildTabOrder(root, order);
    if (order.empty()) return nullptr;
    if (!cur) return order.back();
    auto it = std::find(order.begin(), order.end(), cur);
    if (it == order.begin() || it == order.end()) return order.back();
    --it; return *it;
  }

  // Spatial (arrow/D-pad) – pick the best rect in a direction
  //--------------------------------------------------------------------------
  inline float dot(glm::vec2 a, glm::vec2 b) { return a.x * b.x + a.y * b.y; }

  //--------------------------------------------------------------------------
  inline Widget* bestInDirection(Widget* root, Widget* from, NavDir dir)
  {
    if (!from) return nullptr;
    // collect candidates
    std::vector<Widget*> cand; buildTabOrder(root, cand);
    auto itMe = std::find(cand.begin(), cand.end(), from);
    if (itMe == cand.end()) return nullptr;

    // direction vector
    glm::vec2 d = { 0,0 };
    if (dir == NavDir::Right) d = { 1,0 }; else if (dir == NavDir::Left) d = { -1,0 };
    else if (dir == NavDir::Down) d = { 0,1 }; else d = { 0,-1 };

    // from center
    Rect R = from->rect; glm::vec2 C{ R.x + R.w * 0.5f, R.y + R.h * 0.5f };

    Widget* best = nullptr; float bestScore = 1e30f;
    for (Widget* w : cand) {
      if (w == from) continue;
      Rect q = w->rect; glm::vec2 P{ q.x + q.w * 0.5f, q.y + q.h * 0.5f };
      glm::vec2 v = { P.x - C.x, P.y - C.y };
      // Must be in forward half-plane
      if (dot(v, d) <= 0.0f) continue;
      // score: angle bias + distance (smaller is better)
      float dist2 = v.x * v.x + v.y * v.y;
      float align = 1.0f - std::abs(dot(glm::normalize(v), d)); // 0=perfectly aligned
      float score = dist2 * (1.0f + 0.75f * align);
      if (score < bestScore) { bestScore = score; best = w; }
    }
    return best;
  }

} // namespace