#include "GrowthPopup.h"
#include "Value.h"
#include "Context.h"
#include "UISystem.h"

using namespace UI_lib;

static std::string getStr(const Payload& p, const char* k)
{
  if (auto v = p.get(k); v && v->asStr()) return *v->asStr();
  return {};
}

static int getInt(const Payload& p, const char* k, int def = 0)
{
  if (auto v = p.get(k); v && v->asInt()) return (int)*v->asInt();
  if (auto v = p.get(k); v && v->asDouble()) return (int)*v->asDouble();
  return def;
}

// Helpers (same style as your ChooseJob popup)
static inline std::string GPStr(const UI_lib::Payload& node, const char* k, const char* def = "")
{
  if (auto v = node.get(k); v && v->asStr()) return *v->asStr();
  return def ? std::string(def) : std::string();
}

static inline int GPInt(const UI_lib::Payload& node, const char* k, int def = 0)
{
  if (auto v = node.get(k); v && v->asInt())   return (int)*v->asInt();
  if (auto v = node.get(k); v && v->asDouble())return (int)*v->asDouble();
  return def;
}

//--------------------------------------------------------------------------
void GrowthPopup::onOpen(const std::string& resKey, Widget* _parent)
{
  parent = _parent; // (PopupPanel already has `parent` in your EIPopup style; if not, store m_parent)

  id = "GrowthPopup";
  nineName = "parchment_bar";
  visible = true;

  // ModalLayer::open(popup, anchor, resKeyOrData)
  // You said you pass `data` as resKey-like string OR via ModalLayer custom storage.
  // In your current modal system, you likely pass "data" as resKey (string) OR a payload object.
  // If ModalLayer provides the payload, just read it here. If not, keep what you already do in ChooseJobPopup.
  // Assume ModalLayer sets popup->resourceKey or calls onOpen with a serialized key; but you already did it for ChooseJobPopup.
  // ---- Practical approach: ModalLayer should pass a Payload map to PopupPanel before calling onOpen.
  // If you already store it in a member, parse from there. I’ll assume PopupPanel has `resourceKey` only:
  // => use ctx.hub payload-based init instead if needed.

  buildChildren();
}

//--------------------------------------------------------------------------
void GrowthPopup::apply(const Payload& data)
{
  m_title = GPStr(data, "title", "Cannons or butter?");
  m_scopeId = GPStr(data, "scopeId", "");

  m_rows.clear();

  if (auto rv = data.get("rows"); rv && rv->asVec())
  {
    for (const auto& it : rv->asVec()->data)
    {
      auto* m = it.asMap();
      if (!m) continue;

      Row r;
      r.code = GPStr(it, "code", "");
      r.label = GPStr(it, "label", r.code.c_str());
      r.sprite = GPStr(it, "sprite", "");
      r.cost.clear();

      if (auto cv = it.get("cost"); cv && cv->asVec())
      {
        for (const auto& cit : cv->asVec()->data)
        {
          auto* cm = cit.asMap();
          if (!cm) continue;

          CostItem ci;
          ci.qty = GPInt(cit, "qty", 0);
          ci.sprite = GPStr(cit, "sprite", "");
          if (ci.qty > 0 && !ci.sprite.empty())
            r.cost.push_back(std::move(ci));
        }
      }

      if (!r.code.empty())
        m_rows.push_back(std::move(r));
    }
  }

  // If already open, rebuild live
  if (m_root)
  {
    if (m_titleLb) m_titleLb->text = m_title.c_str();
    rebuildRows();
  }
}

//--------------------------------------------------------------------------
void GrowthPopup::onClose(UiContext& ctx, const char* reason)
{
  // Optional: tell controller it was dismissed (outside click / escape)
  Payload out = Payload::object({
    {"scopeId", Payload(m_scopeId)},
    {"reason",  Payload(std::string(reason ? reason : "close"))}
    });
  ctx.hub.post("ui.growth.closed", out);
}

