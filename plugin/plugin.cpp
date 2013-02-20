// Copyright (c) 2012-2013 Zeex
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
  #include <windows.h>
#else
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE 1 // for dl_addr()
  #endif
  #include <dlfcn.h>
#endif
#include <subhook.h>
#include "plugin.h"
#include "version.h"
#include "amxjit/amxdisasm.h"
#include "amxjit/compiler.h"
#include "amxjit/compiler-asmjit.h"
#include "amxjit/jit.h"

#if defined __GNUC__ && !defined __MINGW32__
  #define USE_OPCODE_MAP 1
#endif

extern void *pAMXFunctions;

typedef void (*logprintf_t)(const char *format, ...);
static logprintf_t logprintf;

typedef std::map<AMX*, amxjit::JIT*> AmxToJitMap;
static AmxToJitMap amx_to_jit;

static SubHook amx_Exec_hook;
#if USE_OPCODE_MAP
  static cell *opcode_map = 0;
#endif

static std::string GetModulePath(void *address,
                                 std::size_t max_length = FILENAME_MAX)
{
  #ifdef _WIN32
    std::vector<char> name(max_length + 1);
    if (address != 0) {
      MEMORY_BASIC_INFORMATION mbi;
      VirtualQuery(address, &mbi, sizeof(mbi));
      GetModuleFileName((HMODULE)mbi.AllocationBase, &name[0], max_length);
    }
    return std::string(&name[0]);
  #else
    std::vector<char> name(max_length + 1);
    if (address != 0) {
      Dl_info info;
      dladdr(address, &info);
      strncpy(&name[0], info.dli_fname, max_length);
    }  
    return std::string(&name[0]);
  #endif
}

static std::string GetFileName(const std::string &path) {
  std::string::size_type pos = path.find_last_of("/\\");
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

static int AMXAPI amx_Exec_JIT(AMX *amx, cell *retval, int index) {
  #if USE_OPCODE_MAP
    if ((amx->flags & AMX_FLAG_BROWSE) == AMX_FLAG_BROWSE) {
      assert(::opcode_map != 0);
      *retval = reinterpret_cast<cell>(::opcode_map);
      return AMX_ERR_NONE;
    }
  #endif
  AmxToJitMap::iterator iterator = ::amx_to_jit.find(amx);
  if (iterator == ::amx_to_jit.end()) {
    SubHook::ScopedRemove r(&amx_Exec_hook);
    return amx_Exec(amx, retval, index);
  } else {
    amxjit::JIT *jit = iterator->second;
    return jit->exec(index, retval);
  }
}

static int AMXAPI amx_Debug_JIT(AMX *amx) {
  return AMX_ERR_NONE;
}

class CompileErrorHandler : public amxjit::CompileErrorHandler {
 public:
  CompileErrorHandler(amxjit::JIT *jit) : jit_(jit) {}
  virtual void execute(const amxjit::AMXInstruction &instr) {
    logprintf("[jit] Invalid or unsupported instruction at address %p:",
              instr.address());
    logprintf("[jit]   => %s", instr.string().c_str());
  }
 private:
  amxjit::JIT *jit_;
};

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
  return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
  logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];
  pAMXFunctions = reinterpret_cast<void*>(ppData[PLUGIN_DATA_AMX_EXPORTS]);

  void *ptr = SubHook::ReadDst(((void**)pAMXFunctions)[PLUGIN_AMX_EXPORT_Exec]);
  if (ptr != 0) {
    std::string module = GetFileName(GetModulePath(ptr));
    if (!module.empty()) {
      logprintf("  JIT must be loaded before '%s'", module.c_str());
      return false;
    }
  }

  #if USE_OPCODE_MAP
    // Get opcode table before we hook amx_Exec().
    AMX amx = {0};
    amx.flags |= AMX_FLAG_BROWSE;
    amx_Exec(&amx, reinterpret_cast<cell*>(&::opcode_map), 0);
    amx.flags &= ~AMX_FLAG_BROWSE;
  #endif

  amx_Exec_hook.Install(((void**)pAMXFunctions)[PLUGIN_AMX_EXPORT_Exec],
                        (void*)amx_Exec_JIT);

  logprintf("  JIT plugin v%s is OK.", PROJECT_VERSION_STRING);
  return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
  for (AmxToJitMap::iterator it = amx_to_jit.begin();
       it != amx_to_jit.end(); ++it) {
    delete it->second;
  }
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx) {
  amxjit::JIT *jit = new amxjit::JIT(amx);

  const char *backend_string = getenv("JIT_BACKEND");
  if (backend_string == 0) {
    backend_string = "asmjit";
  }

  amxjit::Compiler *compiler = 0;
  if (std::strcmp(backend_string, "asmjit") == 0) {
    compiler = new amxjit::CompilerAsmjit;
  } else {
    logprintf("[jit] Unknown backend '%s'", backend_string);
  }

  if (compiler != 0) {
    CompileErrorHandler error_handler(jit);
    if (!jit->compile(compiler, &error_handler)) {
      delete jit;
    } else {
      #ifdef DEBUG
        amx_SetDebugHook(amx, amx_Debug_JIT);
      #endif
      ::amx_to_jit.insert(std::make_pair(amx, jit));
    }
  } else {
    delete jit;
  }

  delete compiler;
  return AMX_ERR_NONE;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx) {
  AmxToJitMap::iterator it = amx_to_jit.find(amx);
  if (it != amx_to_jit.end()) {
    delete it->second;
    amx_to_jit.erase(it);
  }
  return AMX_ERR_NONE;
}
