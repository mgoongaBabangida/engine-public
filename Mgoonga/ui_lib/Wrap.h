#pragma once
#include "stdafx.h"
#include "FontMSDF.h"

namespace UI_lib
{

  struct TextLineMetrics
  {
    float widthPx = 0.f;
    float ascPx = 0.f;     // ascender in px (typically >0)
    float descPx = 0.f;     // descender in px (positive magnitude)
    float lineH = 0.f;     // line height in px
  };

  struct ShapedLine
  {
    std::u32string text;
    float widthPx = 0.f;
  };

  static inline TextLineMetrics measureRun(const FontMSDF& f,
    std::u32string_view s,
    float pxSize)
  {
    TextLineMetrics m;
    m.ascPx = f.ascender * pxSize;
    m.descPx = std::abs(f.descender * pxSize);
    m.lineH = f.lineHeight * pxSize;

    float penX = 0.f;
    uint32_t prev = 0; bool havePrev = false;
    for (char32_t cp : s) {
      if (havePrev) {
        auto itK = f.kerning.find((uint64_t(prev) << 32) | uint64_t(cp));
        if (itK != f.kerning.end()) penX += itK->second * pxSize;
      }
      auto it = f.glyphs.find(cp);
      penX += (it != f.glyphs.end() ? it->second.advance * pxSize : 0.5f * pxSize);
      prev = uint32_t(cp); havePrev = true;
    }
    m.widthPx = penX;
    return m;
  }

  static inline std::vector<ShapedLine>
    wrapGreedy(const FontMSDF& f, std::u32string_view s, float pxSize, float maxWidthPx)
  {
    std::vector<ShapedLine> out;
    ShapedLine cur;

    auto flushLine = [&]() {
      if (!cur.text.empty() || cur.widthPx > 0.f) out.push_back(cur);
      cur = {};
    };

    std::u32string word; word.reserve(32);
    float wordW = 0.f;
    uint32_t prev = 0; bool havePrev = false;

    auto finishWord = [&](bool addSpaceAfter) {
      if (word.empty()) return;
      // If word alone exceeds width, hard-break the word (naive split)
      if (wordW > maxWidthPx && !cur.text.empty()) flushLine();
      if (cur.widthPx > 0.f && cur.widthPx + wordW > maxWidthPx) flushLine();

      // Append word
      if (cur.widthPx == 0.f) { cur.text = word; cur.widthPx = wordW; }
      else {
        // insert a space between words
        cur.text.push_back(U' ');
        cur.text += word;
        // measure added " space + word "
        float spaceW = measureRun(f, U" ", pxSize).widthPx;
        cur.widthPx += spaceW + wordW;
      }
      word.clear(); wordW = 0.f;
      havePrev = false; prev = 0;
      if (addSpaceAfter) {
        // we accounted the space only when concatenating; nothing else to do
      }
    };

    // iterate chars
    for (char32_t cp : s) {
      if (cp == U'\n') { finishWord(false); flushLine(); continue; }

      if (cp == U' ') { finishWord(true); continue; }

      if (havePrev) {
        auto itK = f.kerning.find((uint64_t(prev) << 32) | uint64_t(cp));
        if (itK != f.kerning.end()) wordW += itK->second * pxSize;
      }
      auto it = f.glyphs.find(cp);
      wordW += (it != f.glyphs.end() ? it->second.advance * pxSize : 0.5f * pxSize);

      word.push_back(cp);
      prev = uint32_t(cp); havePrev = true;
    }
    finishWord(false);
    flushLine();
    return out;
  }

  static inline std::u32string elideWithEllipsis(const FontMSDF& f,
    std::u32string_view s,
    float pxSize, float maxWidthPx)
  {
    const char32_t HELLIP = U'\u2026';
    const bool hasHellip = (f.glyphs.find(HELLIP) != f.glyphs.end());
    const std::u32string ell = hasHellip ? std::u32string(1, HELLIP) : std::u32string(U"...");

    const float ellW = measureRun(f, ell, pxSize).widthPx;
    if (measureRun(f, s, pxSize).widthPx <= maxWidthPx) return std::u32string(s);

    // binary search cut so [0..cut) + ell fits
    size_t lo = 0, hi = s.size();
    while (lo < hi) {
      size_t mid = (lo + hi) / 2;
      float w = measureRun(f, std::u32string_view(s.data(), mid), pxSize).widthPx + ellW;
      if (w <= maxWidthPx) lo = mid + 1; else hi = mid;
    }
    size_t cut = lo ? lo - 1 : 0;

    std::u32string out; out.reserve(cut + ell.size());
    out.append(s.data(), s.data() + cut);
    out += ell;
    return out;
  }

}