//--------------------------------------------------------------------------
void GrowthPopup::buildChildren()
{
  children.clear();
  m_root = nullptr;
  m_titleLb = nullptr;
  m_sv = nullptr;

  // --- hard safety in case header init was missed ---
  if (m_w <= 0)      m_w = 320;
  if (m_pad < 0)     m_pad = 12;
  if (m_titleH <= 0) m_titleH = 36;
  if (m_rowH <= 0)   m_rowH = 44;

  const int totalH = m_pad + m_titleH + (int)m_rows.size() * m_rowH + m_pad;

  const float W = (float)m_w;
  const float H = (float)std::max(140, totalH);

  // IMPORTANT:
  // Keep rect.x/y as set by ModalLayer (anchor positioning),
  // but enforce our desired size.
  rect.w = W;
  rect.h = H;

  // Also set local, because some layout code sizes popups from local.
  local = Rect{ 0, 0, W, H };

  auto root = std::make_unique<Panel>();
  root->id = "GR.Root";
  root->local = Rect{ 0, 0, W, H };
  root->visible = true;
  m_root = root.get();

  auto title = std::make_unique<Label>();
  title->id = "GR.Title";
  title->text = m_title.c_str();
  title->fontSize = 24;
  title->color = 0x3A2A12FF;
  title->halign = Label::HAlign::Center;
  title->hitTestSelf = false;
  title->local = Rect{ 0.0f, (float)m_pad, W, (float)m_titleH };
  m_titleLb = title.get();

  auto sv = std::make_unique<ScrollView>();
  sv->id = "GR.List";
  sv->local = Rect{
    (float)m_pad,
    (float)(m_pad + m_titleH),
    std::max(1.0f, W - 2.0f * (float)m_pad),
    std::max(1.0f, H - (float)(m_pad + m_titleH + m_pad))
  };
  sv->visible = true;
  sv->wheelStep = 60;
  sv->showScrollbar = true;
  m_sv = sv.get();

  root->children.push_back(std::move(title));
  root->children.push_back(std::move(sv));
  children.push_back(std::move(root));

  rebuildRows();
}

//--------------------------------------------------------------------------
void GrowthPopup::clearDynamicRows()
{
  if (!m_sv) return;
  auto& kids = m_sv->children;
  for (int i = (int)kids.size() - 1; i >= 0; --i)
  {
    if (!kids[i]) continue;
    if (kids[i]->id.rfind("GR.Row.", 0) == 0 || kids[i]->id == "GR.__spacer")
      kids.erase(kids.begin() + i);
  }
}

//--------------------------------------------------------------------------
bool GrowthPopup::onEvent(UiContext& ctx, const UIEvent& e)
{
  // Ensure row onClick lambdas can post to hub without singletons
  m_ctx = &ctx;
  return PopupPanel::onEvent(ctx, e); // let base do standard popup routing
}

