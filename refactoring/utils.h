/*
    This file is part of KDevelop

    Copyright 2015 Maciej Poleski <d82ks8djf82msd83hf8sc02lqb5gh5@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KDEV_CLANG_UTILS_H
#define KDEV_CLANG_UTILS_H

// C++ std
#include <memory>
#include <unordered_map>

// Clang
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Core/Replacement.h>

// KDevelop
#include <language/codegen/documentchangeset.h>
#include <language/util/kdevhash.h>

// refactoring
#include "documentcache.h"

class DeclarationComparator;

namespace cpp
{
// Use std::make_unique instead of this in C++14
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args &&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
};

std::unique_ptr<clang::tooling::CompilationDatabase> makeCompilationDatabaseFromCMake(
    const std::string &buildPath, QString &errorMessage);

std::unique_ptr<clang::tooling::RefactoringTool> makeRefactoringTool(
    const clang::tooling::CompilationDatabase &database,
    const std::vector<std::string> &sources);

llvm::ErrorOr<KDevelop::DocumentChangeSet> toDocumentChangeSet(
    const clang::tooling::Replacements &replacements,
    DocumentCache *cache,
    clang::FileManager &fileManager
);

llvm::ErrorOr<unsigned> toOffset(const std::string &sourceFileName,
                                 const KTextEditor::Cursor &position,
                                 clang::tooling::ClangTool &clangTool,
                                 DocumentCache *documentCache);

/**
 * Check if @p location is in range [start;end]
 */
bool isInRange(const std::string &fileName, unsigned offset, clang::SourceLocation start,
               clang::SourceLocation end, const clang::SourceManager &sourceManager);

bool isInRange(const std::string &fileName, unsigned offset, clang::SourceRange range,
               const clang::SourceManager &sourceManager);

clang::SourceRange tokenRangeToCharRange(clang::SourceRange range,
                                         const clang::SourceManager &sourceManager,
                                         const clang::LangOptions &langOptions);

clang::SourceRange tokenRangeToCharRange(clang::SourceRange range,
                                         const clang::CompilerInstance &CI);

// NOTE: @p offset must be equal to location offset
bool isLocationEqual(const std::string &fileName, unsigned offset, clang::SourceLocation location,
                     const clang::SourceManager &sourceManager);

/* comparison of declarations within translation unit
 * - chain of redeclarations (their locations)
 * - if two chains intersects - the same entity (declarator)
 */

/* comparison of declarations with external linkage (global)
 * - like name mangling (templates!!!)
 * - namespaces (also anonymous)
 * - classes (RecordDecl) (also anonymous)
 * - function (full signature)
 * - types (canonical, as function parameters)
 * - templates (classes, functions)
 * - specializations
 */

/* how to compare:
 * within translation unit:
 * - create chain of locations and for each declaration from each translation unit check if it
 *   intersects (maybe optimize - cache from canonical declaration to chain and cache checked
 *   canonical declarations)
 * global (external non-unique linkage):
 * - generate identifier of selected (canonical?) declaration
 * - compare identifiers
 */

/* anonymous namespaces
 * - in header file - used in many translation units
 *   - separate entities, but lexicaly the same location and the same usages
 */

/* external linkage requires global view
 * the rest (entities) are visible only in translation unit
 *   but may be lexicaly used in many translation units (header files)
 */

/* ALGO:
 * 0. generate chain (non external) or identifier (external linkage)
 * We have TU:
 * 1. visit all declarations (usages included)
 * 1.1. check correspondence with chain (or identifier)
 */

// TU dispatcher - keeps chain/identifier and cache of seen declarations (canonical)

/**
 * Stable information about location of (begin of) some token
 */
struct LexicalLocation
{
    std::string fileName;
    unsigned offset;
};

bool operator==(const LexicalLocation &lhs, const LexicalLocation &rhs);

namespace std
{
template<>
struct hash<LexicalLocation>
{
    size_t operator()(const LexicalLocation &o) const
    {
        auto h1 = hash<string>()(o.fileName);
        auto h2 = hash<unsigned>()(o.offset);
        return KDevHash() << h1 << h2;
    }
};
}

LexicalLocation lexicalLocation(const clang::Decl *decl);

std::unique_ptr<DeclarationComparator> declarationComparator(const clang::Decl *decl);

int getTokenRangeSize(const clang::SourceRange &range, const clang::SourceManager &sourceManager,
                      const clang::LangOptions &langOpts);

std::string textFromTokenRange(clang::SourceRange range, const clang::SourceManager &sourceManager,
                               const clang::LangOptions &langOpts);

// Clang provides absolute no support for moving code...
template<class Node>
std::string codeFromASTNode(const Node *node, const clang::SourceManager &sourceManager,
                            const clang::LangOptions &langOpts);

// AST locations discovery tool...
void dumpTokenRange(clang::SourceRange range, const clang::SourceManager &sourceManager,
                    const clang::LangOptions &langOpts);

template<class Node>
std::string codeFromASTNode(const Node *node, const clang::SourceManager &sourceManager,
                            const clang::LangOptions &langOpts)
{
    return textFromTokenRange(node->getSourceRange(), sourceManager, langOpts);
}

std::string suggestGetterName(const std::string &fieldName);

std::string suggestSetterName(const std::string &fieldName);

std::string functionName(const std::string &functionDeclaration, const std::string &fallbackName);

std::string toString(clang::QualType type, const clang::LangOptions &langOpts);

#endif //KDEV_CLANG_UTILS_H
