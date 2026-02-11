#pragma once
#include "Widget.h"

namespace UI_lib
{
  class GrowthPopup : public PopupPanel
  {
  public:
    struct CostItem { int qty = 0; std::string sprite; };
    struct Row {
      std::string code;   // e.g. "peasant", "archer"
      std::string label;  // visible text
      std::string sprite; // left icon
      std::vector<CostItem> cost; // empty => free
    };

    void onOpen(const std::string& /*resKey*/, UI_lib::Widget* _parent) override;
    void onClose(UI_lib::UiContext& ctx, const char* reason) override;

    bool onEvent(UiContext& ctx, const UIEvent& e) override;
    void apply(const Payload& data);
  private:
    void buildChildren();
    void rebuildRows();
    void clearDynamicRows();

    UiContext* m_ctx = nullptr;

    std::string m_title = "Cannons or butter?";
    std::string m_scopeId;

    std::vector<Row> m_rows;

    // layout
    int m_w = 320;
    int m_pad = 12;
    int m_titleH = 34;
    int m_rowH = 44;

    Panel* m_root = nullptr;
    Label* m_titleLb = nullptr;
    ScrollView* m_sv = nullptr;
  };

}

