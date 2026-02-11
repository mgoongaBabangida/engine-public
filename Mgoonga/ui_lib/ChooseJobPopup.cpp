#include "ChooseJobPopup.h"
#include "Context.h"
#include "UISystem.h"

namespace UI_lib {

  static constexpr uint32_t kText = 0x3A2A12FF;   // dark brown
  static constexpr uint32_t kTextDim = 0x4A3A22FF; // optional slightly softer

  static std::string PStr(const Payload& p, const char* k, const char* def = "")
  {
    if (auto v = p.get(k); v && v->asStr()) return *v->asStr();
    return def ? def : "";
  }
  static int PInt(const Payload& p, const char* k, int def = 0)
  {
    if (auto v = p.get(k); v && v->asInt()) return (int)*v->asInt();
    return def;
  }

  ChooseJobPopup::ChooseJobPopup()
  {
    id = "ChooseJobPopup";
    nineName = "parchment_bar";   // match your popup styling
    visible = true;

    // Defaults for testing (safe if apply() not called)
    m_title = "Choose a Profession for Free Colonist";
    m_tileIndex = -1;
    m_rows = {
      {"farmer", "Farmer", "wheat", 4},
      {"cotton_planter", "Cotton Planter", "cotton", 6}
    };
  }

  void ChooseJobPopup::apply(const Payload& data)
  {
    m_title = PStr(data, "title", "Choose a Profession");
    m_tileIndex = PInt(data, "tileIndex", -1);

    if (auto ek = data.get("entityKey"); ek && ek->asStr())
      m_entityKey = *ek->asStr();

    m_rows.clear();
    if (auto rv = data.get("rows"); rv && rv->asVec())
    {
      for (const auto& it : rv->asVec()->data)
      {
        auto* m = it.asMap();
        if (!m) continue;

        Row r;
        r.job = PStr(it, "job", "");
        r.name = PStr(it, "name", r.job.c_str());
        r.sprite = PStr(it, "sprite", "");
        r.amount = PInt(it, "amount", 0);

        if (!r.job.empty())
          m_rows.push_back(std::move(r));
      }
    }

    // If already open, rebuild live
    if (m_root) {
      if (m_titleLb) m_titleLb->text = m_title.c_str();
      rebuildRows();
    }
  }

  void ChooseJobPopup::onOpen(const std::string& resKey, Widget* _parent)
  {
    m_resKey = resKey;
    m_parent = _parent;
    buildChildren();
  }

  void ChooseJobPopup::onClose(UiContext& ctx, const char* reason)
  {
    // Optional but very useful for controller flow:
    // controller can decide to revert a temporary assignment, etc.
    Payload out = Payload::object({
      {"tileIndex", Payload((int64_t)m_tileIndex)},
      {"reason",    Payload(std::string(reason ? reason : "close"))}
      });
    ctx.hub.post("ui.choose_job.closed", out);
  }

  bool ChooseJobPopup::onEvent(UiContext& ctx, const UIEvent& e)
  {
    // Ensure row onClick lambdas can post to hub without singletons
    m_ctx = &ctx;
    return PopupPanel::onEvent(ctx, e); // let base do standard popup routing
  }

  void ChooseJobPopup::buildChildren()
  {
    children.clear();
    m_root = nullptr;
    m_titleLb = nullptr;
    m_list = nullptr;

    // Popup size based on rows
    const int totalH = m_pad + m_titleH + (int)m_rows.size() * m_rowH + m_pad;
    rect = Rect{ rect.x, rect.y, (float)m_w, (float)std::max(120, totalH) };

    auto root = std::make_unique<Panel>();
    root->id = "CJ.Root";
    root->anchors = {}; // none
    root->local = Rect{ 0, 0, rect.w, rect.h };
    root->visible = true;
    m_root = root.get();

    auto title = std::make_unique<Label>();
    title->id = "CJ.Title";
    title->text = m_title.c_str();
    title->fontSize = 22;
    title->color = kText;
    title->halign = Label::HAlign::Center;
    title->hitTestSelf = false;
    title->local = Rect{ 0.0f, (float)m_pad, (float)m_w, (float)m_titleH };
    m_titleLb = title.get();

    auto list = std::make_unique<Panel>();
    list->id = "CJ.List";
    list->local = Rect{ (float)m_pad, (float)(m_pad + m_titleH), (float)(m_w - 2 * m_pad), 1.0f };
    list->visible = true;
    m_list = list.get();

    root->children.push_back(std::move(title));
    root->children.push_back(std::move(list));

    children.push_back(std::move(root));

    rebuildRows();
  }

