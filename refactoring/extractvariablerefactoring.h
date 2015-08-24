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

#ifndef KDEV_CLANG_EXTRACTVARIABLEREFACTORING_H
#define KDEV_CLANG_EXTRACTVARIABLEREFACTORING_H

// Clang
#include <clang/AST/Stmt.h>
#include <clang/Tooling/Core/Replacement.h>

#include "refactoring.h"

/**
 * Extract variable from selected expression. Can embed new variable only in compound statements
 * (blocks). This is "local refactoring" - changes are made only in one source file, immediately
 * after @c invoke.
 */
class ExtractVariableRefactoring : public Refactoring
{
public:
    ExtractVariableRefactoring(const clang::Expr *expr, clang::ASTContext *astContext,
                               clang::SourceManager *sourceManager);

    virtual ResultType invoke(RefactoringContext *ctx) override;
    virtual QString name() const override;

    /**
     * Essence of this refactoring, used from testing code
     */
    clang::tooling::Replacements doRefactoring(const std::string &name);

private:
    std::string m_filenameExpression; // final
    std::string m_expression; // final
    std::string m_filenameVariablePlacement; // final
    std::string m_variableType; // final
    unsigned m_offsetExpression; // final
    unsigned m_lengthExpression; // final
    unsigned m_offsetVariablePlacement; // final
};

namespace Refactorings
{
namespace ExtractVariable
{
/**
 * Used from testing code
 */
clang::tooling::Replacements run(const std::string &filenameExpression,
                                 const std::string &expression,
                                 const std::string &filenameVariablePlacement,
                                 const std::string &variableType, const std::string &variableName,
                                 const unsigned offsetExpression,
                                 const unsigned lengthExpression,
                                 const unsigned offsetVariablePlacement);
}
}


#endif //KDEV_CLANG_EXTRACTVARIABLEREFACTORING_H
