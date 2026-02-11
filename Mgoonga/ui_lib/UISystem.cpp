#include "stdafx.h"

#include"UISystem.h"
#include "SignalHub.h"
#include "Context.h"
#include "Utils.h"
#include "ChooseJobPopup.h"
#include "GrowthPopup.h"

#include <filesystem>
#include <unordered_set>
namespace fs = std::filesystem;

namespace UI_lib
{
  static void applyPresetTo(Widget& w)
  {
    using Preset = Widget::AnimCfg::Preset;
    auto& tr = w.trans;

    // defaults
    tr.dur_open = 0.18f; tr.dur_close = 0.14f;
    tr.ease_open = w.animCfg.easeOpen;
    tr.ease_close = w.animCfg.easeClose;

    switch (w.animCfg.preset) {
    case Preset::Popup:      // slight scale-in, rise, fade
      tr.scale = { 0.96f,0.96f };
      tr.offset = { 0, +8 };
      tr.opacity = 0.f;
      break;
    case Preset::PanelLeft:  // slide from left, fade
      tr.scale = { 1.f,1.f };
      tr.offset = { -24, 0 };
      tr.opacity = 0.f;
      break;
    case Preset::PanelRight:
      tr.scale = { 1.f,1.f };
      tr.offset = { +24, 0 };
      tr.opacity = 0.f;
      break;
    case Preset::ToastTop:
      tr.scale = { 1.f,1.f };
      tr.offset = { 0, -16 };
      tr.opacity = 0.f;
      break;
    case Preset::ToastBottom:
      tr.scale = { 1.f,1.f };
      tr.offset = { 0, +16 };
      tr.opacity = 0.f;
      break;
    case Preset::None:
      // no offset/scale bias
      tr.scale = { 1.f,1.f };
      tr.offset = { 0, 0 };
      tr.opacity = 0.f;
      break;
    }

    // Per-widget timing overrides
    if (w.animCfg.openMs >= 0) tr.dur_open = w.animCfg.openMs * 0.001f;
    if (w.animCfg.closeMs >= 0) tr.dur_close = w.animCfg.closeMs * 0.001f;

    // Optional explicit start pose overrides
    if (w.animCfg.startOpacity) tr.opacity = *w.animCfg.startOpacity;
    if (w.animCfg.startScale)   tr.scale = *w.animCfg.startScale;
    if (w.animCfg.startOffset)  tr.offset = *w.animCfg.startOffset;
    if (w.animCfg.startColor)   tr.color = *w.animCfg.startColor;
  }

  static inline std::string mkKey(const std::string& scope, const std::string& ent) {
    return scope + "|" + ent;
  }