//--------------------------------------------------------------------------
void GrowthPopup::rebuildRows()
{
  if (!m_sv) return;

  // Clear old rows
  auto& kids = m_sv->children;
  kids.erase(std::remove_if(kids.begin(), kids.end(),
    [](std::unique_ptr<Widget>& w) {
      return w && (w->id.rfind("GR.Row.", 0) == 0);
    }), kids.end());

  const float rowW = m_sv->local.w;

  // --- layout constants (match your list-row vibe) ---
  const float kPadL = 8.0f;
  const float kIconX = 10.0f;
  const float kIconY = 10.0f;
  const float kIconS = 24.0f;
  const float kTextX = 44.0f;
  const float kTextY = 8.0f;
  const float kTextH = 28.0f;

  const float kCostIconS = 20.0f;
  const float kCostIconY = 12.0f;

  const float kCostNumW = 44.0f;
  const float kCostRightPad = 32.0f;
  const float kCostIconX = rowW - kCostRightPad - kCostIconS;
  const float kCostNumX = kCostIconX - 8.0f - kCostNumW;

  for (int i = 0; i < (int)m_rows.size(); ++i)
  {
    const Row& r = m_rows[i];
    const float y = (float)(i * m_rowH);

    // --- Row container (visuals live here; NOT clickable) ---
    auto row = std::make_unique<Panel>();
    row->id = "GR.Row." + std::to_string(i);
    row->local = Rect{ 0.0f, y, rowW, (float)m_rowH };
    row->visible = true;
    row->hitTestSelf = false;
    row->hitTestChildren = true;

    // --- Left icon (non-pickable) ---
    auto icon = std::make_unique<Image>();
    icon->id = row->id + ".icon";
    icon->spriteName = r.sprite;
    icon->local = Rect{ kIconX, kIconY, kIconS, kIconS };
    icon->draggable = false;
    icon->hitTestSelf = false;
    icon->hitTestChildren = false;
    row->children.push_back(std::move(icon));

    // --- Label (non-pickable) ---
    auto lb = std::make_unique<Label>();
    lb->id = row->id + ".label";
    lb->text = r.label.c_str();
    lb->fontSize = 20;
    lb->color = 0x3A2A12FF;
    lb->halign = Label::HAlign::Left;
    lb->local = Rect{ kTextX, kTextY, rowW - 160.0f, kTextH };
    lb->hitTestSelf = false;
    lb->hitTestChildren = false;
    row->children.push_back(std::move(lb));

    // --- Cost (non-pickable) ---
    if (!r.cost.empty() && r.cost[0].qty > 0)
    {
      const int qty = r.cost[0].qty;
      const std::string spr = r.cost[0].sprite;

      auto costLb = std::make_unique<Label>();
      costLb->id = row->id + ".cost";
      costLb->text = std::to_string(qty).c_str();
      costLb->fontSize = 20;
      costLb->color = 0x3A2A12FF;
      costLb->halign = Label::HAlign::Right;
      costLb->local = Rect{ kCostNumX, kTextY, kCostNumW, kTextH };
      costLb->hitTestSelf = false;
      costLb->hitTestChildren = false;
      row->children.push_back(std::move(costLb));

      auto costIcon = std::make_unique<Image>();
      costIcon->id = row->id + ".costIcon";
      costIcon->spriteName = spr;
      costIcon->local = Rect{ kCostIconX, kCostIconY, kCostIconS, kCostIconS };
      costIcon->draggable = false;
      costIcon->hitTestSelf = false;
      costIcon->hitTestChildren = false;
      row->children.push_back(std::move(costIcon));
    }

    // --- Click / hover layer (full-row hitbox ON TOP) ---
    auto hit = std::make_unique<IconButton>();
    hit->id = row->id + ".hit";
    hit->focusable = true;
    hit->local = Rect{ 0.0f, 0.0f, rowW, (float)m_rowH };

    // Critical: receives events; does NOT test children
    hit->hitTestSelf = true;
    hit->hitTestChildren = false;

    // Visual feedback (same idea as your bg)
    hit->spriteName = "parchment_bar"; // or "" if you prefer fully invisible
    hit->tintNormal = 0xFFFFFF00;
    hit->tintHover = 0xFFFFFF2A;
    hit->tintDown = 0xFFFFFF55;

    hit->onClick = [this, code = r.code]() {
      if (!m_ctx) return;
      Payload out = Payload::object({
        {"scopeId", Payload(m_scopeId)},
        {"code",    Payload(code)}
        });
      m_ctx->hub.post("ui.growth.chosen", out);
      };

    // Add LAST so it is on top for hit-test
    row->children.push_back(std::move(hit));

    m_sv->children.push_back(std::move(row));
  }

  // Make scroll content height sane (if ScrollView needs it)
  // If ScrollView auto-sizes children, you can omit this.
  // Otherwise add an invisible spacer like before.
}

