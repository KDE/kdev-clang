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
#include <clang/Tooling/Refactoring.h>

// refactoring
#include "documentcache.h"

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

#endif //KDEV_CLANG_UTILS_H