  //-----------------------------------------------------------------------------
  static inline void trackPointer(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind != UIEvent::Kind::Pointer) return;
    const auto& pe = std::get<PointerEvent>(e.data);
    switch (pe.type) {
    case PointerEvent::Type::Move:
    case PointerEvent::Type::Down:
    case PointerEvent::Type::Up:
      ctx.tip.cursor = pe.pos;
      ctx.cursorPx = pe.pos;  // custom cursor uses this
      break;
    default: break;
    }
  }

  //------------------------------------------------------------------------
  inline void wireBindings(UiContext& ctx, Widget* w)
  {
    if (!w) return;

    // Label: bindText -> text
    if (auto* lbl = dynamic_cast<Label*>(w)) {
      if (!lbl->bindText.empty()) {
        ctx.hub.subscribe(lbl->bindText, [lbl, &ctx](const Payload& v) {
          lbl->text = PayloadString(v);
          ctx.invalidate();
          });
      }
    }

    // Button: bindText -> text (useful for showing dynamic counters on buttons)
    if (auto* btn = dynamic_cast<Button*>(w)) {
      if (!btn->bindText.empty()) {
        ctx.hub.subscribe(btn->bindText, [btn, &ctx](const Payload& v) {
          btn->text = PayloadString(v);
          ctx.invalidate();
          });
      }
    }

    if (!w->animCfg.openSignal.empty()) {
      ctx.hub.subscribe(w->animCfg.openSignal, [w, &ctx](const Payload&) {
        w->open(); ctx.invalidate();
        });
    }
    if (!w->animCfg.closeSignal.empty()) {
      ctx.hub.subscribe(w->animCfg.closeSignal, [w, &ctx](const Payload&) {
        w->close(); ctx.invalidate();
        });
    }

    // Recurse children
    for (auto& c : w->children) wireBindings(ctx, c.get());
  }

  //-----------------------------------------------------------------------------------------------
  static inline void drawDragGhost(UiContext& ctx)
  {
    const auto& d = ctx.drag;
    if (!d.active || !d.thresholdMet) return;

    // Compute ghost rect
    float gw = 48.f, gh = 48.f; // default
    // 1) If sprite visual provided, get its px size from atlas
    if (!d.visual.spriteName.empty()) {
      if (auto spr = ctx.findSprite(d.visual.spriteName)) {
        gw = spr->uv.w; gh = spr->uv.h;
      }
    }
    // 2) Override by visual.size if provided
    if (d.visual.size.x > 0) gw = d.visual.size.x;
    if (d.visual.size.y > 0) gh = d.visual.size.y;

    const float dx = d.pos.x - d.visual.hotspot.x * gw;
    const float dy = d.pos.y - d.visual.hotspot.y * gh;
    Rect dst{ dx, dy, gw, gh };

    // “no-drop” tint overlay if not acceptable right now
    uint32_t tint = d.visual.tint;
    if (!d.anyAccepting) {
      // multiply alpha and add reddish
      uint32_t A = (tint) & 0xFFu, R = (tint >> 24) & 0xFFu, G = (tint >> 16) & 0xFFu, B = (tint >> 8) & 0xFFu;
      R = std::min(255u, R + 60u);
      G = (uint32_t)(G * 0.6f);
      tint = (R << 24) | (G << 16) | (B << 8) | A;
    }

    // Draw sprite if provided, else draw a faint rect
    if (!d.visual.spriteName.empty()) {
      if (auto spr = ctx.findSprite(d.visual.spriteName)) {
        ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, dst, spr->uv, tint, ctx.viewport);
        return;
      }
    }
    ctx.prim->solidRect(*ctx.dl, dst, tint, ctx.viewport);
  }

  //--------------------------------------------------------------------------------------------------------------------------
  void UiSystem::initialize(const std::string& themePath, const std::string& atlasPath, const std::string& layoutPath, int fbW, int fbH, int virtW, int virtH)
  {
    registerBuiltins();
    registerLegacy();     // from the canvas adapter

    ctx.fbW = fbW; ctx.fbH = fbH; ctx.virtualW = virtW; ctx.virtualH = virtH;
    ctx.prim = &m_prim;
    ctx.dl = &m_drawList;
    ctx.animator = &m_animator;

    loadThemeYaml(themePath, ctx.theme);
    ctx.tipCfg = ctx.theme.tipCfg; //?
    
    // --- MULTI-ATLAS LOAD (YAML -> ctx.atlases) ---
    try {
      if (fs::exists(atlasPath) && fs::is_directory(atlasPath))
      {
        const auto panels = (fs::path(atlasPath) / "atlas_panels.yaml").string();
        const auto topbar = (fs::path(atlasPath) / "atlas_city.yaml").string();
        const auto citizens = (fs::path(atlasPath) / "atlas_citizens.yaml").string();
        const auto cityview = (fs::path(atlasPath) / "atlas_cityview.yaml").string();
        const auto buildings = (fs::path(atlasPath) / "atlas_buildings.yaml").string();

        if (fs::exists(panels)) loadAtlasYaml(ctx, "panels", panels);
        if (fs::exists(topbar)) loadAtlasYaml(ctx, "topbar", topbar);
        if (fs::exists(citizens)) loadAtlasYaml(ctx, "citizens", citizens);
        if (fs::exists(cityview)) loadAtlasYaml(ctx, "cityview", cityview);
        if (fs::exists(buildings)) loadAtlasYaml(ctx, "buildings", buildings);
      }
      else {
        // fallback: single file, atlas name "default"
        loadAtlasYaml(ctx, "default", atlasPath);
      }
    }
    catch (...) {
      fprintf(stderr, "[UI] atlasPath check failed: %s\n", atlasPath.c_str());
      // still continue; you can load later if needed
    }
    ctx.root = &root;

    modal = std::make_unique<ModalLayer>();

    loadView("main", layoutPath, 100, /*isScreen=*/true);
    loadView("upper_bar", "ui/layout/upper_bar.yaml", 50, /*isScreen=*/true);

    // Show them
    showView("main");
    showView("upper_bar");

    mountingHost.rebuildIdIndex(activeRoot());
    wireBindings(ctx, activeRoot());

    ctx.hub.subscribe("ui.applyView", [this](const Payload& v) {
      auto* m = v.asMap(); if (!m) return;

      const std::string scopeId = PayloadString(*v.get("scopeId"), "global");

      if (auto bl = v.get("cityName"); bl && bl->asStr())
      {
        if (auto* title = findAny<UI_lib::Label>("TBTitle"))
          title->text = bl->asStr()->c_str();
      }

      if (auto bl = v.get("pop"); bl && bl->asInt())
      {
        if (auto* title = findAny<UI_lib::Label>("TBPop"))
          title->text = std::to_string(*bl->asInt());
      }

      // --- (TB) Topbar rich tooltips -----------------------------------------------
      {
        auto fmtSigned = [](int64_t v) -> std::string {
          if (v > 0) return "+" + std::to_string(v);
          return std::to_string(v); // includes 0 and negatives
          };

        auto fmtFixed2 = [](double x) -> std::string {
          std::ostringstream oss;
          oss.setf(std::ios::fixed);
          oss << std::setprecision(2) << x;
          return oss.str();
          };

        auto clearTip = [](UI_lib::Widget* w, const char* title) {
          if (!w) return;
          w->hasRichTip = true;
          w->richTip.title = title;
          w->richTip.titleSize = 18;
          w->richTip.titleColor = 0x3A2A12FF;
          w->richTip.rows.clear();
          };

        auto addRow = [](UI_lib::Widget* w,
          const std::string& icon,
          const std::string& text,
          uint32_t color = 0x3A2A12FF,
          int fontSize = 16)
          {
            if (!w) return;
            UI_lib::RichTipRow r;
            r.icon = icon;
            r.text = text;
            r.color = color;
            r.fontSize = fontSize;
            w->richTip.rows.push_back(std::move(r));
          };

        int64_t surplus = 0;
        // --- TBIconGrowth (Image): yield/consumption/surplus (wheat icon in every row)
        if (auto gg = v.get("tbGrowth"); gg && gg->asMap())
        {
          const int64_t y = (gg->get("yield") && gg->get("yield")->asInt()) ? *gg->get("yield")->asInt() : 0;
          const int64_t c = (gg->get("consumption") && gg->get("consumption")->asInt()) ? *gg->get("consumption")->asInt() : 0;
          const int64_t s = (gg->get("surplus") && gg->get("surplus")->asInt()) ? *gg->get("surplus")->asInt() : 0;
          surplus = s;

          if (auto* im = findAny<UI_lib::Image>("TBIconGrowth"))
          {
            clearTip(im, "Food");
            addRow(im, "wheat", "Yield   " + fmtSigned(y));
            addRow(im, "wheat", "Consumption   " + fmtSigned(-c)); // show as negative
            addRow(im, "wheat", "Surplus   " + fmtSigned(s));
          }
        }

        // --- TBGrowth (Label): base/health/stock/needed
        if (auto gc = v.get("tbGrowthCalc"); gc && gc->asMap())
        {
          const int64_t base = (gc->get("base") && gc->get("base")->asInt()) ? *gc->get("base")->asInt() : 0;
          const int64_t health = (gc->get("health") && gc->get("health")->asInt()) ? *gc->get("health")->asInt() : 0;
          const int64_t stock = (gc->get("stock") && gc->get("stock")->asInt()) ? *gc->get("stock")->asInt() : 0;
          const int64_t needed = (gc->get("needed") && gc->get("needed")->asInt()) ? *gc->get("needed")->asInt() : (base + health - stock);

          if (auto* lb = findAny<UI_lib::Label>("TBGrowth"))
          {
            clearTip(lb, "Growth");
            addRow(lb, "", "Base   " + fmtSigned(base));
            addRow(lb, "", "Health   " + fmtSigned(health));
            addRow(lb, "", "Stock   " + fmtSigned(-stock)); // stock reduces "needed"
            addRow(lb, "", "Needed   " + fmtSigned(needed));
            if (surplus > 0)
            {
              int turnsToGrow = needed % surplus == 0 ? needed / surplus : needed / surplus + 1; // @todo this math should be in sim
              lb->text = "+" + std::to_string(surplus) + " (" + std::to_string(turnsToGrow) + " turns to grow)";
            }
            else if (surplus == 0)
              lb->text = "stagnation";
            else
              lb->text = "starvation"; // red color?
          }
        }

        // --- TBIconGold (Image): per-good rows + final gold income row
        if (auto gd = v.get("tbGold"); gd && gd->asMap())
        {
          if (auto* im = findAny<UI_lib::Image>("TBIconGold"))
          {
            clearTip(im, "Gold");

            auto lines = gd->get("lines");
            if (lines && lines->asVec())
            {
              for (const auto& ln : lines->asVec()->data)
              {
                auto* lm = ln.asMap(); if (!lm) continue;

                const std::string sprite =
                  (ln.get("sprite") && ln.get("sprite")->asStr()) ? *ln.get("sprite")->asStr() : "";

                const int64_t qty =
                  (ln.get("qty") && ln.get("qty")->asInt()) ? *ln.get("qty")->asInt() : 0;

                const double price =
                  (ln.get("price") && ln.get("price")->asDouble()) ? *ln.get("price")->asDouble() : 0.0;

                const double total =
                  (ln.get("total") && ln.get("total")->asDouble()) ? *ln.get("total")->asDouble() : (double)qty * price;

                // "X2   0.75   1.50"
                addRow(im, sprite,
                  "X" + std::to_string((int)qty) +
                  "   " + fmtFixed2(price) +
                  "   " + fmtFixed2(total));
              }
            }

            const double income =
              (gd->get("income") && gd->get("income")->asDouble()) ? *gd->get("income")->asDouble() : 0.0;

            addRow(im, "coin", "Income   " + fmtFixed2(income));
          }

          if (auto* lb = findAny<UI_lib::Label>("TBGpt"))
          {
            const double i = (gd->get("income") && gd->get("income")->asDouble()) ? *gd->get("income")->asDouble() : 0;
            lb->text = fmtFixed2(i);
          }
        }

        // --- TBIconHammer (Image):
        if (auto gp = v.get("tbProduction"); gp && gp->asMap())
        {
          if (auto* lb = findAny<UI_lib::Label>("TBHammers"))
          {
            const double i = (gp->get("production") && gp->get("production")->asDouble()) ? *gp->get("production")->asDouble() : 0;
            lb->text = fmtFixed2(i);
          }
        }

        // --- TBHealth (Image): base - pop - age - buildingsNeg + buildingsPos + oliveOil => total
        if (auto hd = v.get("tbHealth"); hd && hd->asMap())
        {
          if (auto* bt = findAny<UI_lib::Image>("TBHealth"))
          {
            clearTip(bt, "Status");

            const int64_t base =
              (hd->get("base") && hd->get("base")->asInt()) ? *hd->get("base")->asInt() : 0;

            const int64_t pop =
              (hd->get("population") && hd->get("population")->asInt()) ? *hd->get("population")->asInt() : 0;

            const int64_t age =
              (hd->get("age") && hd->get("age")->asInt()) ? *hd->get("age")->asInt() : 0;

            const int64_t bneg =
              (hd->get("buildingsNeg") && hd->get("buildingsNeg")->asInt()) ? *hd->get("buildingsNeg")->asInt() : 0;

            const int64_t bpos =
              (hd->get("buildingsPos") && hd->get("buildingsPos")->asInt()) ? *hd->get("buildingsPos")->asInt() : 0;

            const int64_t oo =
              (hd->get("oliveOil") && hd->get("oliveOil")->asInt()) ? *hd->get("oliveOil")->asInt() : 0;

            const int64_t total =
              (hd->get("total") && hd->get("total")->asInt()) ? *hd->get("total")->asInt()
              : (base - pop - age - bneg + bpos + oo);

            addRow(bt, "", "Base   " + fmtSigned(base));
            addRow(bt, "", "Population   " + fmtSigned(-pop));
            addRow(bt, "", "Age   " + fmtSigned(-age));
            addRow(bt, "", "Buildings   " + fmtSigned(-bneg));
            addRow(bt, "", "Buildings   " + fmtSigned(+bpos)); // second buildings row (bonus)
            addRow(bt, "oil", "Stock " + fmtSigned(+oo));
            addRow(bt, "", "Total   " + fmtSigned(total));
          }
        }
      }
      // ---------------------------------------------------------------------------

      // --- (R) Handle the resources -------------------
      if (auto bl = v.get("resources"); bl && bl->asVec())
      {
        // --- helpers scoped to this block ---
        auto findChildImage = [](UI_lib::Panel* parent, const std::string& cid) -> UI_lib::Image*
          {
            if (!parent) return nullptr;
            for (std::unique_ptr<Widget>& ch : parent->children)
              if (ch && ch->id == cid)
                if (UI_lib::Image* im = dynamic_cast<UI_lib::Image*>(ch.get())) return im;
            return nullptr;
          };

        auto removeChildById = [](UI_lib::Panel* parent, const std::string& cid)
          {
            if (!parent) return;
            std::vector<std::unique_ptr<Widget>>& kids = parent->children;
            kids.erase(std::remove_if(kids.begin(), kids.end(),
              [&](std::unique_ptr<Widget>& w) { return w.get() && w->id == cid; }), kids.end());
          };

        auto ensureBadge = [findChildImage](UI_lib::Panel* parent,
          const std::string& cid,
          const std::string& sprite,
          const Rect& r)
          {
            if (!parent) return;
            if (UI_lib::Image* im = findChildImage(parent, cid))
            {
              im->spriteName = sprite;
              Rect rect = { parent->rect.x + r.x,  parent->rect.y + r.y, r.h, r.w };
              im->rect = rect;
              im->clip = rect;
              im->local = r;
              im->visible = true;
              return;
            }
            // create new
            auto im = std::make_unique<UI_lib::Image>();
            im->id = cid;
            im->spriteName = sprite;
            Rect rect = { parent->rect.x + r.x,  parent->rect.y + r.y, r.h, r.w };
            im->rect = rect;
            im->clip = rect;
            im->local = r;
            im->tooltip = "";      // no tooltip for tiny badges
            im->draggable = false; // never a drag source
            //m->hitTest = false;   // never intercept pointer @todo
            parent->children.push_back(std::move(im));
          };

        // Layout constants for 100x88 slots
        constexpr int kSlotW = 100;
        constexpr int kSlotH = 88;
        constexpr int kBadgeW = 20;
        constexpr int kBadgeH = 20;

        const auto TL = Rect{ 2, 2, kBadgeW, kBadgeH };                                        // top-left
        const auto TR = Rect{ kSlotW - kBadgeW - 2, 2, kBadgeW, kBadgeH };                     // top-right
        const auto BR = Rect{ kSlotW - kBadgeW - 2, kSlotH - kBadgeH - 2 - 8, kBadgeW, kBadgeH };  // bottom-right

        int row_index = 0; std::string slotPrefix = "RefSlot";
        for (const auto& row : bl->asVec()->data)
        {
          if (row_index == 18) { row_index = 0; slotPrefix = "RawSlot"; }
          auto* rm = row.asMap(); if (!rm) { ++row_index; continue; }

          // Existing details
          const auto pr = row.get("price");
          const auto ex = row.get("exotic");
          if (!pr || !pr->asDouble() || !ex || !ex->asBool()) { ++row_index; continue; }
          const int price = (int)*pr->asDouble();
          const bool exotic = *ex->asBool();

          // NEW: stock qty (ResourceRow::stock comes as payload "qty")
          int stockQty = 0;
          if (auto qn = row.get("qty"); qn && qn->asInt())
            stockQty = (int)*qn->asInt();

          // New flags (missing → false)
          const bool exported = (row.get("exported") && row.get("exported")->asBool() && *row.get("exported")->asBool());
          const bool imported = (row.get("imported") && row.get("imported")->asBool() && *row.get("imported")->asBool());
          const bool nonconsumed = (row.get("nonconsumed") && row.get("nonconsumed")->asBool() && *row.get("nonconsumed")->asBool());

          if (auto* resPanel = findAny<UI_lib::Panel>(slotPrefix + std::to_string(row_index)))
          {
            // NEW: update the *existing* amount label with no guessing
            const bool isRaw = (slotPrefix == "RawSlot");
            const std::string amtId = (isRaw ? "RawAmt" : "RefAmt") + std::to_string(row_index);
            if (auto* amt = findAny<UI_lib::Label>(amtId))
              amt->text = std::to_string(stockQty);

            // Keep your richTip logic
            resPanel->hasRichTip = true;
            resPanel->richTip.title = "Resource";
            resPanel->richTip.titleSize = 18;
            resPanel->richTip.rows.clear();
            resPanel->richTip.rows.push_back({ "coin", "Price: " + std::to_string(price), 0x3A2A12FF, 16 });
            resPanel->richTip.rows.push_back({ "coin", std::string("Status: ") + (exotic ? "Exotic" : "Normal"), 0x3A2A12FF, 16 });

            // --- badges: add or remove as requested ---
            if (exported)
              ensureBadge(resPanel, "badge.export", "down-arrow", TL);
            else
              removeChildById(resPanel, "badge.export");

            if (imported)
              ensureBadge(resPanel, "badge.import", "top-arrow", BR);
            else
              removeChildById(resPanel, "badge.import");

            if (nonconsumed)
              ensureBadge(resPanel, "badge.stop", "stop-consume", TR);
            else
              removeChildById(resPanel, "badge.stop");
          }

          ++row_index;
        }
      }

      // --- (A) Ensure building slots exist/update their captions (optional) ---
      if (auto bl = v.get("buildings"); bl && bl->asVec())
      {
        for (const auto& row : bl->asVec()->data)
        {
          auto* rm = row.asMap(); if (!rm) continue;
          const auto sk = row.get("slotKey");
          if (!sk || !sk->asStr()) continue;
          const std::string slotKey = *sk->asStr();

          std::string displayName;
          if (auto name = row.get("name"); name && name->asStr())
            displayName = *name->asStr();

          mountingHost.ensureSlot(ctx, scopeId, slotKey, displayName);
        }
      }

      // --- (A2) Create/update building images inside BuildingsScroll -----------------
      if (auto bl = v.get("buildings"); bl && bl->asVec())
      {
        // Grab the scroll container where we want to place the images
        auto* scroll = findAny<UI_lib::ScrollView>("BuildingsScroll");
        if (scroll)
        {
          // We will attach images as children of the ScrollView itself
          // using ABS coords = parent->rect + local offset
          auto& kids = scroll->children;

          // helpers for unique_ptr<vector>
          auto findChildById = [](UI_lib::Widget* parent, const std::string& cid)
            -> std::pair<int, UI_lib::Widget*> {
            if (!parent) return { -1, nullptr };
            auto& v = parent->children;
            for (int i = 0; i < static_cast<int>(v.size()); ++i) {
              if (v[i] && v[i]->id == cid) return { i, v[i].get() };
            }
            return { -1, nullptr };
            };

          auto bringToTop = [](UI_lib::Widget* parent, int idx) {
            if (!parent || idx < 0 || idx >= static_cast<int>(parent->children.size())) return;
            auto up = std::move(parent->children[idx]);
            parent->children.erase(parent->children.begin() + idx);
            parent->children.push_back(std::move(up));
            };

          for (const auto& row : bl->asVec()->data)
          {
            auto* rm = row.asMap();
            if (!rm) continue;

            // required fields
            const auto c_code = row.get("name");
            const auto c_x = row.get("x");
            const auto c_y = row.get("y");
            const auto c_sx = row.get("size_x");
            const auto c_sy = row.get("size_y");
            const auto c_sprite = row.get("sprite");

            if (!c_code || !c_code->asStr() ||
              !c_x || !c_x->asInt() ||
              !c_y || !c_y->asInt() ||
              !c_sx || !c_sx->asInt() ||
              !c_sy || !c_sy->asInt() ||
              !c_sprite || !c_sprite->asStr())
            {
              continue;
            }

            const std::string code = *c_code->asStr();
            const int x = *c_x->asInt();
            const int y = *c_y->asInt();
            const int sx = *c_sx->asInt();
            const int sy = *c_sy->asInt();
            const std::string sprite = *c_sprite->asStr();
            if(sprite.empty())
              continue;
            const std::string imgId = "bimg." + code;

            // Absolute placement relative to the ScrollView's rect
            Rect abs{
              scroll->rect.x + x,
              scroll->rect.y + y,
              sx, sy
            };

            auto [idx, w] = findChildById(scroll, imgId);
            UI_lib::Image* im = nullptr;

            if (!w) {
              auto np = std::make_unique<UI_lib::Image>();
              np->id = imgId;
              np->spriteName = sprite;
              np->rect = abs;
              np->clip = abs;
              np->local = Rect{float(x),float(y),float(sx) ,float(sy) };
              np->visible = true;
              np->draggable = false;     // images are static visuals
              //np->hitTest = false;       // don't steal drag from citizens/slots
              //np->clipToParent = true;   // keep inside scroll area
              im = static_cast<UI_lib::Image*>(np.get());
              kids.push_back(std::move(np));
            }
            else {
              // ensure it's an Image; if not, replace
              im = dynamic_cast<UI_lib::Image*>(w);
              if (!im) {
                // replace the conflicting child
                scroll->children.erase(scroll->children.begin() + idx);
                auto np = std::make_unique<UI_lib::Image>();
                np->id = imgId;
                im = static_cast<UI_lib::Image*>(np.get());
                kids.push_back(std::move(np));
              }
              else {
                bringToTop(scroll, idx);
              }
              im->spriteName = sprite;
              im->rect = abs;
              im->clip = abs;
              im->local = Rect{ float(x),float(y),float(sx) ,float(sy) };
              im->visible = true;
              im->draggable = false;
              //im->hitTest = false;
              //im->clipToParent = true;
            }
          }
        }
      }

      // --- (A3) Manage building DropSlots: reparent by ID under BuildingsScroll, then position ----
      if (auto bl = v.get("buildings"); bl && bl->asVec())
      {
        auto* scroll = findAny<UI_lib::ScrollView>("BuildingsScroll");
        if (scroll)
        {
          // ---------- helpers ----------
          auto toLower = [](std::string s) {
            for (char& c : s) c = (char)std::tolower((unsigned char)c);
            return s;
            };

          auto isDescendantOf = [](UI_lib::Widget* w, UI_lib::Widget* ancestor) -> bool {
            for (UI_lib::Widget* p = w ? w->parent : nullptr; p; p = p->parent)
              if (p == ancestor) return true;
            return false;
            };

          // Protect slots that must remain exactly as authored in YAML (e.g., garrison).
          // This prevents us from reparenting/hiding them.
          auto isProtectedSlot = [](UI_lib::DropSlot* ds) -> bool
            {
              if (!ds) return false;
              // ID-based protection
              if (ds->id.find("Garrison") != std::string::npos) return true;
              // slotKey-based protection (if your tags use this)
              if (ds->slotKey.find("garrison") != std::string::npos) return true;
              return false;
            };

          auto bringPtrToTop = [](UI_lib::Widget* parent, UI_lib::Widget* w) {
            if (!parent || !w) return;
            auto& kids = parent->children;
            for (int i = 0; i < (int)kids.size(); ++i) {
              if (kids[i].get() == w) {
                auto up = std::move(kids[i]);
                kids.erase(kids.begin() + i);
                kids.push_back(std::move(up));
                return;
              }
            }
            };

          auto findChildImage = [](UI_lib::Widget* parent, const std::string& cid) -> UI_lib::Image*
            {
              if (!parent) return nullptr;
              for (auto& ch : parent->children)
                if (ch && ch->id == cid)
                  if (auto* im = dynamic_cast<UI_lib::Image*>(ch.get()))
                    return im;
              return nullptr;
            };

          // Detach w from its current parent and attach to newParent (both own via unique_ptr)
          auto reparentTo = [](UI_lib::Widget* w, UI_lib::Widget* newParent) -> bool
            {
              if (!w || !w->parent || !newParent) return false;
              UI_lib::Widget* oldParent = w->parent;

              auto& oldKids = oldParent->children;
              for (size_t i = 0; i < oldKids.size(); ++i)
              {
                if (oldKids[i].get() == w)
                {
                  auto up = std::move(oldKids[i]);
                  oldKids.erase(oldKids.begin() + i);

                  w->parent = newParent;
                  newParent->children.push_back(std::move(up));
                  return true;
                }
              }
              return false;
            };

          // Traverse and collect ALL building-managed DropSlots (id starts with "bld."),
          // EXCEPT protected ones (garrison etc.)
          std::unordered_map<std::string, std::vector<UI_lib::DropSlot*>> slotsById;

          std::function<void(UI_lib::Widget*)> collect = [&](UI_lib::Widget* w)
            {
              if (!w) return;
              if (auto* ds = dynamic_cast<UI_lib::DropSlot*>(w))
              {
                if (!isProtectedSlot(ds) && ds->id.rfind("bld.", 0) == 0)
                  slotsById[ds->id].push_back(ds);
              }
              for (auto& ch : w->children) collect(ch.get());
            };

          if (auto* city = findAny<UI_lib::Widget>("CityScreen"))
            collect(city);

          // Create a new DropSlot directly under BuildingsScroll (only if missing everywhere)
          auto createDropSlotUnderScroll = [&](const std::string& slotId,
            const std::string& dropTag,
            const std::string& tip) -> UI_lib::DropSlot*
            {
              auto np = std::make_unique<UI_lib::DropSlot>();
              np->id = slotId;
              np->slotKey = dropTag;          // DnD tag only (identity/layout uses id)
              np->tooltip = tip;

              np->local = Rect{ 0,0,65,140 };
              np->rect = Rect{ scroll->rect.x, scroll->rect.y, 65,140 };
              np->clip = np->rect;

              np->visible = true;
              //np->draggable = false;
              np->hitTestSelf = true;
              np->hitTestChildren = true;

              UI_lib::DropSlot* ds = static_cast<UI_lib::DropSlot*>(np.get());
              np->parent = scroll;
              scroll->children.push_back(std::move(np));

              // register in index (so duplicate hiding logic sees it)
              mountingHost.registerWidget(ds);
              slotsById[slotId].push_back(ds);
              return ds;
            };

          // ---------- 1) Build desired slots per building from payload ----------
          // NOTE: Your payload field "slotKey" currently contains the SLOT WIDGET ID like "bld.Market.slot0"
          std::unordered_map<std::string, std::vector<std::string>> desiredSlotsByBuilding;
          std::unordered_set<std::string> desiredSlotIds;

          for (const auto& row : bl->asVec()->data)
          {
            auto* rm = row.asMap(); if (!rm) continue;

            auto bn = row.get("name");
            if (!bn || !bn->asStr()) continue;
            const std::string bname = *bn->asStr();

            auto sid = row.get("slotKey"); // <-- slot widget id, e.g. "bld.Market.slot0"
            if (!sid || !sid->asStr()) continue;
            const std::string slotId = *sid->asStr();
            if (slotId.empty()) continue;

            desiredSlotsByBuilding[bname].push_back(slotId);
            desiredSlotIds.insert(slotId);
          }

          // ---------- 2) For each desired slotId: choose primary instance, reparent it under scroll, position it ----------
          std::unordered_map<std::string, UI_lib::DropSlot*> primary; // slotId -> instance we keep visible + position

          constexpr float kSlotW = 65.0f;
          constexpr float kSlotH = 140.0f;
          constexpr float kGap = 8.0f;

          for (auto& kv : desiredSlotsByBuilding)
          {
            const std::string& bname = kv.first;
            auto& slotIds = kv.second;
            if (slotIds.empty()) continue;

            UI_lib::Image* bimg = findChildImage(scroll, "bimg." + bname);
            if (!bimg) continue;

            const Rect br = bimg->rect;

            const int n = (int)slotIds.size();
            const float totalW = n * kSlotW + (n - 1) * kGap;

            const float cx = br.x + br.w * 0.5f;
            const float startX = cx - totalW * 0.5f;

            // lower half centering
            const float yAbs = (br.y + br.h * 0.5f) + ((br.h * 0.5f) - kSlotH) * 0.5f;

            const std::string dropTag = "job:" + toLower(bname);
            const std::string tip = "Drop here \u2192 " + bname;

            for (int i = 0; i < n; ++i)
            {
              const std::string& slotId = slotIds[i];

              // pick preferred existing instance:
              // 1) already under BuildingsScroll, else 2) any existing, else 3) create new
              UI_lib::DropSlot* chosen = nullptr;

              auto it = slotsById.find(slotId);
              if (it != slotsById.end() && !it->second.empty())
              {
                // prefer descendant of scroll
                for (auto* cand : it->second)
                {
                  if (!cand || isProtectedSlot(cand)) continue;
                  if (cand->parent == scroll || isDescendantOf(cand, scroll)) { chosen = cand; break; }
                }
                if (!chosen)
                {
                  // fall back to first non-protected
                  for (auto* cand : it->second)
                    if (cand && !isProtectedSlot(cand)) { chosen = cand; break; }
                }
              }

              if (!chosen)
                chosen = createDropSlotUnderScroll(slotId, dropTag, tip);

              // Ensure direct child of BuildingsScroll (so coords match bimg.* logic)
              if (chosen->parent != scroll)
                reparentTo(chosen, scroll);

              // Position: in scroll-local coords (like your building image code does)
              const float xAbs = startX + i * (kSlotW + kGap);
              const float lx = xAbs - scroll->rect.x;
              const float ly = yAbs - scroll->rect.y;

              chosen->local = Rect{ lx, ly, kSlotW, kSlotH };
              chosen->rect = Rect{ scroll->rect.x + lx, scroll->rect.y + ly, kSlotW, kSlotH };
              chosen->clip = chosen->rect;

              chosen->visible = true;
              chosen->tooltip = tip;

              bringPtrToTop(scroll, chosen);

              // only set slotKey if it looks unset (don’t fight YAML)
              if (chosen->slotKey.empty())
                chosen->slotKey = dropTag;

              primary[slotId] = chosen;
            }
          }

          auto eraseFromParent = [](UI_lib::Widget* w)
            {
              if (!w || !w->parent) return;
              auto& kids = w->parent->children;
              kids.erase(std::remove_if(kids.begin(), kids.end(),
                [&](const std::unique_ptr<UI_lib::Widget>& p) { return p.get() == w; }),
                kids.end());
            };

          auto adoptChildrenExceptPlaceholder = [](UI_lib::Widget* from, UI_lib::Widget* to)
            {
              if (!from || !to) return;
              auto& src = from->children;
              for (auto it = src.begin(); it != src.end(); )
              {
                UI_lib::Widget* c = it->get();
                if (!c || c->id == "__placeholder") { ++it; continue; }

                auto up = std::move(*it);
                it = src.erase(it);

                up->parent = to;
                to->children.push_back(std::move(up));
              }
            };

          bool changedTree = false;

          // Prune duplicates per slotId, and also cleanup non-desired slots.
          for (auto& [slotId, vec] : slotsById)
          {
            // Decide which instance is canonical.
            UI_lib::DropSlot* keep = nullptr;

            if (auto itP = primary.find(slotId); itP != primary.end())
              keep = itP->second;
            else if (!vec.empty())
              keep = vec.front(); // canonical even if not desired (prevents duplicates later)

            // Kill all other instances of the same id.
            for (UI_lib::DropSlot* ds : vec)
            {
              if (!ds || ds == keep) continue;
              if (isProtectedSlot(ds)) continue;

              // Rescue mounted citizens etc.
              adoptChildrenExceptPlaceholder(ds, keep);

              // Keep MountingHost index sane if it tracks by id.
              mountingHost.unregisterWidget(ds);

              // Hard delete.
              eraseFromParent(ds);
              changedTree = true;
            }

            // Now handle slots that are no longer desired (tech/law removed them).
            if (keep && !isProtectedSlot(keep))
            {
              const bool desiredNow = (desiredSlotIds.count(slotId) > 0);
              keep->visible = desiredNow;

              // Optional: if you prefer hard delete instead of hide when not desired:
              // if (!desiredNow) { mountingHost.unregisterWidget(keep); eraseFromParent(keep); changedTree = true; }
            }
          }

          // If you did any deletes/creates, rebuild the id index once as a safety net.
          if (changedTree)
            mountingHost.rebuildIdIndex(activeRoot()); // or findAny<Widget>("CityScreen")

        }
      }

      // --- (Q) Populate ViewQueue with available buildings (proportional + aligned cols + hover) ----
      if (auto bl = v.get("availible_biuldings"); bl && bl->asVec())
      {
        auto* scroll = findAny<UI_lib::ScrollView>("ViewQueue");
        if (scroll)
        {
          auto& kids = scroll->children;

          // helpers for unique_ptr children
          auto findChildById = [](UI_lib::Widget* parent, const std::string& cid)
            -> std::pair<int, UI_lib::Widget*> {
            if (!parent) return { -1, nullptr };
            auto& v = parent->children;
            for (int i = 0; i < static_cast<int>(v.size()); ++i) {
              if (v[i] && v[i]->id == cid) return { i, v[i].get() };
            }
            return { -1, nullptr };
            };

          auto bringToTop = [](UI_lib::Widget* parent, int idx) {
            if (!parent || idx < 0 || idx >= static_cast<int>(parent->children.size())) return;
            auto up = std::move(parent->children[idx]);
            parent->children.erase(parent->children.begin() + idx);
            parent->children.push_back(std::move(up));
            };

          auto upsertImageAbs = [&](const std::string& id,
            const std::string& sprite,
            int lx, int ly, int w, int h,
            bool hitTest = false, bool draggable = false) -> UI_lib::Image* {
              Rect local{ float(lx), float(ly), float(w), float(h) };
              Rect abs{
                scroll->rect.x + lx,
                scroll->rect.y + ly,
                w, h
              };
              auto [idx, wptr] = findChildById(scroll, id);
              UI_lib::Image* im = nullptr;
              if (!wptr) {
                auto np = std::make_unique<UI_lib::Image>();
                np->id = id;
                np->spriteName = sprite;
                np->rect = abs;
                np->clip = abs;
                np->local = local;
                np->visible = true;
                np->draggable = draggable;
                np->hitTestSelf = hitTest;
                im = static_cast<UI_lib::Image*>(np.get());
                kids.push_back(std::move(np));
              }
              else {
                im = dynamic_cast<UI_lib::Image*>(wptr);
                if (!im) {
                  scroll->children.erase(scroll->children.begin() + idx);
                  auto np = std::make_unique<UI_lib::Image>();
                  np->id = id;
                  im = static_cast<UI_lib::Image*>(np.get());
                  kids.push_back(std::move(np));
                }
                else {
                  bringToTop(scroll, idx);
                }
                im->spriteName = sprite;
                im->rect = abs;
                im->clip = abs;
                im->local = local;
                im->visible = true;
                im->draggable = draggable;
                im->hitTestSelf = hitTest;
              }
              return im;
            };

          auto upsertLabelAbs = [&](const std::string& id,
            const std::string& text,
            int lx, int ly, int w, int h,
            int fontSize, uint32_t color) -> UI_lib::Label* {
              Rect local{ float(lx), float(ly), float(w), float(h) };
              Rect abs{
                scroll->rect.x + lx,
                scroll->rect.y + ly,
                w, h
              };
              auto [idx, wptr] = findChildById(scroll, id);
              UI_lib::Label* lb = nullptr;
              if (!wptr) {
                auto np = std::make_unique<UI_lib::Label>();
                np->id = id;
                np->text = text.c_str();
                np->rect = abs;
                np->clip = abs;
                np->local = local;
                np->fontSize = fontSize;
                np->color = color;
                np->visible = true;
                np->hitTestSelf = false;
                np->hitTestChildren = false;
                lb = static_cast<UI_lib::Label*>(np.get());
                kids.push_back(std::move(np));
              }
              else {
                lb = dynamic_cast<UI_lib::Label*>(wptr);
                if (!lb) {
                  scroll->children.erase(scroll->children.begin() + idx);
                  auto np = std::make_unique<UI_lib::Label>();
                  np->id = id;
                  lb = static_cast<UI_lib::Label*>(np.get());
                  kids.push_back(std::move(np));
                }
                else {
                  bringToTop(scroll, idx);
                }
                lb->text = text.c_str();
                lb->rect = abs;
                lb->clip = abs;
                lb->local = local;
                lb->fontSize = fontSize;
                lb->color = color;
                lb->visible = true;
                lb->hitTestSelf = false;
                lb->hitTestChildren = false;
              }
              return lb;
            };

          // Optional: A thin, transparent button spanning the row to provide hover tint
          auto upsertRowHover = [&](const std::string& id, int lx, int ly, int w, int h) -> UI_lib::IconButton* {
            Rect local{ float(lx), float(ly), float(w), float(h) };
            Rect abs{
              scroll->rect.x + lx,
              scroll->rect.y + ly,
              w, h
            };
            auto [idx, wptr] = findChildById(scroll, id);
            UI_lib::IconButton* bt = nullptr;
            if (!wptr) {
              auto np = std::make_unique<UI_lib::IconButton>();
              np->id = id;
              np->spriteName = "";       // no visible sprite; rely on skin hover highlight
              np->rect = abs;
              np->clip = abs;
              np->local = local;
              np->visible = true;
              np->hitTestSelf = true;        // let it receive hover to show tint
              np->hitTestChildren = false;
              np->onClick = [id, scopeId, this]() {
                Payload out = Payload::object({
                  {"scopeId",    Payload(scopeId)},
                  {"buildingId", Payload(id)}
                  });
                ctx.hub.post("ui.available_building.clicked", out);
                };

              // np->clipToParent = true; // uncomment if needed by your UI
              bt = static_cast<UI_lib::IconButton*>(np.get());
              kids.push_back(std::move(np));
            }
            else {
              bt = dynamic_cast<UI_lib::IconButton*>(wptr);
              if (!bt) {
                scroll->children.erase(scroll->children.begin() + idx);
                auto np = std::make_unique<UI_lib::IconButton>();
                np->id = id;
                bt = static_cast<UI_lib::IconButton*>(np.get());
                kids.push_back(std::move(np));
              }
              else {
                bringToTop(scroll, idx);
              }
              bt->spriteName = "";
              bt->rect = abs;
              bt->clip = abs;
              bt->local = local;
              bt->visible = true;
              bt->hitTestSelf = true;
              bt->hitTestChildren = false;
              //bt->draggable = false;
              bt->onClick = [id, scopeId, this]() {
                Payload out = Payload::object({
                  {"scopeId",    Payload(scopeId)},
                  {"buildingId", Payload(id)}
                  });
                ctx.hub.post("ui.available_building.clicked", out);
                };
            }
            return bt;
            };

          // ---- Layout: proportional left column; fixed aligned right column ----
          const int imgTargetH = 64;    // constant image height
          const int nameH = 20;
          const int topPad = 8;
          const int botPad = 8;
          const int rowH = topPad + imgTargetH + 4 + nameH + botPad; // ~104
          const int padX = 12;

          // RIGHT COLUMN fixed local X so all rows align regardless of image width.
          // Choose something safely to the right of max expected thumbnail size.
          const int rightColX = 320;    // tweak as desired to align a neat second column

          // track what we saw (for GC)
          std::unordered_set<std::string> seen;

          int idxRow = 0;
          for (const auto& item : bl->asVec()->data)
          {
            auto* rm = item.asMap(); if (!rm) continue;

            const auto c_code = item.get("code");
            const auto c_name = item.get("name");
            const auto c_sprite = item.get("sprite");
            const auto c_cost = item.get("cost");

            // Optional source dimensions (for aspect)
            const auto c_sx = item.get("size_x");
            const auto c_sy = item.get("size_y");

            if (!c_name || !c_name->asStr() ||
              !c_sprite || !c_sprite->asStr() ||
              !c_cost || !c_cost->asInt())
            {
              continue;
            }

            const std::string name = *c_name->asStr();
            const std::string sprite = *c_sprite->asStr();
            const int         cost = *c_cost->asInt();

            // Aspect
            int srcW = (c_sx && c_sx->asInt()) ? *c_sx->asInt() : 64;
            int srcH = (c_sy && c_sy->asInt()) ? *c_sy->asInt() : 64;
            if (srcW <= 0) srcW = 64;
            if (srcH <= 0) srcH = 64;

            const int imgH = imgTargetH;
            const int imgW = std::max(1, (srcW * imgH + srcH / 2) / srcH);

            const int baseY = 8 + idxRow * rowH;
            const std::string prefix =/* "q." +*/ name;

            // --- Row hover background hitbox (placed first so visuals sit on top) ---
            upsertRowHover(prefix /*+ ".row"*/, 0, baseY, std::max(rightColX + 200, padX + imgW + 200), rowH);

            // --- Left column: image + name (proportional) ---
            const int imgX = padX;
            const int imgY = baseY + topPad;
            upsertImageAbs(prefix + ".img", sprite, imgX, imgY, imgW, imgH);

            // Let the name span the entire left column up to the aligned right column.
            // This keeps names from being clipped by narrow thumbnails.
            const int nameX = imgX;
            const int nameY = imgY + imgH + 4;
            const int nameW = std::max(1, rightColX - nameX - padX);  // fill to the right column gutter
            const int nameH_local = nameH;

            upsertLabelAbs(prefix + ".name", name,
              nameX, nameY, nameW, nameH_local,
              18, 0x3A2A12FF);

            // --- Right column: aligned cost + hammer (hammer to the RIGHT of cost) ---
            // Place cost label first, then hammer to the right
            const int costX = rightColX;
            const int costY = baseY + (rowH / 2 - 12);  // roughly centered vertically
            upsertLabelAbs(prefix + ".cost",
              std::to_string(cost),
              costX, costY, 80, 24,
              22, 0x3A2A12FF);

            // Hammer icon just to the RIGHT of the cost label
            // You can tweak +10 spacing and icon size
            const int hammerX = costX + 10 + 8 * static_cast<int>(std::to_string(cost).size());
            const int hammerY = costY - 2;
            upsertImageAbs(prefix + ".hammer", "hammer", hammerX, hammerY, 24, 24);

            // mark widgets for GC
            seen.insert(prefix + ".row");
            seen.insert(prefix + ".img");
            seen.insert(prefix + ".name");
            seen.insert(prefix + ".cost");
            seen.insert(prefix + ".hammer");

            ++idxRow;
          }

          // GC old q.* widgets
          for (int i = static_cast<int>(kids.size()) - 1; i >= 0; --i) {
            auto& up = kids[i];
            if (!up) continue;
            if (up->id.rfind("q.", 0) == 0) {
              if (!seen.count(up->id)) {
                kids.erase(kids.begin() + i);
              }
            }
          }

          // Spacer to extend scroll content height
          const int contentH = 8 + idxRow * rowH + 8;
          const std::string spacerId = "q.__spacer";
          auto [sidx, spacer] = findChildById(scroll, spacerId);
          if (!spacer) {
            auto np = std::make_unique<UI_lib::Label>();
            np->id = spacerId;
            np->text = "";
            Rect local{ 0, float(std::max(0, contentH - 1)), 1, 1 };
            Rect abs{ scroll->rect.x, scroll->rect.y + (int)local.y, 1, 1 };
            np->rect = abs; np->clip = abs; np->local = local;
            np->visible = false;
            kids.push_back(std::move(np));
          }
          else {
            if (auto* lb = dynamic_cast<UI_lib::Label*>(spacer)) {
              Rect local{ 0, float(std::max(0, contentH - 1)), 1, 1 };
              Rect abs{ scroll->rect.x, scroll->rect.y + (int)local.y, 1, 1 };
              lb->rect = abs; lb->clip = abs; lb->local = local;
            }
          }
        }
      }

      // --- (B) Mount/move citizens to requested slots + rich tooltips ---
      std::unordered_map<std::string, std::pair<std::string, std::string>> desired; // entityKey -> (slotKey, sprite)

      if (auto arr = v.get("assignments"); arr && arr->asVec())
      {
        // ---- helpers (local to this block) ----
        auto getStr = [](const Payload& node, const char* key) -> std::string {
          if (!node.asMap()) return {};
          if (auto v = node.get(key); v && v->asStr()) return *v->asStr();
          return {};
          };

        auto appendKV = [](UI_lib::Widget* w,
          const char* icon,
          const std::string& label,
          const std::string& value,
          uint32_t color = 0x3A2A12FF,
          int fontSize = 16)
          {
            if (!w) return;
            w->hasRichTip = true;
            UI_lib::RichTipRow row;
            // Simulated two-column look: "Label: Value"
            row.icon = icon ? icon : "";
            row.text = label + ": " + (value.empty() ? "—" : value);
            row.color = color;
            row.fontSize = fontSize;
            w->richTip.rows.push_back(std::move(row));
          };

        auto buildCitizenTip = [&](UI_lib::Widget* w, const Payload& rm, const std::string& fallbackTitle)
          {
            if (!w) return;

            const std::string name = getStr(rm, "name");
            const std::string nat = getStr(rm, "nationality");
            const std::string rel = getStr(rm, "religion");
            const std::string profS = getStr(rm, "professionStr"); // optional; will arrive as string later
            const std::string jobS = getStr(rm, "jobStr");         // optional; will arrive as string later
            const std::string status = getStr(rm, "status");

            w->hasRichTip = true;
            w->richTip.title = (name.empty() ? fallbackTitle.c_str() : name.c_str());
            w->richTip.titleSize = 18;
            w->richTip.titleColor = 0x3A2A12FF;
            w->richTip.rows.clear();

            // Pick icons you like from your atlas
            appendKV(w, "demography icon", "Name", name);
            appendKV(w, "economy icon", "Nationality", nat);
            appendKV(w, "law icon", "Religion", rel);
            appendKV(w, "tech icon", "Profession", profS);
            appendKV(w, "building_button", "Job", jobS);
            appendKV(w, "questions_icon", "Status", status);
          };
        // ---------------------------------------

        for (const auto& row : arr->asVec()->data)
        {
          auto* rm = row.asMap(); if (!rm) continue;

          auto ek = row.get("entityKey");
          auto jb = row.get("job");
          if (!ek || !ek->asStr() || !jb || !jb->asStr()) continue;

          const std::string entityKey = *ek->asStr();
          const std::string slotKey = *jb->asStr();

          // sprite selection (same as your original)
          std::string sprite = "citizen_generic";
          if (auto spr = row.get("sprite"); spr && spr->asStr())
            sprite = *spr->asStr();
          else if (auto prof = row.get("profession"); prof && prof->asStr())
            sprite = "citizen_" + *prof->asStr();

          const std::string kk = mountingHost.mkKey(scopeId, entityKey);
          UI_lib::Image* mountedImg = nullptr;

          if (auto it = mountingHost.mounts.find(kk); it == mountingHost.mounts.end())
          {
            // create
            mountingHost.mountSprite(ctx, scopeId, slotKey, entityKey, sprite, Rect{ 0,0,65,140 }); // keep your default rect
            if (auto it2 = mountingHost.mounts.find(kk); it2 != mountingHost.mounts.end())
              mountedImg = dynamic_cast<UI_lib::Image*>(it2->second);
            // build tooltip on create
            if (mountedImg) buildCitizenTip(mountedImg, row, entityKey);
          }
          else
          {
            // move / retint as needed
            UI_lib::Widget* w = it->second;
            const std::string wantSlotId = /*"slot." +*/ slotKey;
            if (!w->parent || w->parent->id != wantSlotId)
              mountingHost.moveMount(ctx, scopeId, slotKey, entityKey);

            if (auto* img = dynamic_cast<UI_lib::Image*>(w)) {
              if (img->spriteName != sprite)
                mountingHost.retintMount(ctx, scopeId, entityKey, sprite);
              mountedImg = img;
            }

            // rebuild tooltip on update (name/status/etc. might change)
            if (mountedImg) buildCitizenTip(mountedImg, row, entityKey);
          }

          desired[entityKey] = { slotKey, sprite };
        }
      }

      // --- (C) Garbage-collect mounts in this scope not present anymore ---
      std::vector<std::string> drop;
      for (auto& [k, w] : mountingHost.mounts)
      {
        if (k.rfind(scopeId + "|", 0) != 0) continue; // other scopes
        const std::string entityKey = k.substr(scopeId.size() + 1);
        if (!desired.count(entityKey))
          drop.push_back(entityKey);
      }
      for (auto& ek : drop)
        mountingHost.unmount(ctx, scopeId, ek);

      // --- (D) Empty-slot placeholder sprites -------------------
      {
        // Build occupied slot set from current desired map (entityKey -> (slotKey, sprite))
        std::unordered_set<std::string> occupiedSlots;
        occupiedSlots.reserve(desired.size());
        for (const auto& [ek, pair] : desired)
          occupiedSlots.insert(pair.first); // slotKey

        auto findChildImageW = [](UI_lib::Widget* parent, const std::string& cid) -> UI_lib::Image*
          {
            if (!parent) return nullptr;
            for (std::unique_ptr<UI_lib::Widget>& ch : parent->children)
            {
              if (!ch || ch->id != cid) continue;
              if (auto* im = dynamic_cast<UI_lib::Image*>(ch.get()))
                return im;
            }
            return nullptr;
          };

        auto ensurePlaceholder = [&](UI_lib::Widget* host, bool show)
          {
            if (!host) return;

            // Prefer a stable child id inside each slot host.
            constexpr const char* kPhId = "__placeholder";

            UI_lib::Image* ph = findChildImageW(host, kPhId);
            if (!ph)
            {
              auto np = std::make_unique<UI_lib::Image>();
              np->id = kPhId;
              np->spriteName = "placeholder"; // <-- citizens atlas
              np->tooltip = "";
              np->draggable = false;
              np->hitTestSelf = false;            // IMPORTANT: don't block DropSlot interactions

              // Fill host rect (abs + local). This keeps it robust if you resize later.
              Rect abs{ host->rect.x, host->rect.y, host->rect.w, host->rect.h };
              np->rect = abs;
              np->clip = abs;
              np->local = Rect{ 0, 0, host->rect.w, host->rect.h };

              // Insert at front so any mounted citizen sprite stays above it.
              host->children.insert(host->children.begin(), std::move(np));
              ph = static_cast<UI_lib::Image*>(host->children.front().get());
            }
            else
            {
              // Keep geometry synced (in case layout moved since last frame)
              Rect abs{ host->rect.x, host->rect.y, host->rect.w, host->rect.h };
              ph->rect = abs;
              ph->clip = abs;
              ph->local = Rect{ 0, 0, host->rect.w, host->rect.h };
              ph->spriteName = "placeholder";
              ph->draggable = false;
              ph->hitTestSelf = false;
            }

            ph->visible = show;
          };

        // Walk CityScreen and process all DropSlots
        std::function<void(UI_lib::Widget*)> walk = [&](UI_lib::Widget* w)
          {
            if (!w) return;

            if (auto* slot = dynamic_cast<UI_lib::DropSlot*>(w))
            {
              const std::string slotKey = slot->id;
              const bool occupied = (occupiedSlots.find(slotKey) != occupiedSlots.end());

              // If a mount-host exists, attach placeholder there (same parent as mounted citizens).
              UI_lib::Widget* host = findAny<UI_lib::Widget>("slot." + slotKey);

              // If not, create the mount-host once (with empty displayName so we don't touch captions).
              if (!host)
              {
                mountingHost.ensureSlot(ctx, scopeId, slotKey, "");
                host = findAny<UI_lib::Widget>("slot." + slotKey);
              }

              // Fallback: if your ensureSlot doesn't create a wrapper for some reason, attach to DropSlot itself.
              if (!host) host = slot;

              ensurePlaceholder(host, /*show=*/!occupied);
            }

            for (auto& ch : w->children)
              walk(ch.get());
          };

        if (auto* city = findAny<UI_lib::Widget>("CityScreen"))
          walk(city);
      }

      // --- (TY) Tile yields overlay (icons above DropSlots) --------------------------
      if (auto ty = v.get("tileYields"); ty && ty->asVec())
      {
        auto* overlay = findAny<UI_lib::Panel>("TilesOverlay");
        if (overlay)
        {
          auto& kids = overlay->children;

          // ---- helpers (local to this block) ----
          auto findChildById = [](UI_lib::Widget* parent, const std::string& cid)
            -> std::pair<int, UI_lib::Widget*> {
            if (!parent) return { -1, nullptr };
            auto& v = parent->children;
            for (int i = 0; i < static_cast<int>(v.size()); ++i)
              if (v[i] && v[i]->id == cid) return { i, v[i].get() };
            return { -1, nullptr };
            };

          auto upsertTinyIcon = [&](const std::string& id,
            const std::string& sprite,
            float lx, float ly, float w, float h) -> UI_lib::Image*
            {
              Rect local{ lx, ly, w, h };
              Rect abs{
                overlay->rect.x + lx,
                overlay->rect.y + ly,
                w, h
              };

              auto [idx, wptr] = findChildById(overlay, id);
              UI_lib::Image* im = nullptr;

              if (!wptr)
              {
                auto np = std::make_unique<UI_lib::Image>();
                np->id = id;
                np->spriteName = sprite;
                np->rect = abs;
                np->clip = abs;
                np->local = local;
                np->visible = true;
                np->draggable = false;
                np->hitTestSelf = false;
                np->hitTestChildren = false;
                np->tooltip = "";
                im = static_cast<UI_lib::Image*>(np.get());
                kids.push_back(std::move(np));
              }
              else
              {
                im = dynamic_cast<UI_lib::Image*>(wptr);
                if (!im)
                {
                  // replace if some other widget hijacked the id
                  overlay->children.erase(overlay->children.begin() + idx);
                  auto np = std::make_unique<UI_lib::Image>();
                  np->id = id;
                  im = static_cast<UI_lib::Image*>(np.get());
                  kids.push_back(std::move(np));
                }

                im->spriteName = sprite;
                im->rect = abs;
                im->clip = abs;
                im->local = local;
                im->visible = true;
                im->draggable = false;
                im->hitTestSelf = false;
                im->hitTestChildren = false;
                im->tooltip = "";
              }
              return im;
            };

          auto centerForTileIndex = [&](int idx, float& outCx, float& outCy) -> bool
            {
              // Special case: city center (no DropSlot)
              if (idx == -1)
              {
                outCx = 248.0f;
                outCy = 248.0f;
                return true;
              }

              std::string slotId;
              if (idx >= 0 && idx <= 5)
                slotId = "tile.ring1.slot" + std::to_string(idx);
              else if (idx >= 6 && idx <= 17)
                slotId = "tile.ring2.slot" + std::to_string(idx - 6);
              else
                return false;

              auto* slot = findAny<UI_lib::Widget>(slotId);
              if (!slot) return false;

              // Prefer local if it’s meaningful (YAML rects usually populate local)
              float lx = slot->local.x;
              float ly = slot->local.y;
              float lw = slot->local.w;
              float lh = slot->local.h;

              // Fallback if local is not set:
              if (lw <= 0 || lh <= 0)
              {
                lx = slot->rect.x - overlay->rect.x;
                ly = slot->rect.y - overlay->rect.y;
                lw = slot->rect.w;
                lh = slot->rect.h;
              }

              outCx = lx + lw * 0.5f;
              outCy = ly + lh * 0.5f;
              return true;
            };
          // --------------------------------------

          // Track ids we produced this frame (for GC)
          std::unordered_set<std::string> seen;

          // Visual constants (tweak to taste)
          constexpr float kIcon = 20.0f;     // tiny yield icon size
          constexpr float kGap = 1.0f;       // gap between icons in a row
          constexpr float kRowGap = 2.0f;    // gap between rows

          for (const auto& entry : ty->asVec()->data)
          {
            auto* em = entry.asMap();
            if (!em) continue;

            auto ix = entry.get("index");
            auto ys = entry.get("yields");
            if (!ix || !ix->asInt() || !ys || !ys->asVec())
              continue;

            const int tileIndex = static_cast<int>(*ix->asInt());

            float cx = 0, cy = 0;
            if (!centerForTileIndex(tileIndex, cx, cy))
              continue;

            // Parse yields: payload format is array of objects with 1 kv each: { "wheat": 3 }
            std::vector<std::pair<std::string, int>> rows;
            rows.reserve(4);

            for (const auto& y : ys->asVec()->data)
            {
              auto* ym = y.asMap();
              if (!ym) continue;

              // We assume map stores key/value pairs in `data`
              // (common in your codebase where Vec uses `.data` too).
              if (ym->data.empty()) continue;

              for (const auto& kv : ym->data)
              {
                const std::string& sprite = kv.first;
                const UI_lib::Payload& qn = kv.second;

                if (!qn.asInt()) continue;
                int qty = static_cast<int>(*qn.asInt());
                if (qty <= 0) continue;

                rows.push_back({ sprite, qty });
                break; // one key per object
              }
            }

            if (rows.empty()) continue;

            // Total block height, centered on (cx,cy)
            const float totalH = rows.size() * kIcon + (rows.size() - 1) * kRowGap;
            float y0 = cy - totalH * 0.5f;

            // Draw each yield type as its own centered row
            for (int r = 0; r < static_cast<int>(rows.size()); ++r)
            {
              const std::string& sprite = rows[r].first;
              const int qty = rows[r].second;

              const float rowW = qty * kIcon + (qty - 1) * kGap;
              float x0 = cx - rowW * 0.5f;

              for (int c = 0; c < qty; ++c)
              {
                const float lx = x0 + c * (kIcon + kGap);
                const float ly = y0 + r * (kIcon + kRowGap);

                // Stable id for GC; include tileIndex so it’s unique per tile
                const std::string id =
                  "ty." + std::to_string(tileIndex) + "." + sprite + "." + std::to_string(r) + "." + std::to_string(c);

                upsertTinyIcon(id, sprite, lx, ly, kIcon, kIcon);
                seen.insert(id);
              }
            }
          }

          // GC: remove old yield icons that aren’t produced anymore
          for (int i = static_cast<int>(kids.size()) - 1; i >= 0; --i)
          {
            auto& up = kids[i];
            if (!up) continue;
            if (up->id.rfind("ty.", 0) == 0)
            {
              if (!seen.count(up->id))
                kids.erase(kids.begin() + i);
            }
          }
        }
      }

      ctx.invalidate();
      });

    auto primeBind = [&](auto&& self, Widget* w) -> void {
      if (!w) return;

      // 1) Wire channels to the widget properties (generic)
      w->trans.bindAll(w->animV);

      // 2) Apply preset + per-widget overrides to set the "closed pose"
      applyPresetTo(*w);

      // 3) Prime to visible/hidden without popping
      if (w->visible) w->trans.jumpOpen();
      else            w->trans.jumpClosed();

      // Keep animV in sync on frame 0
      w->animV.opacity = w->trans.opacity;
      w->animV.scale = w->trans.scale;
      w->animV.offset = w->trans.offset;
      w->animV.color = w->trans.color;

      // 4) Wire signals (external control)
      if (!w->animCfg.openSignal.empty()) {
        ctx.hub.subscribe(w->animCfg.openSignal, [w, this](const Payload&) { w->open(); ctx.invalidate(); });
      }
      if (!w->animCfg.closeSignal.empty()) {
        ctx.hub.subscribe(w->animCfg.closeSignal, [w, this](const Payload&) { w->close(); ctx.invalidate(); });
      }

      for (auto& c : w->children) self(self, c.get());
    };
    primeBind(primeBind, activeRoot());

    bus.reserve(50'000, 80'000, 4'096); // tune for your HUD scale !!! @todo
  }

  //------------------------------------------------------
  bool UiSystem::route(const UIEvent& e)
  {
    // Always keep cursor in sync (even if modal consumes the event).
    trackPointer(ctx, e);

    // 1) If it's keyboard-like, deliver to current focus first.
    if ((e.kind == UIEvent::Kind::Key) && ctx.root && ctx.root->focusWidget) {
      Widget* f = ctx.root->focusWidget;
      // If a modal is up, only honor focus if it lives under the modal
      if (!modal || !modal->active || UiRoot::isDescendant(modal->popup.get(), f)) {
        UIEvent ev = e;
        if (f->onEvent(ctx, ev)) return true;
      }
    }

    // 2) if a modal is up, first deliver to the globally captured widget
    // when that widget lives under the modal popup.
    if (modal && modal->active) {
      if (e.kind == UIEvent::Kind::Pointer && ctx.root && ctx.root->pointerCapture) {
        Widget* cap = ctx.root->pointerCapture;
        if (cap && UiRoot::isDescendant(modal->popup.get(), cap)) {
          UIEvent ev = e;
          if (cap->onEvent(ctx, ev)) return true;     // consumed by captured widget
        }
      }
      // Otherwise, let the modal do its local routing
      if (modal->onEvent(ctx, e)) return true;
    }

    // Normal routing
    const bool consumed = root.route(ctx, e);
    dirty = dirty || consumed;
    return consumed;
  }

  //------------------------------------------------------------------------------------------
  bool UiSystem::update(float dt)
  {
    ctx.hub.pump();
    updateTooltip(ctx, dt);
    ctx.updateRichTooltip(dt);

    // --- hot reload check ---
    if (m_hotReload)
    {
      for (auto& w : m_watched)
      {
        try {
          if (std::filesystem::exists(w.path))
          {
            auto t = std::filesystem::last_write_time(w.path);
            if (w.lastWrite != t)
            {
              w.lastWrite = t;
              fprintf(stderr, "[UI] hot-reload: %s\n", w.path.c_str());
              reloadView(w.viewId, w.path);
            }
          }
        }
        catch (...) { /* ignore transient file write errors */ }
      }
    }

    bool subtreeAnimating = false;
    for (auto* r : root.contents) if (r && r->visible) subtreeAnimating |= updateTree(r, ctx, dt);

    // global animator for tooltips, etc. ???
    const bool globalAnimating = ctx.animator ? ctx.animator->update(dt) : false;

    const bool animating = subtreeAnimating || globalAnimating;
    dirty = dirty || animating || ctx.consumeInvalidated();  // consider theme/binding invalidations too
    return dirty;
  }

  //-----------------------------------------------------------------------------------------------
  void UiSystem::build()
  {
    if (!dirty && !ctx.consumeInvalidated()) return;

    DrawList& back = bus.beginWrite();
    back.clear();
    ctx.dl = &back;

    ctx.prim->setTarget(&back);
    ctx.prim->pushClip(ctx.viewport);
    root.draw(ctx);

    if (modal) {
      modal->layout(ctx, ctx.viewport);
      modal->draw(ctx);
    }

      // -- - DEBUG BOUNDS FOR CLUSTERS-- -
      //if (ctx.debugLayout && root.content) {
      //  auto* L = findWidgetById<Widget>(root.content.get(), "leftCluster");
      //  auto* R = findWidgetById<Widget>(root.content.get(), "rightCluster");

      //  // stroke color helpers
      //  constexpr uint32_t RED = 0xFF0000FF;
      //  constexpr uint32_t GREEN = 0x00FF00FF;

      //  if (L) {
      //    // 1px device outline on the *visual* rect so tween offset/scale is visible
      //    drawOutlineDevicePx(*ctx.prim, *ctx.dl, L->visualRect(), RED, L->clip, ctx);
      //    // translucent fill to see overlap (optional)
      //    ctx.prim->solidRect(*ctx.dl, { L->visualRect().x, L->visualRect().y, L->visualRect().w, L->visualRect().h },
      //      0xFF000044, L->clip);
      //    // log once
      //    static bool onceL = false;
      //    if (!onceL) {
      //      printf("[DBG] leftCluster rect=(%.1f,%.1f,%.1f,%.1f) clip=(%.1f,%.1f,%.1f,%.1f)\n",
      //        L->rect.x, L->rect.y, L->rect.w, L->rect.h,
      //        L->clip.x, L->clip.y, L->clip.w, L->clip.h);
      //      onceL = true;
      //    }
      //  }
      //  if (R) {
      //    drawOutlineDevicePx(*ctx.prim, *ctx.dl, R->visualRect(), GREEN, R->clip, ctx);
      //    ctx.prim->solidRect(*ctx.dl, { R->visualRect().x, R->visualRect().y, R->visualRect().w, R->visualRect().h },
      //      0x00FF0044, R->clip);
      //    static bool onceR = false;
      //    if (!onceR) {
      //      printf("[DBG] rightCluster rect=(%.1f,%.1f,%.1f,%.1f) clip=(%.1f,%.1f,%.1f,%.1f)\n",
      //        R->rect.x, R->rect.y, R->rect.w, R->rect.h,
      //        R->clip.x, R->clip.y, R->clip.w, R->clip.h);
      //      onceR = true;
      //    }
      //  }
      //}

    ctx.prim->popClip();

    if (ctx.drag.active && ctx.drag.thresholdMet) {
      drawDragGhost(ctx);
    }

    drawOverlay(ctx);

    bus.publish();
    dirty = false;
  }

  //----------------------------------------------------------------------------------------------------
  void UiSystem::drawOverlay(UiContext& ctx)
  {
    // Rich tooltip first
    drawRichTooltip(ctx);

    // 1) Thin tooltip
    if (ctx.tip.visible || ctx.tip.fadingOut || (ctx.tipCfg.enableFade && ctx.tip.alpha > 0.f))
      drawTooltip(ctx);

    // 2) Custom cursor: always last
    const auto& cur = ctx.cursorSkin;
    if (!cur.spriteName.empty())
    {
      if (auto spr = ctx.findSprite(cur.spriteName))
      {
        // base source size (atlas pixels)
        const float iw = spr->uv.w;
        const float ih = spr->uv.h;

        // final scale = DPI auto-scale * user scale
        float s = cur.scale;
        if (cur.autoScaleDPI)
        {
          // same helper you already use for device-pixel outlines
          const float sx = float(ctx.fbW) / float(ctx.virtualW);
          const float sy = float(ctx.fbH) / float(ctx.virtualH);
          s *= std::min(sx, sy);
        }

        // scaled size (rounded to ints for crispness)
        const float dw = std::floor(iw * s + 0.5f);
        const float dh = std::floor(ih * s + 0.5f);

        // scaled hotspot
        const float hx = cur.hotspot.x * s;
        const float hy = cur.hotspot.y * s;

        // snap to whole pixels
        const float dx = std::floor(ctx.cursorPx.x - hx + 0.5f);
        const float dy = std::floor(ctx.cursorPx.y - hy + 0.5f);

        Rect dst{ dx, dy, dw, dh };
        ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, dst, spr->uv, cur.tint, ctx.viewport);
      }
    }
  }

  //--------------------------------------------------------------------------------------------
  void UiSystem::drawTooltip(UiContext& ctx)
  {
    auto& tip = ctx.tip;
    const auto& cfg = ctx.tipCfg;

    if (!ctx.font) return;

    // In fade mode we still want to draw while fading out (alpha > 0)
    if (!cfg.enableFade) {
      if (!tip.visible) return;
    }
    else {
      if (!tip.visible && !tip.fadingOut && tip.alpha <= 0.f) return;
    }

    const Rect& box = tip.box;
    const float fadeA = cfg.enableFade ? std::clamp(tip.alpha, 0.f, 1.f) : 1.f;

    // Helper: apply alpha; premult if pipeline expects premult
    auto tintWithAlpha = [](uint32_t rgba, float a, bool premult) -> uint32_t {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;

      // compose alpha
      float Af = (float)A / 255.0f;
      Af = Af * a; // multiply by fade
      A = (uint32_t)std::round(Af * 255.0f);

      if (premult) {
        // multiply RGB by composed alpha when using premult pipeline
        R = (uint32_t)std::round((float)R * (Af));
        G = (uint32_t)std::round((float)G * (Af));
        B = (uint32_t)std::round((float)B * (Af));
      }
      return (R << 24) | (G << 16) | (B << 8) | A;
    };

    // ---- Background 9-slice ----
    if (auto nine = ctx.findNineWithTex(cfg.nineName); nine.nine)
    {
      // If your 9-slice pipeline is premultiplied, set this to true.
      // (Common when PNGs were baked as premult and you use ONE, ONE_MINUS_SRC_ALPHA)
      constexpr bool kBgPremultPipeline = false; // set true if your NineSlice pipeline is premult

      // Combine theme opacity with fade
      const float bgA = cfg.opacity * fadeA;
      uint32_t bgTint = tintWithAlpha(cfg.bgTint, bgA, kBgPremultPipeline);

      ctx.prim->nineSlice(*ctx.dl, nine.tex, box, *nine.nine, bgTint, box);
    }

    // ---- Text ----
    // Shape again for lines (same params as update)
    const float px = ctx.theme.fontSizes[2];
    auto shaped = shapeTooltip(*ctx.font, tip.text, px, cfg.maxWidth);

    float x = box.x + cfg.padL;
    float y = box.y + cfg.padT;
    const float lineH = ctx.font->lineHeight * px;
    const float ascPx = ctx.font->ascender * px;

    // MSDF pipeline is STRAIGHT alpha → do NOT premultiply RGB
    uint32_t textCol = tintWithAlpha(cfg.textColor, fadeA, /*premult*/ false);

    ctx.prim->pushClip(box);
    for (const auto& L : shaped.lines) {
      float baselineY = y + ascPx;
      ctx.prim->textMSDF(*ctx.dl, *ctx.font, L.text, { x, baselineY - ascPx }, px, textCol, box);
      y += lineH;
    }
    ctx.prim->popClip();
  }

  //-----------------------------------------------------------------------------------------------------
  void UiSystem::drawRichTooltip(UiContext& ctx)
  {
    auto& tip = ctx.rich; const auto& cfg = ctx.richCfg;
    if (!tip.visible && !tip.fadingOut && tip.alpha <= 0.f) return;

    const float fadeA = cfg.enableFade ? std::clamp(tip.alpha, 0.f, 1.f) : 1.f;
    const Rect& box = tip.box;

    // background
    if (auto nl = ctx.findNineWithTex(cfg.nineName); nl.nine) {
      // Straight alpha by default; change if your nine pipeline is premult
      auto tintWithAlpha = [](uint32_t rgba, float a) -> uint32_t {
        a = std::clamp(a, 0.f, 1.f);
        uint32_t A = (rgba) & 0xFFu;
        uint32_t R = (rgba >> 24) & 0xFFu;
        uint32_t G = (rgba >> 16) & 0xFFu;
        uint32_t B = (rgba >> 8) & 0xFFu;
        float Af = float(A) / 255.f * a;
        A = uint32_t(std::round(Af * 255.f));
        return (R << 24) | (G << 16) | (B << 8) | A;
      };
      uint32_t bg = tintWithAlpha(0xFFFFFFFF, fadeA);
      ctx.prim->nineSlice(*ctx.dl, nl.tex, box, *nl.nine, bg, box);
    }

    float x = box.x + cfg.padL;
    float y = box.y + cfg.padT;

    // title
    if (!tip.content.title.empty() && ctx.font) {
      uint32_t c = tip.content.titleColor;
      c = (c & 0xFFFFFF00u) | uint32_t(std::round(float(c & 0xFFu) * fadeA));
      auto s32 = utf8ToUtf32(tip.content.title);
      const float asc = ctx.font->ascender * tip.content.titleSize;
      ctx.prim->textMSDF(*ctx.dl, *ctx.font, s32, { x, y }, tip.content.titleSize, c, box);
      y += ctx.font->lineHeight * tip.content.titleSize + (tip.content.rows.empty() ? 0.f : cfg.gapY);
    }

    // rows
    for (const auto& r : tip.content.rows) {
      float rowH = std::max(cfg.iconPx, ctx.font ? ctx.font->lineHeight * r.fontSize : cfg.iconPx);

      float cx = x;
      // icon
      if (!r.icon.empty()) {
        if (auto spr = ctx.findSprite(r.icon)) {
          const float iw = spr->uv.w, ih = spr->uv.h;
          const float s = std::min(cfg.iconPx / std::max(1.f, ih), cfg.iconPx / std::max(1.f, iw));
          const float dw = std::floor(iw * s + 0.5f);
          const float dh = std::floor(ih * s + 0.5f);
          Rect dst{ std::floor(cx + 0.5f), std::floor(y + (rowH - dh) * 0.5f + 0.5f), dw, dh };
          ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, dst, spr->uv, 0xFFFFFFu | uint32_t(std::round(255.f * fadeA)), box);
          cx += dw + cfg.textGap;
        }
        else {
          cx += cfg.iconPx + cfg.textGap; // reserve space anyway
        }
      }

      // text
      if (ctx.font && !r.text.empty()) {
        uint32_t c = r.color;
        c = (c & 0xFFFFFF00u) | uint32_t(std::round(float(c & 0xFFu) * fadeA));
        const float asc = ctx.font->ascender * r.fontSize;
        ctx.prim->textMSDF(*ctx.dl, *ctx.font, utf8ToUtf32(r.text), { cx, y }, r.fontSize, c, box);
      }

      y += rowH + cfg.rowGap;
    }
  }

  //------------------------------------------------------------------------------
  bool UiSystem::updateTree(Widget* w, UiContext& ctx, float dt)
  {
    if (!w || !w->visible) return false;

    w->update(ctx, dt);
    bool anim = w->isAnimating();

    for (auto& c : w->children)
      anim |= updateTree(c.get(), ctx, dt);

    return anim;
  }

  //----------------------------------------------------------------
  bool UiSystem::anyAnimating(Widget* w) const
  {
    return w->isAnimating();
  }

  //----------------------------------------------------------------
  void UiSystem::openChooseJobPopup(const Rect& anchor, const Payload& data)
  {
    if (!modal) modal = std::make_unique<ModalLayer>();

    std::unique_ptr<PopupPanel> popup = std::make_unique<ChooseJobPopup>();
    popup->id = "ChooseJobPopup";

    // Preload the popup data before onOpen()
    if (auto* cj = dynamic_cast<ChooseJobPopup*>(popup.get())) {
      cj->apply(data);
    }

    // resKey not needed; pass a tag for debugging if you want
    modal->open(std::move(popup), anchor, "choose_job");
    ctx.invalidate();
  }

  //-------------------------------------------------------------------
  void UiSystem::closeChooseJobPopup(bool sendPayload)
  {
    if (modal && modal->active) modal->close(ctx, sendPayload);
  }

  //-------------------------------------------------------------------
  void UiSystem::openGrowthPopup(const Rect& anchor, const Payload& data)
  {
    if (!modal) modal = std::make_unique<ModalLayer>();
    std::unique_ptr<PopupPanel> popup = std::make_unique<GrowthPopup>();
    popup->id = "GrowthPopup";
    // Preload the popup data before onOpen()
    if (auto* cj = dynamic_cast<GrowthPopup*>(popup.get())) {
      cj->apply(data);
    }
    modal->open(std::move(popup), anchor, "growth");
    ctx.invalidate();
  }

  // MountingHost UiSystem.cpp (implementations)
  //-------------------------------------------------------------------
  Widget* MountingHost::mountSprite(UiContext& ctx,
                                    const std::string& scopeId,
                                    const std::string& slotKey,
                                    const std::string& entityKey,
                                    const std::string& spriteName,
                                    const Rect& localRect)
  {
    const std::string slotId = /*"slot." +*/ slotKey;
    Widget* slot = find(slotId);
    if (!slot) { fprintf(stderr, "[UI] slot '%s' not found\n", slotId.c_str()); return nullptr; }

    const std::string k = mkKey(scopeId, entityKey);

    // If exists, just move + retint if needed
    if (auto it = mounts.find(k); it != mounts.end())
    {
      Widget* w = it->second;
      // reparent if needed
      if (w->parent != slot)
      {
        // remove from old
        if (w->parent)
        {
          auto& kids = w->parent->children;
          kids.erase(std::remove_if(kids.begin(), kids.end(),
            [w](const std::unique_ptr<Widget>& p) { return p.get() == w; }),
            kids.end());
        }
        // reattach
        w->parent = slot;
        // take ownership back (w was raw ptr; we need to re-wrap it)
        // Create a holder that doesn't delete twice: we must own it only once.
        // Easiest: we only support move via erase/insert path below, so instead:
      }
      // Update sprite if Image
      if (auto* img = dynamic_cast<Image*>(w)) {
        img->spriteName = spriteName;
      }
      ctx.invalidate();
      return w;
    }

    // Create new Image
    auto img = std::make_unique<Image>();
    img->id = /*"ent:" +*/ entityKey; // local id (not global)
    img->local = localRect; img->rect = localRect;
    img->visible = true; img->spriteName = spriteName;
    img->focusable = true;
    img->tooltip = entityKey;

    Widget* raw = img.get();
    img->parent = slot;
    slot->children.emplace_back(std::move(img));
    mounts[k] = raw;
    registerWidget(raw);
    ctx.invalidate();
    return raw;
  }

  //--------------------------------------------------------------------------------------------------------
  bool MountingHost::moveMount(UiContext& ctx, const std::string& scopeId, const std::string& newSlotKey, const std::string& entityKey)
  {
    const std::string k = mkKey(scopeId, entityKey);
    auto it = mounts.find(k); if (it == mounts.end()) return false;
    Widget* w = it->second;

    const std::string slotId = /*"slot." +*/ newSlotKey;
    Widget* dst = find(slotId); if (!dst) return false;

    // detach from old
    if (w->parent)
    {
      auto& kids = w->parent->children;
      std::unique_ptr<Widget> owned;
      for (auto ki = kids.begin(); ki != kids.end(); ++ki) {
        if (ki->get() == w) { owned = std::move(*ki); kids.erase(ki); break; }
      }
      if (owned)
      {
        w->parent = dst;
        dst->children.emplace_back(std::move(owned));
        ctx.invalidate();
        return true;
      }
    }
    return false;
  }

  //--------------------------------------------------------------------------------------------------------
  bool MountingHost::retintMount(UiContext& ctx, const std::string& scopeId, const std::string& entityKey, const std::string& spriteName)
  {
    const std::string k = mkKey(scopeId, entityKey);
    auto it = mounts.find(k); if (it == mounts.end()) return false;
    if (auto* img = dynamic_cast<Image*>(it->second)) {
      img->spriteName = spriteName;
      ctx.invalidate();
      return true;
    }
    return false;
  }

  //--------------------------------------------------------------------------------------------------------
  void MountingHost::unmount(UiContext& ctx, const std::string& scopeId, const std::string& entityKey)
  {
    const std::string k = mkKey(scopeId, entityKey);
    auto it = mounts.find(k); if (it == mounts.end()) return;

    Widget* w = it->second;
    if (w->parent) {
      auto& kids = w->parent->children;
      unregisterWidget(w);
      kids.erase(std::remove_if(kids.begin(), kids.end(),
        [w](const std::unique_ptr<Widget>& p) { return p.get() == w; }),
        kids.end());
    }
    mounts.erase(it);
    ctx.invalidate();
  }

  //--------------------------------------------------------------------------------------------------------
  void MountingHost::clearScope(UiContext& ctx, const std::string& scopeId)
  {
    // remove all mounts for this scope
    std::vector<std::string> toErase;
    toErase.reserve(mounts.size());
    for (auto& [k, w] : mounts) {
      if (k.rfind(scopeId + "|", 0) == 0) { // starts_with
        if (w && w->parent) {
          auto& kids = w->parent->children;
          kids.erase(std::remove_if(kids.begin(), kids.end(),
            [w](const std::unique_ptr<Widget>& p) { return p.get() == w; }),
            kids.end());
        }
        toErase.push_back(k);
      }
    }
    for (auto& k : toErase) mounts.erase(k);
    ctx.invalidate();
  }

  //---------------------------------------------------------------------------------------------------
  void MountingHost::ensureSlot(UiContext& ctx, const std::string& scopeId, const std::string& slotKey, const std::string& displayName)
  {
    (void)scopeId; // not needed for creation; slot id is global in the tree

    if (!ctx.root || !ctx.root->contents.empty()) return;

    const std::string slotId =/* "slot." +*/ slotKey;
    if (find(slotId)) return; // already exists

    // Choose a container. For now: inside BuildingsPane → BuildingsScroll (first child)
    UI_lib::Widget* buildingsPane = find("BuildingsPane");
    if (!buildingsPane) return;

    UI_lib::Widget* scroll = nullptr;
    for (auto& c : buildingsPane->children) {
      if (c->id == "BuildingsScroll") { scroll = c.get(); break; }
    }
    if (!scroll) return;

    // Build a simple horizontal row: [DropSlot][Label]
    auto row = std::make_unique<UI_lib::FlexRow>();
    row->id = std::string("row.") + slotKey;
    row->local = UI_lib::Rect{ 0,0, 600, 48 };
    row->gap = 8.f;
    row->align = UI_lib::FlexRow::Align::Center;

    auto slot = std::make_unique<UI_lib::DropSlot>();
    slot->id = "slot." + slotKey;
    slot->local = UI_lib::Rect{ 0,0, 48,48 };
    slot->slotKey = slotKey;
    slot->tooltip = "Drop to assign";

    auto label = std::make_unique<UI_lib::Label>();
    label->id = "lbl." + slotKey;
    label->text = displayName.empty() ? slotKey : displayName;
    label->fontSize = 22.f;
    label->color = 0x3A2A12FF;
    label->local = UI_lib::Rect{ 0,0, 520, 28 };

    slot->parent = row.get();
    label->parent = row.get();
    row->children.emplace_back(std::move(slot));
    row->children.emplace_back(std::move(label));

    row->parent = scroll;
    scroll->children.emplace_back(std::move(row));

    ctx.invalidate();
  }

  // View API
  //--------------------------------------------------------------
  UiView* UiSystem::findView(const std::string& name) {
    for (auto& v : m_views) if (v.name == name) return &v;
    return nullptr;
  }

  //--------------------------------------------------------------
  UiView& UiSystem::loadView(const std::string& name,
    const std::string& yamlPath,
    int layer,
    bool isScreen)
  {
    if (auto* v = findView(name)) return *v; // already loaded

    UiView v;
    v.name = name;
    v.yamlPath = yamlPath;
    v.layer = layer;
    v.isScreen = isScreen;

    v.root = loadLayout(yamlPath);
    // Ownership stays here (UiSystem). We only pass raw pointers to UiRoot.

    // Wire bindings and prime transitions once
    wireBindings(ctx, v.root.get());
    auto primeBind = [&](auto&& self, Widget* w) -> void {
      if (!w) return;
      w->trans.bindAll(w->animV);
      applyPresetTo(*w);
      if (w->visible) w->trans.jumpOpen(); else w->trans.jumpClosed();
      w->animV.opacity = w->trans.opacity;
      w->animV.scale = w->trans.scale;
      w->animV.offset = w->trans.offset;
      w->animV.color = w->trans.color;
      if (!w->animCfg.openSignal.empty())  ctx.hub.subscribe(w->animCfg.openSignal, [w, this](const Payload&) { w->open();  ctx.invalidate(); });
      if (!w->animCfg.closeSignal.empty()) ctx.hub.subscribe(w->animCfg.closeSignal, [w, this](const Payload&) { w->close(); ctx.invalidate(); });
      for (auto& c : w->children) self(self, c.get());
    };
    primeBind(primeBind, v.root.get());

    m_views.emplace_back(std::move(v));
    rebuildUiRootContents();
    return *findView(name);
  }

  //---------------------------------------------------------------------------------
  void UiSystem::showView(const std::string& name)
  {
    if (auto* v = findView(name)) {
      v->visible = true;
      if (v->root) v->root->open();
      rebuildUiRootContents();
      ctx.invalidate();
    }
  }

  //------------------------------------------------------------------------------------
  void UiSystem::hideView(const std::string& name)
  {
    if (auto* v = findView(name)) {
      if (v->root) v->root->close();
      v->visible = false;
      rebuildUiRootContents();
      ctx.invalidate();
    }
  }

  //-------------------------------------------------------------------------------------
  void UiSystem::toggleView(const std::string& name)
  {
    if (auto* v = findView(name)) {
      if (v->visible) hideView(name); else showView(name);
    }
  }

  //-------------------------------------------------------------------------------------
  void UiSystem::switchTo(const std::string& screenName)
  {
    // hide current screen, show the new one
    for (auto& v : m_views) {
      if (v.isScreen && v.visible && v.name != screenName) {
        if (v.root) v.root->close();
        v.visible = false;
      }
    }
    showView(screenName);
    m_activeScreen = screenName;
  }

  //------------------------------------------------------------------------------------
  Widget* UiSystem::viewRoot(const std::string& name)
  {
    if (auto* v = findView(name)) return v->root.get();
    return nullptr;
  }

  //-------------------------------------------------------------------------------------
  Widget* UiSystem::activeRoot() const
  {
    // top-most visible screen by layer; fallback: any visible
    const UiView* best = nullptr;
    for (auto& v : m_views) {
      if (!v.visible) continue;
      if (!v.isScreen) continue;
      if (!best || v.layer > best->layer) best = &v;
    }
    if (!best) {
      for (auto& v : m_views) if (v.visible) { best = &v; break; }
    }
    return best ? best->root.get() : nullptr;
  }

  //----------------------------------------------------------------------------------------
  void UiSystem::rebuildUiRootContents()
  {
    // Rebuild UiRoot’s non-owning list in bottom→top order
    std::vector<UiView*> z;
    z.reserve(m_views.size());
    for (auto& v : m_views) if (v.visible && v.root) z.push_back(&v);
    std::sort(z.begin(), z.end(), [](auto a, auto b) { return a->layer < b->layer; });

    root.contents.clear();
    for (auto* v : z) root.contents.push_back(v->root.get());
  }

  //--------------------------------------------------------------------------------------------
  // Hot Reload helpers
  namespace {
  inline UiView* findViewByName(std::vector<UiView>& views, const std::string& name) {
    for (auto& v : views) if (v.name == name) return &v;
    return nullptr;
  }
  inline const UiView* findViewByName(const std::vector<UiView>& views, const std::string& name) {
    for (auto& v : views) if (v.name == name) return &v;
    return nullptr;
  }

  static UI_lib::Widget* firstFocusable(UI_lib::Widget* n) {
    if (!n) return nullptr;
    if (n->wantsFocus()) return n;
    for (auto& c : n->children) if (auto* f = firstFocusable(c.get())) return f;
    return nullptr;
  }
  static bool inSubtree(const UI_lib::Widget* root, const UI_lib::Widget* w) {
    for (auto* p = w; p; p = p->parent) if (p == root) return true;
    return false;
  }

  // Same priming pass you already do during initialize()
  //--------------------------------------------------------------------------------
  static void primeTree(UI_lib::UiContext& ctx, UI_lib::Widget* node)
  {
    if (!node) return;
    node->trans.bindAll(node->animV);
    applyPresetTo(*node);
    if (node->visible) node->trans.jumpOpen(); else node->trans.jumpClosed();
    node->animV.opacity = node->trans.opacity;
    node->animV.scale = node->trans.scale;
    node->animV.offset = node->trans.offset;
    node->animV.color = node->trans.color;
    for (auto& c : node->children) primeTree(ctx, c.get());
  }
}

  //-----------------------------------------------------------------------------------
  static bool isInSubtree(const UI_lib::Widget* root, const UI_lib::Widget* w) {
    for (auto* p = w; p; p = p->parent) if (p == root) return true;
    return false;
  }

  //-----------------------------------------------------------------------------------
  void UiSystem::reloadView(const std::string& viewName, const std::string& overrideYaml /*=""*/)
  {
    // 1) find view
    UiView* v = nullptr;
    for (auto& it : m_views) if (it.name == viewName) { v = &it; break; }
    if (!v) { fprintf(stderr, "[UI] reloadView: unknown view '%s'\n", viewName.c_str()); return; }

    const std::string path = overrideYaml.empty() ? v->yamlPath : overrideYaml;
    if (path.empty()) { fprintf(stderr, "[UI] reloadView: no yamlPath for '%s'\n", viewName.c_str()); return; }

    // 2) remember the old raw ptr (to fix UiRoot::contents)
    UI_lib::Widget* oldRoot = v->root.get();

    // 3) load new tree
    auto newRoot = loadLayout(path);
    if (!newRoot) { fprintf(stderr, "[UI] reloadView: failed to load '%s'\n", path.c_str()); return; }

    // 4) release pointer capture/hover/focus if they belong to the old subtree
    if (ctx.root) {
      auto& r = *ctx.root;
      if (r.pointerCapture && isInSubtree(oldRoot, r.pointerCapture)) r.pointerCapture = nullptr;
      if (r.hoverWidget && isInSubtree(oldRoot, r.hoverWidget))    r.hoverWidget = nullptr;
      if (r.focusWidget && isInSubtree(oldRoot, r.focusWidget))    r.focusWidget = nullptr;
      if (ctx.focus.current && isInSubtree(oldRoot, ctx.focus.current)) ctx.focus.current = nullptr;
    }

    // 5) swap trees in the view (ownership)
    v->root = std::move(newRoot);
    v->yamlPath = path;
    UI_lib::Widget* nowRoot = v->root.get();

    // 6) patch UiRoot::contents to point at the new raw pointer
    if (ctx.root) {
      for (auto& p : ctx.root->contents) {
        if (p == oldRoot) { p = nowRoot; break; }
      }
    }

    // 7) rebuild indices/bindings/animation priming on this view only
    mountingHost.rebuildIdIndex(nowRoot);
    wireBindings(ctx, nowRoot);

    auto prime = [&](auto&& self, UI_lib::Widget* node)->void {
      if (!node) return;
      node->trans.bindAll(node->animV);
      applyPresetTo(*node);
      if (node->visible) node->trans.jumpOpen(); else node->trans.jumpClosed();
      node->animV.opacity = node->trans.opacity;
      node->animV.scale = node->trans.scale;
      node->animV.offset = node->trans.offset;
      node->animV.color = node->trans.color;
      for (auto& c : node->children) self(self, c.get());
    };
    prime(prime, nowRoot);

    // 8) if this was the active screen, restore a sane focus
    if (v->isScreen && v->name == m_activeScreen) {
      auto firstFocusable = [&](auto&& self, UI_lib::Widget* n) -> UI_lib::Widget* {
        if (!n) return nullptr;
        if (n->wantsFocus()) return n;
        for (auto& c : n->children) if (auto* f = self(self, c.get())) return f;
        return nullptr;
      };
      if (!ctx.focus.current) {
        if (auto* f = firstFocusable(firstFocusable, nowRoot)) ctx.requestFocus(f);
        else ctx.clearFocus();
      }
    }

    ctx.invalidate();
    ctx.hub.post("ui.view.reloaded", Payload(viewName));
  }

  //-----------------------------------------------------------------------
  void UiSystem::reloadActiveView()
  {
    if (m_activeScreen.empty()) return;
    UiView* v = findViewByName(m_views, m_activeScreen);
    if (!v || v->yamlPath.empty()) return;
    reloadView(m_activeScreen, v->yamlPath);
  }
}

