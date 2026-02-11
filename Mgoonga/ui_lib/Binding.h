#pragma once
#include "SignalHub.h"
#include "Widget.h"
#include "Value.h"
#include <cstdio>

namespace UI_lib
{
  // One-way: Label.text <- Signal(string/int/double)
  inline void bindLabelText(Label& lbl, SignalHub& hub, const std::string& chan)
  {
    hub.subscribe(chan, [&lbl](const Payload& p)
      {
      std::string s;
      if (p.asStr()) s = *p.asStr();
      else if (p.asInt()) s = std::to_string(*p.asInt());
      else if (p.asDouble())  s = std::to_string((int64_t)(*p.asDouble()));
      else if (p.asBool())    s = p.asBool() ? "true" : "false";
      lbl.text = s;
      // mark dirty if you track it (optional)
      });
  }

  // Button emits on click
  inline void bindButtonClick(Button& btn, SignalHub& hub, const std::string& chan, Payload payload = true)
  {
    btn.onClick = [&hub, chan, payload]() {
      hub.post(chan, payload);
    };
  }

} // namespace

