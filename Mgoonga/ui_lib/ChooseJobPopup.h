#pragma once
#include <string>
#include <vector>
#include <memory>

#include "Value.h"
#include "Widget.h"

namespace UI_lib {

  class ChooseJobPopup : public PopupPanel
  {
  public:
    ChooseJobPopup();

    // Feed data BEFORE modal->open()
    // {
    //   "title": "Choose a Profession for Free Colonist",
    //   "tileIndex": 12,
    //   "rows": [ {"job":"farmer","name":"Farmer","amount":4,"sprite":"wheat"}, ... ]
    // }
    void apply(const Payload& data);

    // PopupPanel interface
    void onOpen(const std::string& resKey, Widget* _parent) override;
    void onClose(UiContext& ctx, const char* reason /*"close"|"ok"|"cancel_outside"*/) override;

    // Optional: capture ctx so row onClick can post to hub safely
    bool onEvent(UiContext& ctx, const UIEvent& e) override;

  private:
    struct Row {
      std::string job;
      std::string name;
      std::string sprite;
      int amount = 0;
    };

    void buildChildren();
    void rebuildRows();
    void clearDynamicRows();

  private:
    // Data
    std::string m_title = "Choose a Profession";
    int m_tileIndex = -1;
    std::string m_entityKey;
    std::vector<Row> m_rows;

    // Modal plumbing
    std::string m_resKey;
    Widget* m_parent = nullptr;
    UiContext* m_ctx = nullptr; // set in onEvent()

    // Widgets
    Panel* m_root = nullptr;
    Label* m_titleLb = nullptr;
    Panel* m_list = nullptr;

    // Layout
    int m_pad = 12;
    int m_w = 520;
    int m_titleH = 34;
    int m_rowH = 44;
  };

} // namespace UI_lib
