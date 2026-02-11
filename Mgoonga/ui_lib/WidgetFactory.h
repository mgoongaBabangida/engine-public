#pragma once
#include "ui_lib.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include "Widget.h"

namespace UI_lib
{
  struct Widget;
  using WidgetFactory = std::function<std::unique_ptr<Widget>(const YAML::Node&)>;
  DLL_UI_LIB extern std::unordered_map<std::string, WidgetFactory> g_factory;

  DLL_UI_LIB void registerBuiltins();
  DLL_UI_LIB void registerLegacy();

  DLL_UI_LIB std::unique_ptr<Widget> buildWidgetTree(const YAML::Node& root);
  DLL_UI_LIB std::unique_ptr<Widget> loadLayout(const std::string& path);

  DLL_UI_LIB uint32_t parseColor(const YAML::Node& n);
  DLL_UI_LIB Rect     parseRect(const YAML::Node& n);
}