#include "stdafx.h"

#include "WidgetFactory.h"
#include "LegacyGuiAdapter.h"
#include "ThemeLoader.h"
#include "ParseYamlHelper.h"
#include "RichTip.h"

namespace UI_lib
{
  std::unordered_map<std::string, WidgetFactory> g_factory;   // single definition

  uint32_t parseColor(const YAML::Node& n) { return n.as<uint32_t>(); }

  Rect parseRect(const YAML::Node& n) {
    Rect r; r.x = n["x"].as<float>(); r.y = n["y"].as<float>();
    r.w = n["w"].as<float>(); r.h = n["h"].as<float>(); return r;
  }

  //uint32_t parseColor(const YAML::Node& n)
  //{
  //  if (n.IsScalar()) {
  //    // Handles 0xRRGGBBAA or integer
  //    const std::string s = n.as<std::string>();
  //    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
  //      return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  //    return n.as<uint32_t>();
  //  }
  //  // { r: , g: , b: , a: } also supported
  //  auto r = n["r"].as<int>(); auto g = n["g"].as<int>();
  //  auto b = n["b"].as<int>(); auto a = n["a"].as<int>(255);
  //  return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a); // 0xRRGGBBAA
  //}

  static inline tween::Ease parseEase(const YAML::Node& n, tween::Ease def = tween::Ease::CubicOut) {
    if (!n || !n.IsScalar()) return def;
    std::string s = n.as<std::string>(); std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "linear") return tween::Ease::Linear;
    if (s == "quadIn") return tween::Ease::QuadIn;    if (s == "quadin") return tween::Ease::QuadIn;
    if (s == "quadout") return tween::Ease::QuadOut;  if (s == "quadinout") return tween::Ease::QuadInOut;
    if (s == "cubicin") return tween::Ease::CubicIn;  if (s == "cubicout") return tween::Ease::CubicOut;
    if (s == "cubicinout") return tween::Ease::CubicInOut;
    if (s == "backout") return tween::Ease::BackOut;
    if (s == "sinein") return tween::Ease::SineIn;    if (s == "sineout") return tween::Ease::SineOut;  if (s == "sineinout") return tween::Ease::SineInOut;
    if (s == "exoin" || s == "expoin") return tween::Ease::ExpoIn;
    if (s == "exoout" || s == "expoout") return tween::Ease::ExpoOut;
    if (s == "expoinout") return tween::Ease::ExpoInOut;
    if (s == "bounceout") return tween::Ease::BounceOut;
    if (s == "bouncein")  return tween::Ease::BounceIn;
    if (s == "bounceinout") return tween::Ease::BounceInOut;
    return def;
  }

  static inline Widget::AnimCfg::Preset parsePreset(const YAML::Node& n) {
    if (!n || !n.IsScalar()) return Widget::AnimCfg::Preset::None;
    std::string s = n.as<std::string>(); std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "popup")      return Widget::AnimCfg::Preset::Popup;
    if (s == "panelleft")  return Widget::AnimCfg::Preset::PanelLeft;
    if (s == "panelright") return Widget::AnimCfg::Preset::PanelRight;
    if (s == "toasttop")   return Widget::AnimCfg::Preset::ToastTop;
    if (s == "toastbottom")return Widget::AnimCfg::Preset::ToastBottom;
    return Widget::AnimCfg::Preset::None;
  }

  static inline void parseRichTip(const YAML::Node& n, RichTipContent& out) {
    if (!n) return;
    if (auto t = n["title"])      out.title = t.as<std::string>();
    if (auto ts = n["titleSize"]) out.titleSize = ts.as<float>();
    if (auto tc = n["titleColor"]) out.titleColor = parseColorNode(tc);

    if (auto rows = n["rows"]) {
      for (const auto& r : rows) {
        RichTipRow row;
        if (auto ic = r["icon"]) row.icon = ic.as<std::string>();
        if (auto tx = r["text"]) row.text = tx.as<std::string>();
        if (auto fs = r["fontSize"]) row.fontSize = fs.as<float>();
        if (auto co = r["color"]) row.color = parseColorNode(co);
        out.rows.push_back(std::move(row));
      }
    }
  }

  static void parseWidgetCommons(const YAML::Node& n, UI_lib::Widget& w)
  {
    if (n["id"])     w.id = n["id"].as<std::string>();
    if (n["rect"]) { w.local = parseRect(n["rect"]); w.rect = w.local; }
    if (n["visible"]) w.visible = n["visible"].as<bool>();
    if (n["fontSize"]) w.inlineStyle.fontSize = n["fontSize"].as<float>();
    if (n["color"])    w.inlineStyle.color = n["color"].as<uint32_t>();
    if (n["tint"])     w.inlineStyle.tint = n["tint"].as<uint32_t>();
    if (n["opacity"])  w.inlineStyle.opacity = std::clamp(n["opacity"].as<float>(), 0.f, 1.f);
    if (n["tooltip"]) w.tooltip = n["tooltip"].as<std::string>();
    if (auto rt = n["richTip"]) { parseRichTip(rt, w.richTip); w.hasRichTip = !w.richTip.empty(); }

    if (n["trans"]) {
      auto t = n["trans"];
      // Signals
      if (t["openSignal"])  w.animCfg.openSignal = t["openSignal"].as<std::string>();
      if (t["closeSignal"]) w.animCfg.closeSignal = t["closeSignal"].as<std::string>();

      // Preset + timings/eases
      if (t["preset"])    w.animCfg.preset = parsePreset(t["preset"]);
      if (t["openMs"])    w.animCfg.openMs = t["openMs"].as<float>();
      if (t["closeMs"])   w.animCfg.closeMs = t["closeMs"].as<float>();
      if (t["easeOpen"])  w.animCfg.easeOpen = parseEase(t["easeOpen"], tween::Ease::CubicOut);
      if (t["easeClose"]) w.animCfg.easeClose = parseEase(t["easeClose"], tween::Ease::CubicIn);

      // Optional start pose overrides
      if (t["startOpacity"]) w.animCfg.startOpacity = std::clamp(t["startOpacity"].as<float>(), 0.f, 1.f);
      if (t["startScale"]) { auto v = parseVec2(t["startScale"]);   w.animCfg.startScale = Vec2{ v.x, v.y }; }
      if (t["startOffset"]) { auto v = parseVec2(t["startOffset"]);  w.animCfg.startOffset = Vec2{ v.x, v.y }; }
      if (t["startColor"]) { auto v = parseVec4(t["startColor"]);   w.animCfg.startColor = Vec4{ v.x, v.y, v.z, v.w }; }

      if (t["hitTestFollowsVisual"]) w.animCfg.hitTestFollowsVisual = t["hitTestFollowsVisual"].as<bool>();
    }

    // parse anchors for every widget
    if (auto a = n["anchors"])
    {
      auto setF = [&](const char* key, bool& has, float& val) {
        if (a[key]) { has = true; val = a[key].as<float>(); }
      };
      setF("left", w.anchors.hasL, w.anchors.left);
      setF("right", w.anchors.hasR, w.anchors.right);
      setF("top", w.anchors.hasT, w.anchors.top);
      setF("bottom", w.anchors.hasB, w.anchors.bottom);
    }
  }

  //---------------------------------------------------------------------------------------
  void registerBuiltins()
  {
    g_factory["Panel"] = [](const YAML::Node& n) {
      auto w = std::make_unique<Panel>();
      parseWidgetCommons(n, *w);
      if (n["nineName"]) static_cast<Panel*>(w.get())->nineName = n["nineName"].as<std::string>();
      if (n["tint"])     w->tint = parseColor(n["tint"]);
      return w;
    };

    g_factory["DropSlot"] = [](const YAML::Node& n) {
      auto w = std::make_unique<DropSlot>();
      parseWidgetCommons(n, *w);
      if (n["nineName"]) static_cast<DropSlot*>(w.get())->nineName = n["nineName"].as<std::string>();
      if (n["tint"])     w->tint = parseColor(n["tint"]);
      return w;
    };

    auto asWrap = [](const YAML::Node& n) {
      auto s = n.as<std::string>();
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      return s == "wrap" ? Label::Wrap::Wrap : Label::Wrap::NoWrap;
    };
    auto asOverflow = [](const YAML::Node& n) {
      auto s = n.as<std::string>();
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      return s == "ellipsis" ? Label::Overflow::Ellipsis : Label::Overflow::Clip;
    };
    auto asVAlign = [](const YAML::Node& n) {
      auto s = n.as<std::string>();
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      if (s == "top") return Label::VAlign::Top;
      if (s == "bottom") return Label::VAlign::Bottom;
      return Label::VAlign::Center;
    };

    auto asHAlign = [](const YAML::Node& n) {
      auto s = n.as<std::string>();
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      if (s == "center") return Label::HAlign::Center;
      if (s == "right")  return Label::HAlign::Right;
      return Label::HAlign::Left;
    };

    g_factory["Label"] = [=](const YAML::Node& n) {
      auto w = std::make_unique<Label>();
      parseWidgetCommons(n, *w);
      if (n["text"])  w->text = n["text"].as<std::string>();
      if (n["fontSize"]) w->fontSize = n["fontSize"].as<float>();
      if (n["color"])    w->color = parseColor(n["color"]);
      if (n["wrap"])     w->wrap = asWrap(n["wrap"]);
      if (n["overflow"]) w->overflow = asOverflow(n["overflow"]);
      if (n["valign"])   w->valign = asVAlign(n["valign"]);
      if (n["halign"])   w->halign = asHAlign(n["halign"]);
      if (n["bindText"]) w->bindText = n["bindText"].as<std::string>();
      return w;
    };

    g_factory["Button"] = [](const YAML::Node& n) {
      auto w = std::make_unique<Button>();
      parseWidgetCommons(n, *w);
      if (n["text"]) static_cast<Button*>(w.get())->text = n["text"].as<std::string>();
      if (n["bindText"]) w->bindText = n["bindText"].as<std::string>();
      w->focusable = true;
      return w;
    };

    g_factory["Image"] = [](const YAML::Node& n) {
      auto w = std::make_unique<Image>();
      parseWidgetCommons(n, *w);
      if (n["spriteName"]) static_cast<Image*>(w.get())->spriteName = n["spriteName"].as<std::string>();
      if (n["tint"]) static_cast<Image*>(w.get())->tint = parseColor(n["tint"]);
      if (n["draggable"]) static_cast<Image*>(w.get())->draggable = n["draggable"].as<bool>();
      return w;
    };

    g_factory["IconButton"] = [](const YAML::Node& n) {
      auto w = std::make_unique<IconButton>();
      parseWidgetCommons(n, *w);
      if (n["spriteName"]) static_cast<IconButton*>(w.get())->spriteName = n["spriteName"].as<std::string>();
      if (n["tintNormal"]) static_cast<IconButton*>(w.get())->tintNormal = parseColor(n["tintNormal"]);
      if (n["tintHover"])  static_cast<IconButton*>(w.get())->tintHover = parseColor(n["tintHover"]);
      if (n["tintDown"])   static_cast<IconButton*>(w.get())->tintDown = parseColor(n["tintDown"]);
      if (n["bindText"])   static_cast<IconButton*>(w.get())->bindText = n["bindText"].as<std::string>();
      w->focusable = true;
      return w;
    };

    g_factory["FlexRow"] = [](const YAML::Node& n) {
      auto w = std::make_unique<FlexRow>();
      parseWidgetCommons(n, *w);
     /* if (n["anchors"]) {
        auto a = n["anchors"];
        if (a["left"]) { w->anchors.hasL = true; w->anchors.left = a["left"].as<float>(); }
        if (a["right"]) { w->anchors.hasR = true; w->anchors.right = a["right"].as<float>(); }
        if (a["top"]) { w->anchors.hasT = true; w->anchors.top = a["top"].as<float>(); }
        if (a["bottom"]) { w->anchors.hasB = true; w->anchors.bottom = a["bottom"].as<float>(); }
      }*/
      if (n["padding"]) {
        auto p = n["padding"]; // can be {l:..,r:..,t:..,b:..} or single float
        if (p.IsScalar()) w->padL = w->padR = w->padT = w->padB = p.as<float>();
        else {
          if (p["l"]) w->padL = p["l"].as<float>(); if (p["r"]) w->padR = p["r"].as<float>();
          if (p["t"]) w->padT = p["t"].as<float>(); if (p["b"]) w->padB = p["b"].as<float>();
        }
      }
      if (n["gap"]) w->gap = n["gap"].as<float>();
      if (n["justify"]) {
        std::string j = n["justify"].as<std::string>();
        if (j == "start") w->justify = FlexRow::Justify::Start;
        else if (j == "center") w->justify = FlexRow::Justify::Center;
        else if (j == "end") w->justify = FlexRow::Justify::End;
        else if (j == "spaceBetween") w->justify = FlexRow::Justify::SpaceBetween;
        else if (j == "spaceEvenly") w->justify = FlexRow::Justify::SpaceEvenly;
      }
      if (n["align"]) {
        std::string a = n["align"].as<std::string>();
        if (a == "top") w->align = FlexRow::Align::Top;
        else if (a == "center") w->align = FlexRow::Align::Center;
        else if (a == "bottom") w->align = FlexRow::Align::Bottom;
      }
      return w;
    };

    g_factory["ScrollView"] = [](const YAML::Node& n) {
      auto w = std::make_unique<ScrollView>();
      parseWidgetCommons(n, *w); // id, rect/local, opacity, tint, tooltip, anchors...

      if (auto p = n["padding"]) {
        if (p.IsScalar()) { w->padL = w->padR = w->padT = w->padB = p.as<float>(); }
        else {
          if (p["l"]) w->padL = p["l"].as<float>();
          if (p["r"]) w->padR = p["r"].as<float>();
          if (p["t"]) w->padT = p["t"].as<float>();
          if (p["b"]) w->padB = p["b"].as<float>();
        }
      }
      if (n["wheelStep"])     w->wheelStep = n["wheelStep"].as<float>();
      if (n["showScrollbar"]) w->showScrollbar = n["showScrollbar"].as<bool>();
      if (n["barColor"])      w->barColor = parseColorNode(n["barColor"]);
      if (n["thumbColor"])    w->thumbColor = parseColorNode(n["thumbColor"]);

      // optional sprite / nine names + tints
      if (n["trackNineName"])   w->trackNineName = n["trackNineName"].as<std::string>();
      if (n["trackSpriteName"]) w->trackSpriteName = n["trackSpriteName"].as<std::string>();
      if (n["thumbNineName"])   w->thumbNineName = n["thumbNineName"].as<std::string>();
      if (n["thumbSpriteName"]) w->thumbSpriteName = n["thumbSpriteName"].as<std::string>();

      if (n["trackTint"])        w->trackTint = parseColorNode(n["trackTint"]);
      if (n["thumbTintNormal"])  w->thumbTintNormal = parseColorNode(n["thumbTintNormal"]);
      if (n["thumbTintHover"])   w->thumbTintHover = parseColorNode(n["thumbTintHover"]);
      if (n["thumbTintActive"])  w->thumbTintActive = parseColorNode(n["thumbTintActive"]);

      return w;
    };

    g_factory["CheckBox"] = [](const YAML::Node& n) {
      auto w = std::make_unique<CheckBox>();
      parseWidgetCommons(n, *w);
      if (n["checked"]) w->checked = n["checked"].as<bool>();
      return w;
    };
    g_factory["LineEdit"] = [](const YAML::Node& n) {
      auto w = std::make_unique<LineEdit>();
      parseWidgetCommons(n, *w);
      if (n["text"]) w->text = n["text"].as<std::string>();
      return w;
    };
  }

  void registerLegacy()
  {
    g_factory["LegacyGUI"] = [](const YAML::Node& n) {
      // Build the underlying legacy GUI with current screen dims (replace with your values)
      const int scW = 1920, scH = 1080; // virtual canvas; swap for runtime values
      Rect r = parseRect(n["rect"]);
      auto base = std::make_shared<GUI>((int)r.x, (int)r.y, (int)r.w, (int)r.h, scW, scH);
      if (auto v = n["takeMouse"]) base->SetTakeMouseEvents(v.as<bool>());
      if (auto v = n["movable"]) base->SetMovable2D(v.as<bool>());
      if (auto v = n["transparent"]) base->SetTransparent(v.as<bool>());
      if (auto v = n["executeOnRelease"]) base->SetExecuteOnRelease(v.as<bool>());

      auto w = std::make_unique<LegacyGuiAdapter>(base);
      if (n["id"]) w->id = n["id"].as<std::string>();
      w->rect = r;
      if (auto s = n["sprite"]) w->spriteName = s.as<std::string>();
      return w;
    };
  }

  std::unique_ptr<Widget> buildWidgetTree(const YAML::Node& n)
  {
    const auto type = n["type"].as<std::string>();
    const auto it = g_factory.find(type);
    if (it == g_factory.end()) return {};

    auto w = it->second(n);
    if (auto kids = n["children"]) {
      for (const auto& child : kids) {
        auto c = buildWidgetTree(child);
        if (c) { c->parent = w.get(); w->children.emplace_back(std::move(c)); }
      }
    }
    return w;
  }

  std::unique_ptr<Widget> loadLayout(const std::string& path)
  {
    try {
      YAML::Node y = YAML::LoadFile(path);
      return buildWidgetTree(y["root"]);
    }
    catch (const YAML::ParserException& e) {
      fprintf(stderr, "YAML parser error: %s\n", e.what()); // has line:col
      return {};
    }
  }

} // namespace UI_lib