  void ChooseJobPopup::clearDynamicRows()
  {
    if (!m_list) return;
    auto& kids = m_list->children;
    for (int i = (int)kids.size() - 1; i >= 0; --i) {
      if (kids[i] && kids[i]->id.rfind("CJ.Row.", 0) == 0)
        kids.erase(kids.begin() + i);
    }
  }

  void ChooseJobPopup::rebuildRows()
  {
    if (!m_list) return;

    // Resize list to fit rows
    m_list->local.h = (float)((int)m_rows.size() * m_rowH);

    clearDynamicRows();

    const float rowW = m_list->local.w;

    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
      const Row& r = m_rows[i];
      const float y = (float)(i * m_rowH);

      // ---- Row container (visuals live here; NOT clickable) ----
      auto row = std::make_unique<Panel>();
      row->id = "CJ.Row." + std::to_string(i);
      row->local = Rect{ 0.0f, y, rowW, (float)m_rowH };
      row->visible = true;
      row->hitTestSelf = false;
      row->hitTestChildren = true; // children exist, but they don't hit-test

      // Optional: add a subtle background image if you want (kept off here)

      // ---- Click / hover layer (full-row hitbox ON TOP) ----
      auto hit = std::make_unique<IconButton>();
      hit->id = row->id + ".hit";
      hit->spriteName = "";            // invisible
      hit->focusable = true;
      hit->local = Rect{ 0.0f, 0.0f, rowW, (float)m_rowH };

      // Critical: this prevents children from hijacking hover/click
      hit->hitTestSelf = true;
      hit->hitTestChildren = false;

      // Also make sure the button itself doesn't wash out visuals
      // (these are "no-op" visually since spriteName is empty, but safe)
      hit->opacity = 1.0f;
      hit->tintNormal = 0xFFFFFFFF;
      hit->tintHover = 0xFFFFFFFF;
      hit->tintDown = 0xFFFFFFFF;

      hit->onClick = [this, job = r.name, amt = r.amount, spr = r.sprite]() { // ! use name as job description, r.job is for UI Label
        if (!m_ctx) return;
        Payload out = Payload::object({
          {"entityKey", Payload(m_entityKey)},
          {"job",       Payload(job)},
          {"tileIndex", Payload((int64_t)m_tileIndex)},
          {"amount",    Payload((int64_t)amt)},
          {"sprite",    Payload(spr)}
          });
        m_ctx->hub.post("ui.choose_job.chosen", out);
        };

      // ---- Visuals ----
      auto icon = std::make_unique<Image>();
      icon->id = row->id + ".icon";
      icon->spriteName = r.name;
      icon->hitTestSelf = false;
      icon->hitTestChildren = false;
      icon->draggable = false;
      icon->local = Rect{ 10.0f, 10.0f, 16.0f, 34.0f };

      auto name = std::make_unique<Label>();
      name->id = row->id + ".name";
      name->text = r.job.c_str();
      name->fontSize = 20;
      name->color = kText;
      name->halign = Label::HAlign::Left;
      name->hitTestSelf = false;
      name->hitTestChildren = false;
      name->local = Rect{ 44.0f, 8.0f, rowW - 160.0f, 28.0f };

      auto amtLb = std::make_unique<Label>();
      amtLb->id = row->id + ".amt";
      amtLb->text = std::to_string(r.amount).c_str();
      amtLb->fontSize = 20;
      amtLb->color = kText;
      amtLb->halign = Label::HAlign::Right;
      amtLb->hitTestSelf = false;
      amtLb->hitTestChildren = false;
      amtLb->local = Rect{ rowW - 70.0f, 8.0f, 40.0f, 28.0f };

      auto resIcon = std::make_unique<Image>();
      resIcon->id = row->id + ".res";
      resIcon->spriteName = r.sprite;
      resIcon->hitTestSelf = false;
      resIcon->hitTestChildren = false;
      resIcon->draggable = false;
      resIcon->local = Rect{ rowW - 26.0f, 10.0f, 20.0f, 20.0f };

      // Add visuals first, then hitbox LAST so it is on top for routing
      row->children.push_back(std::move(icon));
      row->children.push_back(std::move(name));
      row->children.push_back(std::move(amtLb));
      row->children.push_back(std::move(resIcon));
      row->children.push_back(std::move(hit));

      m_list->children.push_back(std::move(row));
    }
  }

} // namespace UI_lib
