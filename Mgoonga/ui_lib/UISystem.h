
#pragma once
#include "ui_lib.h"
#include "Widget.h"
#include "Context.h"
#include "WidgetFactory.h"
#include "YamlLoader.h"
#include "PrimitiveRenderer.h"
#include "DrawListBus.h"
#include "ThemeLoader.h"

#include <filesystem>

namespace UI_lib
{
  //------------------------------------------------------------------------------
  struct MountingHost
  {
    // fast lookup by id (fill this after loadLayout)
    std::unordered_map<std::string, Widget*> idIndex;

    // Mount table: (scope|entityKey) -> widget
    std::unordered_map<std::string, Widget*> mounts;

    // Call this after loadLayout() or hot-reload
    void rebuildIdIndex(Widget* root) {
      idIndex.clear();
      if (!root) return;
      registerSubtree(root);
    }

    // Register a widget (and its subtree) into idIndex
    void registerSubtree(Widget* w) {
      if (!w) return;
      if (!w->id.empty()) {
        auto [it, inserted] = idIndex.emplace(w->id, w);
        if (!inserted) {
          // Optional: warn about duplicate IDs
          if (it->second != w) {
            fprintf(stderr, "[UI] Duplicate id '%s' (keeping first)\n", w->id.c_str());
          }
        }
      }
      for (auto& c : w->children) registerSubtree(c.get());
    }

    // Unregister a subtree (when removing widgets)
    void unregisterSubtree(Widget* w) {
      if (!w) return;
      if (!w->id.empty()) {
        auto it = idIndex.find(w->id);
        if (it != idIndex.end() && it->second == w) idIndex.erase(it);
      }
      for (auto& c : w->children) unregisterSubtree(c.get());
    }

    // Convenience: when you create a new widget and attach it at runtime
    void registerWidget(Widget* w) { registerSubtree(w); }
    void unregisterWidget(Widget* w) { unregisterSubtree(w); }

    std::string mkKey(const std::string& scopeId, const std::string& entityKey) const {
      return scopeId + "|" + entityKey;
    }

    // Helpers
    Widget* find(const std::string& id) const {
      auto it = idIndex.find(id); return it == idIndex.end() ? nullptr : it->second;
    }

    // Dynamic API (generic)
    Widget* mountSprite(UiContext& ctx, const std::string& scopeId,
      const std::string& slotKey,
      const std::string& entityKey,
      const std::string& spriteName,
      const Rect& localRect = { 0,0,36,36 });

    bool moveMount(UiContext& ctx, const std::string& scopeId,
      const std::string& newSlotKey,
      const std::string& entityKey);

    bool retintMount(UiContext& ctx, const std::string& scopeId,
      const std::string& entityKey,
      const std::string& newSpriteName);

    void unmount(UiContext& ctx, const std::string& scopeId,
      const std::string& entityKey);

    void clearScope(UiContext& ctx, const std::string& scopeId);

    void ensureSlot(UiContext& ctx,
                    const std::string& scopeId,
                    const std::string& slotKey,
                    const std::string& displayName);
  };

  //---------------------------------------------------------------------------------
  struct UiView
  {
    std::string name;
    std::string yamlPath;
    std::unique_ptr<Widget> root;  // OWNED by UiSystem
    int layer = 0;                 // z-order: larger draws above & gets input first
    bool visible = false;
    bool isScreen = false;         // true = primary “screen”, false = overlay/HUD
  };

  //---------------------------------------------------------------------------------
  struct WatchedLayout
  {
    std::string viewId;        // e.g., "main"
    std::string path;          // e.g., "ui/layout/main.yaml"
    std::filesystem::file_time_type lastWrite{};
  };

  //------------------------------------------------------------------------------
  struct DLL_UI_LIB UiSystem
  {
    static UiSystem& I() { static UiSystem s; return s; }

    UiContext ctx;
    UiRoot root;
    DrawListBus bus;
    MountingHost mountingHost;

    bool dirty = true;

    void initialize(const std::string& themePath,
                    const std::string& atlasPath,
                    const std::string& layoutPath,
                    int fbW, int fbH, int virtW = 1920, int virtH = 1080);

    void onResize(int newFbW, int newFbH) { ctx.fbW = newFbW; ctx.fbH = newFbH; }
    bool route(const UIEvent& e);
    bool update(float dt);
    void build();

    void drawOverlay(UiContext& ctx);
    void drawTooltip(UiContext& ctx);
    void drawRichTooltip(UiContext& ctx);

    // View API
    UiView& loadView(const std::string& name,
                     const std::string& yamlPath,
                     int layer,
                     bool isScreen);

    void showView(const std::string& name);
    void hideView(const std::string& name);
    void toggleView(const std::string& name);
    void switchTo(const std::string& screenName); // hide current screen, show target

    // Lookup helpers
    Widget* viewRoot(const std::string& name);            // raw ptr to view’s root (or nullptr)
    Widget* activeRoot() const; // top-most visible screen’s root (for legacy helpers)

    template<class T> T* findInView(const std::string& name, const std::string& id) {
      if (auto* r = viewRoot(name)) return find<T>(r, id);
      return nullptr;
    }

    template<class T> T* findAny(const std::string& id) {
      // search top-most visible → down
      std::vector<UiView*> z;
      z.reserve(m_views.size());
      for (auto& v : m_views) if (v.visible) z.push_back(&v);
      std::sort(z.begin(), z.end(), [](auto a, auto b) { return a->layer < b->layer; });
      for (auto it = z.rbegin(); it != z.rend(); ++it) {
        if (auto* r = (*it)->root.get()) if (auto* w = find<T>(r, id)) return w;
      }
      return nullptr;
    }

    // Hot reload
    void enableHotReload(bool on) { m_hotReload = on; }

    void watchLayout(const std::string& viewId, const std::string& path) {
      WatchedLayout w; w.viewId = viewId; w.path = path;
      try { if (std::filesystem::exists(path)) w.lastWrite = std::filesystem::last_write_time(path); }
      catch (...) {}
      m_watched.push_back(std::move(w));
    }

    void reloadActiveView();
    void reloadView(const std::string& viewId, const std::string& path);
    void restoreFocus();

    // Modal popups
    void openEIPopup(const Rect& anchor, const std::string& resKey) {
      if (!modal) modal = std::make_unique<ModalLayer>();
      std::unique_ptr<PopupPanel> popup = std::make_unique<ExportImportPopup>();
      popup->id = "EIPopup";
      modal->open(std::move(popup), anchor, resKey);
      ctx.invalidate();
    }
    void closeEIPopup(bool sendPayload) {
      if (modal && modal->active) modal->close(ctx, sendPayload);
    }

    void openChooseJobPopup(const Rect& anchor, const Payload& data);
    void closeChooseJobPopup(bool sendPayload = false);

    void openGrowthPopup(const Rect& anchor, const Payload& data);

    void closeGrowthPopup(bool sendPayload)
    {
      if (modal && modal->active) modal->close(ctx, sendPayload);
    }

  private:
    bool updateTree(Widget* w, UiContext& ctx, float dt);
    bool anyAnimating(Widget* w) const;

    //view api
    UiView* findView(const std::string& name);
    void rebuildUiRootContents(); // rebuilds UiRoot’s non-owning pointer list in z-order

    DrawList          m_drawList;
    Animator          m_animator;
    PrimitiveRenderer m_prim;

    std::vector<UiView> m_views;     // OWNERSHIP
    std::string         m_activeScreen;      // name of current primary screen

    std::vector<WatchedLayout> m_watched;
    bool                       m_hotReload = true;

    std::unique_ptr<ModalLayer> modal;
  };
}
