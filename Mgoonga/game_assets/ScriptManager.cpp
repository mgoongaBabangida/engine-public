#include"stdafx.h"
#include "ScriptManager.h"

#include <iostream>

//------------------------------------------------------------------------------
ScriptManager::ScriptManager(eMainContextBase* _context)
  : m_context(_context)
{
}

//------------------------------------------------------------------------------
// Add an already loaded entry
void ScriptManager::addExistingScriptEntry(const ScriptEntry& _entry)
{
  m_scripts[_entry.name] = _entry;
}

//------------------------------------------------------------------------------
// Load (or reload) scripts from folder
bool ScriptManager::loadScriptsFromFolder(const std::string& folder)
{
  m_folder = folder;
  std::filesystem::create_directories(m_tempFolder);
  bool reloaded = false;

  for (const auto& file : std::filesystem::directory_iterator(folder))
  {
    if (file.path().extension() != ".dll") continue;

    auto path = file.path();
    auto writeTime = std::filesystem::last_write_time(path);

    // Already loaded? Check timestamp
    auto existing = std::find_if(m_scripts.begin(), m_scripts.end(),
      [&](const auto& p) { return p.second.originalPath == path; });

    if (existing != m_scripts.end() && existing->second.lastWriteTime == writeTime)
      continue; // Unchanged

  // Unload if already loaded
    if (existing != m_scripts.end())
    {
      DLLToBeUnloaded.Occur(existing->first);
      unloadScript(existing->first);
      m_scripts.erase(existing);
    }

    // Copy DLL to temp
    std::string tempDllPath = makeTempDllName(path.stem().string());
    try {
      std::filesystem::copy_file(path, tempDllPath, std::filesystem::copy_options::overwrite_existing);
    }
    catch (const std::exception& e) {
      std::cerr << "Failed to copy DLL: " << e.what() << "\n";
      continue;
    }

    // Copy PDB (optional)
    std::string pdbOriginal = path.parent_path().string() + "/" + path.stem().string() + ".pdb";
    std::string pdbCopy = tempDllPath.substr(0, tempDllPath.size() - 4) + ".pdb";

    if (std::filesystem::exists(pdbOriginal))
    {
      try {
        std::filesystem::copy_file(pdbOriginal, pdbCopy, std::filesystem::copy_options::overwrite_existing);
      }
      catch (const std::exception& e) {
        std::cerr << "Failed to copy PDB: " << e.what() << "\n";
      }
    }

    // Load DLL
    HMODULE dll = LoadLibraryA(tempDllPath.c_str());
    if (!dll)
    {
      std::cerr << "Failed to load script DLL: " << tempDllPath << "\n";
      continue;
    }

    auto getName = (const char* (*)())GetProcAddress(dll, "GetScriptName");
    auto create = (IScript * (*)(eMainContextBase*))GetProcAddress(dll, "CreateScript");
    auto destroy = (void(*)(IScript*))GetProcAddress(dll, "DestroyScript");

    if (!getName || !create)
    {
      std::cerr << "Missing entry points in script DLL: " << tempDllPath << "\n";
      FreeLibrary(dll);
      continue;
    }
    std::string scriptName = getName();

    ScriptEntry entry;
    entry.name = scriptName;
    entry.originalPath = path;
    entry.tempCopyPath = tempDllPath;
    entry.pdbCopyPath = pdbCopy;
    entry.dll = dll;
    entry.factory = create;
    entry.lastWriteTime = writeTime;
    entry.deleter = destroy;

    m_scripts[scriptName] = entry;
    DLLUnloaded.Occur(scriptName);

    std::cout << "[ScriptManager] Loaded script: " << scriptName << "\n";
    reloaded = true;
  }

  return reloaded;
}

//------------------------------------------------------------------------------
// Check for changes and reload if needed
bool ScriptManager::checkForScriptChanges()
{
  if (m_folder.empty())
    return false;

  return loadScriptsFromFolder(m_folder);
}

//------------------------------------------------------------------------------
// Create script instance
std::pair<IScript*, void(*)(IScript*)> ScriptManager::createScriptByName(const std::string& name)
{
  auto it = m_scripts.find(name);
  if (it != m_scripts.end() && it->second.factory)
    return { it->second.factory(m_context), it->second.deleter};

  return { nullptr, nullptr };
}

//------------------------------------------------------------------------------
// Unload all on destruction
ScriptManager::~ScriptManager()
{
  for (auto& [name, entry] : m_scripts) {
    if (entry.dll) {
      FreeLibrary(entry.dll);
    }

    std::error_code ec;
    std::filesystem::remove(entry.tempCopyPath, ec);
    std::filesystem::remove(entry.pdbCopyPath, ec);
  }
  m_scripts.clear();
}

//------------------------------------------------------------------------------
// Unload a specific script
void ScriptManager::unloadScript(const std::string& name)
{
  auto it = m_scripts.find(name);
  if (it != m_scripts.end())
  {
    if (it->second.dll)
    {
      FreeLibrary(it->second.dll);

      std::error_code ec;
      std::filesystem::remove(it->second.tempCopyPath, ec);
      if (!it->second.pdbCopyPath.empty())
        std::filesystem::remove(it->second.pdbCopyPath, ec);

      std::cout << "[ScriptManager] Unloaded script: " << name << "\n";
    }
  }
}

//------------------------------------------------------------------------------
// Generate temp DLL filename
std::string ScriptManager::makeTempDllName(const std::string& baseName)
{
  static int counter = 0;
  return m_tempFolder + "/" + baseName + "_temp_" + std::to_string(counter++) + ".dll";
}
