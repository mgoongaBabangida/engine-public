#include "YamlLoader.h"
#include "WidgetFactory.h"  // for parseRect/parseColor if you want to reuse
#include "Context.h"

namespace UI_lib
{
   static inline uint64_t kernKey(uint32_t a, uint32_t b) {
    return (uint64_t(a) << 32) | uint64_t(b);
  }

  void loadTheme(const std::string& path, Theme& t)
  {

    try {
      YAML::Node y = YAML::LoadFile(path);
      
      if (auto n = y["spacing"])   for (size_t i = 0; i < std::min<size_t>(6, n.size()); ++i) t.spacing[i] = n[i].as<float>();
      if (auto n = y["radii"])     for (size_t i = 0; i < std::min<size_t>(4, n.size()); ++i) t.radii[i] = n[i].as<float>();
      if (auto n = y["fontSizes"]) for (size_t i = 0; i < std::min<size_t>(5, n.size()); ++i) t.fontSizes[i] = n[i].as<float>();
      if (auto c = y["colors"]) {
        if (c["parchment_bg"]) t.color_parchment_bg = c["parchment_bg"].as<uint32_t>();
        if (c["ink"])          t.color_ink = c["ink"].as<uint32_t>();
        if (c["gold"])         t.color_gold = c["gold"].as<uint32_t>();
      }
    }
    catch (const YAML::ParserException& e) {
      printf("YAML ParserException in '%s': %s\n", path.c_str(), e.what());
    }
    catch (const YAML::BadConversion& e) {
      printf("YAML BadConversion in '%s': %s\n", path.c_str(), e.what());
    }
  }

  void loadAtlas(const std::string& path, Atlas& a) {
    YAML::Node y = YAML::LoadFile(path);
    if (auto sps = y["sprites"]) {
      for (auto it = sps.begin(); it != sps.end(); ++it) {
        Sprite s;
        auto v = it->second;
        s.uv = parseRect(v["uv"]);
        s.texW = v["texW"].as<int>();
        s.texH = v["texH"].as<int>();
        a.sprites[it->first.as<std::string>()] = s;
      }
    }
    if (auto ns = y["nine"]) {
      for (auto it = ns.begin(); it != ns.end(); ++it) {
        NineSlice n;
        auto v = it->second;
        n.texW = v["texW"].as<int>(); n.texH = v["texH"].as<int>();
        n.uvFull = parseRect(v["uvFull"]);
        n.left = v["left"].as<int>(); n.right = v["right"].as<int>();
        n.top = v["top"].as<int>();   n.bottom = v["bottom"].as<int>();
        a.nine[it->first.as<std::string>()] = n;
      }
    }
  }

  //-----------------------------------------------------------------------------------
  bool loadAtlasYaml(UiContext& ctx, const std::string& atlasName, const std::string& yamlPath)
  {
    Atlas A;
    try {
      loadAtlas(yamlPath, A);          // your existing YAML -> Atlas parser
    }
    catch (...) {
      fprintf(stderr, "[UI] loadAtlas failed: %s\n", yamlPath.c_str());
      return false;
    }

    // Register atlas
    ctx.atlases[atlasName] = std::move(A);
    Atlas& AA = ctx.atlases[atlasName];

    // Build global indices (sprites & nine). Texture will be assigned later.
    for (auto& [name, s] : AA.sprites) {
      ctx.spriteIndex[name] = s;
      ctx.spriteOwner[name] = atlasName;
    }
    for (auto& [name, n] : AA.nine) {
      ctx.nineIndex[name] = n;
      ctx.nineOwner[name] = atlasName;
    }

    printf("[UI] YAML atlas loaded '%s': %zu sprites, %zu nine\n",
      atlasName.c_str(), AA.sprites.size(), AA.nine.size());
    return true;
  }

  bool loadFontMSDF_YAML(const std::string& path, FontMSDF& out, TextureID(*loadTexture)(const std::string& imagePath))
  {
    YAML::Node y = YAML::LoadFile(path);

    const std::string image = y["image"].as<std::string>();
    out.atlasW = y["atlasW"].as<int>();
    out.atlasH = y["atlasH"].as<int>();
    out.emSizePx = y["emSizePx"].as<float>();
    if (y["pxRange"])   out.pxRange = y["pxRange"].as<float>();
    if (y["lineHeight"]) out.lineHeight = y["lineHeight"].as<float>();
    if (y["ascender"])   out.ascender = y["ascender"].as<float>();

    // Load the PNG into GL using your existing loader
    out.texture = loadTexture(image);
    if (!out.texture) return false;

    // Glyphs
    auto glyphs = y["glyphs"];
    out.glyphs.clear();
    for (auto it = glyphs.begin(); it != glyphs.end(); ++it) {
      uint32_t cp = it->first.as<uint32_t>();
      auto n = it->second;
      GlyphMSDF g;
      auto uv = n["uv"];
      g.uv = Rect{ uv["x"].as<float>(), uv["y"].as<float>(),
                   uv["w"].as<float>(), uv["h"].as<float>() };
      g.l = n["l"].as<float>(); g.b = n["b"].as<float>();
      g.r = n["r"].as<float>(); g.t = n["t"].as<float>();
      g.advance = n["advance"].as<float>();
      out.glyphs[cp] = g;
    }

    // Kerning (optional)
    out.kerning.clear();
    if (auto k = y["kerning"]) {
      for (auto e : k) {
        uint32_t u1 = e["u1"].as<uint32_t>();
        uint32_t u2 = e["u2"].as<uint32_t>();
        float adv = e["advance"].as<float>(); // ems
        out.kerning[kernKey(u1, u2)] = adv;
      }
    }
    return true;
  }

} // namespace UI_lib
