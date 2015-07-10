# Detect Clang libraries
#
# Defines the following variables:
#  CLANG_FOUND                 - True if Clang was found
#  CLANG_INCLUDE_DIR           - Where to find Clang includes
#  CLANG_LIBRARY_DIR           - Where to find Clang libraries
#
#  CLANG_CLANG_LIB             - LibClang library
#  CLANG_CLANGFRONTEND_LIB     - Clang Frontend Library
#  CLANG_CLANGDRIVER_LIB       - Clang Driver Library
#  ...
#
# Uses the same include and library paths detected by FindLLVM.cmake
#
# See http://clang.llvm.org/docs/InternalsManual.html for full list of libraries

#=============================================================================
# Copyright 2014 Kevin Funk <kfunk@kde.org>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.

#=============================================================================

find_package(LLVM ${Clang_FIND_VERSION} ${Clang_FIND_REQUIRED})

set(CLANG_FOUND FALSE)

if (LLVM_FOUND AND LLVM_LIBRARY_DIR)
  macro(FIND_AND_ADD_CLANG_LIB _libname_)
    string(TOUPPER ${_libname_} _prettylibname_)
    find_library(CLANG_${_prettylibname_}_LIB NAMES ${_libname_} HINTS ${LLVM_LIBRARY_DIR})
    if(CLANG_${_prettylibname_}_LIB)
      set(CLANG_LIBS ${CLANG_LIBS} ${CLANG_${_prettylibname_}_LIB})
    endif()
  endmacro(FIND_AND_ADD_CLANG_LIB)

  FIND_AND_ADD_CLANG_LIB(clang) # LibClang: high-level C interface
  FIND_AND_ADD_CLANG_LIB(clangFrontend)
  FIND_AND_ADD_CLANG_LIB(clangDriver)
  FIND_AND_ADD_CLANG_LIB(clangCodeGen)
  FIND_AND_ADD_CLANG_LIB(clangSema)
  FIND_AND_ADD_CLANG_LIB(clangChecker)
  FIND_AND_ADD_CLANG_LIB(clangAnalysis)
  FIND_AND_ADD_CLANG_LIB(clangIndex)
  FIND_AND_ADD_CLANG_LIB(clangARCMigrate)
  FIND_AND_ADD_CLANG_LIB(clangAST)
  FIND_AND_ADD_CLANG_LIB(clangASTMatchers)
  FIND_AND_ADD_CLANG_LIB(clangParse)
  FIND_AND_ADD_CLANG_LIB(clangLex)
  FIND_AND_ADD_CLANG_LIB(clangBasic)
  FIND_AND_ADD_CLANG_LIB(clangEdit)
  FIND_AND_ADD_CLANG_LIB(clangFrontendTool)
  FIND_AND_ADD_CLANG_LIB(clangRewrite)
  FIND_AND_ADD_CLANG_LIB(clangRewriteFrontend)
  FIND_AND_ADD_CLANG_LIB(clangSerialization)
  FIND_AND_ADD_CLANG_LIB(clangTooling)
  FIND_AND_ADD_CLANG_LIB(clangToolingCore)
  FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerCheckers)
  FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerCore)
  FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerFrontend)
  FIND_AND_ADD_CLANG_LIB(clangSema)
  FIND_AND_ADD_CLANG_LIB(clangRewriteCore)
  FIND_AND_ADD_CLANG_LIB(LLVMLTO)
  FIND_AND_ADD_CLANG_LIB(LLVMObjCARCOpts)
  FIND_AND_ADD_CLANG_LIB(LLVMLinker)
  FIND_AND_ADD_CLANG_LIB(LLVMipo)
  FIND_AND_ADD_CLANG_LIB(LLVMVectorize)
  FIND_AND_ADD_CLANG_LIB(LLVMBitWriter)
  FIND_AND_ADD_CLANG_LIB(LLVMCppBackendCodeGen)
  FIND_AND_ADD_CLANG_LIB(LLVMCppBackendInfo)
  FIND_AND_ADD_CLANG_LIB(LLVMTableGen)
  FIND_AND_ADD_CLANG_LIB(LLVMDebugInfo)
  FIND_AND_ADD_CLANG_LIB(LLVMOption)
  FIND_AND_ADD_CLANG_LIB(LLVMX86Disassembler)
  FIND_AND_ADD_CLANG_LIB(LLVMX86AsmParser)
  FIND_AND_ADD_CLANG_LIB(LLVMX86CodeGen)
  FIND_AND_ADD_CLANG_LIB(LLVMSelectionDAG)
  FIND_AND_ADD_CLANG_LIB(LLVMAsmPrinter)
  FIND_AND_ADD_CLANG_LIB(LLVMX86Desc)
  FIND_AND_ADD_CLANG_LIB(LLVMMCDisassembler)
  FIND_AND_ADD_CLANG_LIB(LLVMX86Info)
  FIND_AND_ADD_CLANG_LIB(LLVMX86AsmPrinter)
  FIND_AND_ADD_CLANG_LIB(LLVMX86Utils)
  FIND_AND_ADD_CLANG_LIB(LLVMMCJIT)
  FIND_AND_ADD_CLANG_LIB(LLVMIRReader)
  FIND_AND_ADD_CLANG_LIB(LLVMAsmParser)
  FIND_AND_ADD_CLANG_LIB(LLVMLineEditor)
  FIND_AND_ADD_CLANG_LIB(LLVMInstrumentation)
  FIND_AND_ADD_CLANG_LIB(LLVMInterpreter)
  FIND_AND_ADD_CLANG_LIB(LLVMExecutionEngine)
  FIND_AND_ADD_CLANG_LIB(LLVMRuntimeDyld)
  FIND_AND_ADD_CLANG_LIB(LLVMCodeGen)
  FIND_AND_ADD_CLANG_LIB(LLVMScalarOpts)
  FIND_AND_ADD_CLANG_LIB(LLVMProfileData)
  FIND_AND_ADD_CLANG_LIB(LLVMObject)
  FIND_AND_ADD_CLANG_LIB(LLVMMCParser)
  FIND_AND_ADD_CLANG_LIB(LLVMBitReader)
  FIND_AND_ADD_CLANG_LIB(LLVMInstCombine)
  FIND_AND_ADD_CLANG_LIB(LLVMTransformUtils)
  FIND_AND_ADD_CLANG_LIB(LLVMipa)
  FIND_AND_ADD_CLANG_LIB(LLVMAnalysis)
  FIND_AND_ADD_CLANG_LIB(LLVMTarget)
  FIND_AND_ADD_CLANG_LIB(LLVMMC)
  FIND_AND_ADD_CLANG_LIB(LLVMCore)
  FIND_AND_ADD_CLANG_LIB(LLVMSupport)
