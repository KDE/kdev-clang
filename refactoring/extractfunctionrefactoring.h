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

#ifndef KDEV_CLANG_EXTRACTFUNCTIONREFACTORING_H
#define KDEV_CLANG_EXTRACTFUNCTIONREFACTORING_H

// Clang
#include <clang/AST/Expr.h>
#include <clang/Tooling/Core/Replacement.h>

#include "refactoring.h"

/**
 * Extract function from selected expression. Can embed new function only in DeclContext
 * (places in which declaration may appear). This is "local refactoring" - changes are made only
 * in one source file, immediately after @c invoke.
 */
class ExtractFunctionRefactoring : public Refactoring
{
    Q_OBJECT;

    struct Task;

public:
    virtual ResultType invoke(RefactoringContext *ctx);
    virtual QString name() const;

    clang::tooling::Replacements doRefactoring(const std::string &name);

    static ExtractFunctionRefactoring *create(const clang::Expr *expr,
                                              clang::ASTContext *astContext,
                                              clang::SourceManager *sourceManager);

private:
    ExtractFunctionRefactoring(std::vector<Task> tasks);

private:
    struct Task
    {
        std::string filename;
        unsigned offset;
        unsigned length;
        std::function<std::string(const std::string &)> replacement;

        Task(const std::string &filename, unsigned offset, unsigned length,
             const std::function<std::string(const std::string &)> &replacement);
    };

    std::vector<Task> m_tasks;
};

#endif //KDEV_CLANG_EXTRACTFUNCTIONREFACTORING_H
