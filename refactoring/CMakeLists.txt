
set(CMAKE_AUTOUIC ON)

set(SRCS
    utils.cpp
    documentcache.cpp
    refactoring.cpp
    refactoringinfo.cpp
    nooprefactoring.cpp
    kdevrefactorings.cpp
    renamevardeclrefactoring.cpp
    refactoringcontext.cpp
    refactoringmanager.cpp
    debug.cpp
    renamefielddeclrefactoring.cpp
    renamefielddeclturefactoring.cpp
    redeclarationchain.cpp
    declarationcomparator.cpp
    tudecldispatcher.cpp
    declarationsymbol.cpp
    changesignaturedialog.cpp
    changesignaturerefactoring.cpp
    changesignaturerefactoringinfopack.cpp
    changesignaturerefactoringchangepack.cpp
    encapsulatefieldrefactoring.cpp
    encapsulatefielddialog.cpp
    encapsulatefieldrefactoring_changepack.cpp
    refactoringcontext_worker.cpp
    contextmenumutator.cpp
    actionwatcher.cpp
)

add_library(kdevclangrefactor STATIC
    ${SRCS}
)

# TODO tests

target_link_libraries(kdevclangrefactor
LINK_PRIVATE
    KDev::Util
    KDev::Serialization
    KDev::Language
    KF5::TextEditor
    kdevclangutil   # For clangDebug()

    # This is really nasty...
    ${CLANG_CLANG_LIB}  # For kdevclangutil
    ${CLANG_CLANGTOOLING_LIB}
    ${CLANG_CLANGTOOLINGCORE_LIB}
    ${CLANG_CLANGFRONTEND_LIB}
    ${CLANG_CLANGDRIVER_LIB}
    ${CLANG_CLANGSERIALIZATION_LIB}
    ${CLANG_CLANGCODEGEN_LIB}
    ${CLANG_CLANGPARSE_LIB}
    ${CLANG_CLANGSEMA_LIB}
    ${CLANG_CLANGSTATICANALYZERFRONTEND_LIB}
    ${CLANG_CLANGSTATICANALYZERCHECKERS_LIB}
    ${CLANG_CLANGSTATICANALYZERCORE_LIB}
    ${CLANG_CLANGANALYSIS_LIB}
    ${CLANG_CLANGINDEX_LIB}
    ${CLANG_CLANGARCMIGRATE_LIB}
    ${CLANG_CLANGREWRITE_LIB}
    ${CLANG_CLANGREWRITEFRONTEND_LIB}
    ${CLANG_CLANGAST_LIB}
    ${CLANG_CLANGASTMATCHERS_LIB}
    ${CLANG_CLANGLEX_LIB}
    ${CLANG_CLANGBASIC_LIB}
    ${CLANG_LLVMLTO_LIB}
    ${CLANG_LLVMOBJCARCOPTS_LIB}
    ${CLANG_LLVMLINKER_LIB}
    ${CLANG_LLVMIPO_LIB}
    ${CLANG_LLVMVECTORIZE_LIB}
    ${CLANG_LLVMBITWRITER_LIB}
    ${CLANG_LLVMCPPBACKENDCODEGEN_LIB}
    ${CLANG_LLVMCPPBACKENDINFO_LIB}
    ${CLANG_LLVMTABLEGEN_LIB}
    ${CLANG_LLVMDEBUGINFO_LIB}
    ${CLANG_LLVMOPTION_LIB}
    ${CLANG_LLVMX86DISASSEMBLER_LIB}
    ${CLANG_LLVMX86ASMPARSER_LIB}
    ${CLANG_LLVMX86CODEGEN_LIB}
    ${CLANG_LLVMSELECTIONDAG_LIB}
    ${CLANG_LLVMASMPRINTER_LIB}
    ${CLANG_LLVMX86DESC_LIB}
    ${CLANG_LLVMMCDISASSEMBLER_LIB}
    ${CLANG_LLVMX86INFO_LIB}
    ${CLANG_LLVMX86ASMPRINTER_LIB}
    ${CLANG_LLVMX86UTILS_LIB}
    ${CLANG_LLVMMCJIT_LIB}
    ${CLANG_LLVMIRREADER_LIB}
    ${CLANG_LLVMASMPARSER_LIB}
    ${CLANG_LLVMLINEEDITOR_LIB}
    ${CLANG_LLVMINSTRUMENTATION_LIB}
    ${CLANG_LLVMINTERPRETER_LIB}
    ${CLANG_LLVMEXECUTIONENGINE_LIB}
    ${CLANG_LLVMRUNTIMEDYLD_LIB}
    ${CLANG_LLVMCODEGEN_LIB}
    ${CLANG_LLVMSCALAROPTS_LIB}
    ${CLANG_LLVMPROFILEDATA_LIB}
    ${CLANG_LLVMOBJECT_LIB}
    ${CLANG_LLVMMCPARSER_LIB}
    ${CLANG_LLVMBITREADER_LIB}
    ${CLANG_LLVMINSTCOMBINE_LIB}
    ${CLANG_LLVMTRANSFORMUTILS_LIB}
    ${CLANG_LLVMIPA_LIB}
    ${CLANG_LLVMANALYSIS_LIB}
    ${CLANG_LLVMTARGET_LIB}
    ${CLANG_LLVMMC_LIB}
    ${CLANG_LLVMCORE_LIB}
    ${CLANG_LLVMSUPPORT_LIB}
    ${CLANG_CLANGEDIT_LIB}
    ${CLANG_CLANGLEX_LIB}
    # A few more dependencies of Clang (TODO: cleanup)
    pthread # it must be somewhere around...
    z       # known as zlib
    ${CMAKE_DL_LIBS}
    ncurses # in fact tinfo (part of ncurses, can be built separately, TODO: handle that)
)

#set_target_properties(kdevclangrefactor PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
