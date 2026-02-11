#pragma once
#include "ui_lib.h"

#include "DrawList.h"
#include "FontMSDF.h"
#include "Theme.h"
#include "PrimitiveRenderer.h"
#include "AnimationSystem.h"
#include "SignalHub.h"
#include "Tooltip.h"
#include "Tween.h"
#include "RichTip.h"
#include "Drag.h"

namespace UI_lib
{
  struct UiContext; // fwd
  struct UiRoot;

  //-----------------------------------------------------------------------------------------------
  struct Anchors
  {
    bool hasL = false, hasR = false, hasT = false, hasB = false;
    float left = 0, right = 0, top = 0, bottom = 0; // distances from parent edges (virtual px)
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB Widget
  {
    Rect rect{}; Rect clip{};  Rect local{};
    bool visible = true; bool hover = false; bool pressed = false; bool focusable = false; float opacity = 1.f;
    bool hitTestSelf = true;          // can this widget receive pointer?
    bool hitTestChildren = true;  // can pointer go to children?
    std::vector<std::unique_ptr<Widget>> children;
    Widget* parent = nullptr;
    std::string id;
    int  tabIndex = 0;           // 0 = part of natural order, >0 explicit order
    bool focused = false;       // runtime state
    RichTipContent richTip;
    bool hasRichTip = false;

    struct AnimCfg {
      enum class Preset { None, Popup, PanelLeft, PanelRight, ToastTop, ToastBottom } preset = Preset::None;
      float openMs = -1.f;          // <0 = keep default
      float closeMs = -1.f;
      tween::Ease easeOpen = tween::Ease::CubicOut;
      tween::Ease easeClose = tween::Ease::CubicIn;
      bool hitTestFollowsVisual = false; // use visualRect() for hittest if true
      // Optional starting pose deltas (override preset if provided)
      std::optional<float> startOpacity;
      std::optional<Vec2>  startOffset;
      std::optional<Vec2>  startScale;
      std::optional<Vec4>  startColor;
      // Signals
      std::string openSignal, closeSignal;
    } animCfg;

    tween::AnimValues animV;
    tween::Transition trans;

    void open() { trans.open(); }
    void close() { trans.close(); }
    
    // Utility: a transformed draw-rect for visual-only motion
    Rect visualRect() const {
    // Apply offset and scale around center; keep layout rect intact
      Rect r = rect;
      const float cx = r.x + r.w * 0.5f;
      const float cy = r.y + r.h * 0.5f;
      r.x = cx - (r.w * animV.scale.x) * 0.5f + animV.offset.x;
      r.y = cy - (r.h * animV.scale.y) * 0.5f + animV.offset.y;
      r.w = r.w * animV.scale.x;
      r.h = r.h * animV.scale.y;
      return r;
    }

    float visualOpacity() const {
      return std::clamp(opacity, 0.f, 1.f) * std::clamp(animV.opacity, 0.f, 1.f);
    }

    bool isAnimating() const {
      return (trans.state == tween::TransState::Opening ||
        trans.state == tween::TransState::Closing);
    }

    virtual void onFocus(UiContext&) { focused = true; }
    virtual void onBlur(UiContext&) { focused = false; }

    // Optional: mark widget as a potential drop target
    virtual bool canAccept(const DragPayload& p) const { (void)p; return false; }

    // Drag hooks (return true if consumed)
    virtual bool onDrag(UiContext& ctx, const DragEvent& d, const DragPayload& p) {
      (void)ctx; (void)d; (void)p; return false;
    }

    // convenience
    bool wantsFocus() const { return visible && focusable; }

    // Rule of 5 (explicit):
    Widget() = default;
    Widget(const Widget&) = delete;                  // no copy
    Widget& operator=(const Widget&) = delete;       // no copy
    Widget(Widget&&) noexcept = default;             // allow moves
    Widget& operator=(Widget&&) noexcept = default;  // allow moves
    virtual ~Widget() = default;

    Anchors anchors{};
    Style inlineStyle; // parsed from YAML (fontSize/color/tint/opacity if present)
    std::string tooltip;

    // layout: compute rect from parent constraints (simple anchors for now)
    virtual void layout(UiContext& ctx, const Rect& parentRect);
    // drawing: emit geometry into drawlist
    virtual void draw(UiContext& ctx) const;
    // event: return true if consumed
    virtual bool onEvent(UiContext& ctx, const UIEvent& e);
    virtual void update(UiContext& ctx, float dt) {
      (void)ctx;
      trans.update(dt);
    }
    // hit testing
    virtual bool hitTest(const Vec2& p) const
    {
      if (!visible) return false;
      if (!contains(clip, p)) return false; // ScrollView !?
      if (animCfg.hitTestFollowsVisual) return contains(visualRect(), p);
      return contains(rect, p);
    }
    virtual void layoutAbsolute(UiContext& ctx, const Rect& absRect);
    virtual void layoutChildren(UiContext& ctx)
    {
      // default = old behavior: recurse using anchor/local logic
      for (auto& c : children) c->layout(ctx, rect);
    }
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB Label : public Widget
  {
    std::string text;
    std::string bindText;
    float fontSize = 16.f;
    uint32_t color = 0x1A1A1AFF; // ink
    enum class Wrap { NoWrap, Wrap } wrap = Wrap::NoWrap;
    enum class Overflow { Clip, Ellipsis } overflow = Overflow::Clip;
    enum class VAlign { Top, Center, Bottom } valign = VAlign::Center;
    enum class HAlign { Left, Center, Right } halign = HAlign::Left;
    void draw(UiContext& ctx) const override;
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB Button : public Widget
  {
     std::string text; std::string bindText; float fontSize = 16.f; uint32_t colorText = 0xFFFCF7FF; uint32_t colorBg = 0x8C6B2AFF; float _alpha = 1.f;
     std::function<void()> onClick;
     bool onEvent(UiContext& ctx, const UIEvent& e) override;
     void draw(UiContext& ctx) const override;
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB Panel : public Widget
  {
    // parchment panel using 9-slice
    std::string nineName = "parchment_panel";
    uint32_t tint = 0xFFFFFFFF;
    void draw(UiContext& ctx) const override;
    bool onEvent(UiContext& ctx, const UIEvent& e);
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB PopupPanel : public Panel
  {
    // Looks/behaves like Panel for draw; we only specialize onEvent.
    // No extra fields needed.

    virtual bool onEvent(UiContext& ctx, const UIEvent& e) override;
    virtual void onOpen(const std::string& resKey, Widget* _parent) = 0;
    virtual void onClose(UiContext& ctx, const char* reason /*"close"|"ok"|"cancel_outside"*/) = 0;
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB Image : public Widget
  {
    std::string spriteName;
    uint32_t tint = 0xFFFFFFFF;
    bool draggable = true;
    bool onEvent(UiContext& ctx, const UIEvent& e);
    void draw(UiContext& ctx) const override;
  };

  //-----------------------------------------------------------------------------------------------
  struct DLL_UI_LIB IconButton : public Button
  {
    std::string spriteName;
    uint32_t tintNormal = 0xFFFFFFFF;
    uint32_t tintHover = 0xFFFFFFFF;
    uint32_t tintDown = 0xFFFFFFFF;

    void draw(UiContext& ctx) const override;
  };

  //-------------------------------------------------------------------------------------
  struct DLL_UI_LIB FlexRow : public Widget
  {
    float gap = 0.f;
    float padL = 0, padR = 0, padT = 0, padB = 0;
    enum class Justify { Start, Center, End, SpaceBetween, SpaceEvenly } justify = Justify::Start;
    enum class Align { Start, Center, End, Top, Bottom } align = Align::Center;

    void layout(UiContext& ctx, const Rect& parentRect) override;
    void layoutChildren(UiContext& ctx) override;
    void draw(UiContext& ctx) const override { Widget::draw(ctx); } // purely layout
  };

  //-----------------------------------------------------------------------------
  struct FlexChildProps
  {
    float grow = 0.f;
    float shrink = 1.f;
    float basis = -1.f; // if >=0, preferred width
  };

  //-----------------------------------------------------------------------------
  struct DLL_UI_LIB CheckBox : public Widget
  {
    bool checked = false;
    std::function<void(bool)> onChanged;

    void draw(UiContext& ctx) const override;
    bool onEvent(UiContext& ctx, const UIEvent& e) override;
  };

  //-----------------------------------------------------------------------------
  struct DLL_UI_LIB LineEdit : public Widget
  {
    std::string text;
    bool active = false;                 // focused editing
    std::function<void(const std::string&)> onCommit; // when closed / enter

    void draw(UiContext& ctx) const override;
    bool onEvent(UiContext& ctx, const UIEvent& e) override;
  };

  //-----------------------------------------------------------------------------------
  struct DLL_UI_LIB ExportImportPopup : public PopupPanel //@todo move to layout(declarative) make PopupPanel for onEvent, wire conections in controller
  {
    std::string resourceKey; // from RefSlotX
    CheckBox* cbExport = nullptr;
    CheckBox* cbImport = nullptr;
    CheckBox* cbStop = nullptr;
    LineEdit* edKeep = nullptr;

    ExportImportPopup() { nineName = "parchment_bar"; }

    virtual void onOpen(const std::string& resKey, Widget* _parent) override;
    virtual void onClose(UiContext& ctx, const char* reason /*"close"|"ok"|"cancel_outside"*/) override;

    void buildChildren();
  };

  //-----------------------------------------------------------------------------------
  struct DLL_UI_LIB ModalLayer : public Widget
  {
    std::unique_ptr<PopupPanel> popup;
    Rect anchor;           // where to appear near
    bool active = false;   // visible?

    void open(std::unique_ptr<PopupPanel>&& _popup, const Rect& anchorRect, const std::string& resKey);
    void close(UiContext& ctx, bool sendPayload);
    void layout(UiContext& ctx, const Rect& parentRect) override;
    void draw(UiContext& ctx) const override;
    bool onEvent(UiContext& ctx, const UIEvent& e) override;
  };

  //-------------------------------------------------------------------------------
  struct DLL_UI_LIB ScrollView : public Widget
  {
    // config
    float padL = 8, padR = 8, padT = 8, padB = 8;
    float wheelStep = 40.f;     // pixels per notch
    bool  showScrollbar = true;

    uint32_t barColor = 0x00000066;    // track
    uint32_t thumbColor = 0x000000AA;  // thumb
    uint32_t thumbHover = 0x000000CC;
    uint32_t thumbActive = 0x000000FF;

    // Track (choose one): nine-slice or sprite; if empty -> fallback color (barColor)
    std::string trackNineName;     // e.g. "scroll_track_9"
    std::string trackSpriteName;   // e.g. "scroll_track"

    // Thumb (choose one): nine-slice or sprite; if empty -> fallback colors
    std::string thumbNineName;     // e.g. "scroll_thumb_9"
    std::string thumbSpriteName;   // e.g. "scroll_thumb"

    // Optional per-state tints for sprite/nine (fallback to existing colors)
    uint32_t trackTint = 0xFFFFFFFF;
    uint32_t thumbTintNormal = 0xFFFFFFFF;
    uint32_t thumbTintHover = 0xFFFFFFFF;
    uint32_t thumbTintActive = 0xFFFFFFFF;

    // runtime
    float scrollY = 0.f;        // current scroll offset (0..max)
    float contentH = 0.f;       // measured total content height (before scroll)
    float innerW = 0.f, innerH = 0.f;  // viewport inner size

    //momentum
    float velY = 0.f;          // px/s
    float damping = 8.f;       // exp decay per second
    float maxVel = 6'000.f;     // clamp
    bool  kinetic = false;     // moving due to momentum

    float  barWidthPx = 8.f;
    float  barPadPx = 2.f;        // inset from right/top/bottom
    float  minThumbH = 20.f;
    float minThumbPx = 18.f;    // minimum thumb size

    // drag state
    bool   dragging = false;
    bool   hoverThumb = false;
    float  grabDy = 0.f;               // mouse–thumb offset for stable drag

    // auto-hide state
    float  autoHideAlpha = 0.f;        // 0..1
    float  autoHideFadeIn = 10.f;     // 1/sec (to 1)
    float  autoHideFadeOut = 2.0f;     // 1/sec (to 0)
    float  sinceScroll = 0.f;          // seconds since last wheel/drag activity
    float  showHoldSeconds = 0.6f;     // stay visible after interaction
    float  dragAnchor = 0.f;     // pointer offset inside the thumb on Down

    // layout: compute contentH and apply translation (-scrollY) to children
    void layout(UiContext& ctx, const Rect& parentRect) override;
    // events: handle wheel
    bool onEvent(UiContext& ctx, const UIEvent& e) override;
    void draw(UiContext& ctx) const override;

    void update(UiContext& ctx, float dt) override;

    // hit test only inside viewport
    bool hitTest(const Vec2& p) const override {
      return visible && contains(rect, p);
    }

    // Vertical scrollbar track inside this ScrollView
    inline Rect trackRect() const {
      const float trW = 8.f;
      const float x = rect.x + rect.w - trW - 2.f;
      return Rect{ x, rect.y + 2.f, trW, rect.h - 4.f };
    }

    // Thumb rect derived from scrollY/contentH/innerH
    inline Rect thumbRect() const {
      if (contentH <= innerH + 0.5f) return Rect{ 0,0,0,0 }; // no scroll
      Rect tr = trackRect();
      const float ratio = innerH / contentH;
      const float h = std::max(minThumbPx, tr.h * ratio);
      const float maxScroll = std::max(0.f, contentH - innerH);
      const float t = (maxScroll > 0.f) ? (scrollY / maxScroll) : 0.f; // 0..1
      const float y = tr.y + (tr.h - h) * t;
      return Rect{ tr.x, y, tr.w, h };
    }
  };

  //----------------------------------------------------------------------------------------------
  struct DropSlot : Panel
  {
    std::string slotKey; // e.g., "market"

    bool canAccept(const DragPayload& p) const override {
      // Accept citizens only, add more rules as needed
      return p.type == DragPayload::Type::Citizen && !p.s0.empty();
    }

    bool onDrag(UiContext& ctx, const DragEvent& d, const DragPayload& p) override;

    void draw(UiContext& ctx) const override;
  };

  // Simple root that handles routing (capture→target→bubble)
  //----------------------------------------------------------------------------------------------------
  struct DLL_UI_LIB UiRoot
  {
    // Non-owning; UiSystem controls lifetime & z-order. Bottom→top.
    std::vector<Widget*> contents;

    Widget* pointerCapture = nullptr;
    Widget* hoverWidget = nullptr;
    Widget* focusWidget = nullptr;

    void setFocus(Widget* w)
    {
      focusWidget = w;
    }
    void clearFocus(Widget* w = nullptr) {
      if (!w || w == focusWidget) focusWidget = nullptr;
    }

  // Build a path from root to target (for bubble)
    static void buildPath(Widget* root, const Vec2& p, std::vector<Widget*>& out);

    bool isInAnyRoot(const Widget* w) const;
    static bool isDescendant(const Widget* root, const Widget* w);
    void sanitizePointers(UiContext& ctx);

    bool route(UiContext& ctx, const UIEvent& ev);
    void draw(UiContext& ctx);
  };

  // Non-const
  template<class T = Widget>
  T* find(Widget* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->id == id) {
      if constexpr (std::is_same_v<T, Widget>) return root;
      if (auto* casted = dynamic_cast<T*>(root)) return casted;
    }
    for (auto& c : root->children) {
      if (auto* r = find<T>(c.get(), id)) return r;
    }
    return nullptr;
  }

  // Const
  template<class T = Widget>
  const T* find(const Widget* root, const std::string& id) {
    return find<T>(const_cast<Widget*>(root), id);
  }

  template <typename T = Widget>
  T* findWidgetById(Widget* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->id == id) return dynamic_cast<T*>(root);
    for (auto& c : root->children)
      if (auto* r = findWidgetById<T>(c.get(), id)) return r;
    return nullptr;
  }

  // Returns UI scale and paddings (same as you use for uVirtualToClip)
  static inline float uiScale(const UI_lib::UiContext& ctx);

  // Draw a 1 device-pixel outline regardless of virtual scale
  inline void drawOutlineDevicePx(UI_lib::PrimitiveRenderer& prim,
                                  UI_lib::DrawList& dl,
                                  const UI_lib::Rect& rect,
                                  uint32_t color,
                                  const UI_lib::Rect& clip,
                                  const UI_lib::UiContext& ctx);
}
