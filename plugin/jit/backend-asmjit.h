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

#ifndef JIT_BACKEND_ASMJIT_H
#define JIT_BACKEND_ASMJIT_H

#include <cstddef>
#include "amxptr.h"
#include "backend.h"
#include "macros.h"

namespace jit {

class CompileErrorHandler;

class AsmjitBackend : public Backend {
 public:
  AsmjitBackend();
  virtual ~AsmjitBackend();

  virtual BackendOutput *compile(AMXPtr amx, 
                                 CompileErrorHandler *error_handler);

 private:
  JIT_DISALLOW_COPY_AND_ASSIGN(AsmjitBackend);
};

class AsmjitBackendOutput : public BackendOutput {
 public:
  AsmjitBackendOutput(void *code, std::size_t code_size);
  virtual ~AsmjitBackendOutput();

  virtual void *code() const {
    return code_;
  }
  virtual std::size_t code_size() const {
   return code_size_;
  }

 private:
  void *code_;
  std::size_t code_size_;
  
 private:
  JIT_DISALLOW_COPY_AND_ASSIGN(AsmjitBackendOutput);
};

} // namespace jit

#endif // !JIT_BACKEND_ASMJIT