endif()

if(CLANG_LIBS)
  set(CLANG_FOUND TRUE)
else()
  message(STATUS "Could not find any Clang libraries in ${LLVM_LIBRARY_DIR}")
endif()

if(CLANG_FOUND)
  set(CLANG_LIBRARY_DIR ${LLVM_LIBRARY_DIR})
  set(CLANG_INCLUDE_DIR ${LLVM_INCLUDE_DIR})

  # check whether llvm-config comes from an install prefix
  execute_process(
    COMMAND ${LLVM_CONFIG_EXECUTABLE} --src-root
    OUTPUT_VARIABLE _llvmSourceRoot
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(FIND "${LLVM_INCLUDE_DIR}" "${_llvmSourceRoot}" _llvmIsInstalled)
  if (NOT _llvmIsInstalled)
    message(STATUS "Detected that llvm-config comes from a build-tree, adding includes from source dir")
    list(APPEND CLANG_INCLUDE_DIR "${_llvmSourceRoot}/tools/clang/include")
  endif()

  message(STATUS "Found Clang (LLVM version: ${LLVM_VERSION})")
  message(STATUS "  Include dirs:  ${CLANG_INCLUDE_DIR}")
  message(STATUS "  Libraries:     ${CLANG_LIBS}")
else()
  if(Clang_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find Clang")
  endif()
endif()
