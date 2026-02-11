#pragma once
#include "stdafx.h"

#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Event.h>
#include <filesystem>
#include "MainContextBase.h"

//-------------------------------------------------
class DLL_GAME_ASSETS ScriptManager
{
public:
  explicit ScriptManager(eMainContextBase*);

  base::Event<std::function<void(const std::string&)>> DLLToBeUnloaded;
  base::Event<std::function<void(const std::string&)>> DLLUnloaded;

  struct ScriptEntry
  {
    std::string name;
    std::filesystem::path originalPath;
    std::filesystem::path tempCopyPath;
    std::filesystem::path pdbCopyPath;
    std::filesystem::file_time_type lastWriteTime;

    HMODULE dll = nullptr;
    IScript* (*factory)(eMainContextBase*) = nullptr;
    void(*deleter)(IScript*) = nullptr;
  };
  
  void      addExistingScriptEntry(const ScriptEntry&);
  bool      loadScriptsFromFolder(const std::string& folder);
  bool      checkForScriptChanges();
  std::pair<IScript*, void(*)(IScript*)> createScriptByName(const std::string& name);

  ~ScriptManager();

protected:
  eMainContextBase* m_context = nullptr;
  std::unordered_map<std::string, ScriptEntry> m_scripts;
  std::string m_folder;
  std::string m_tempFolder = "TempScripts";

  void          unloadScript(const std::string& name);
  std::string   makeTempDllName(const std::string& baseName);
};
