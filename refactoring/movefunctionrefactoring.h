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

#ifndef KDEV_CLANG_MOVEFUNCTIONREFACTORING_H
#define KDEV_CLANG_MOVEFUNCTIONREFACTORING_H

#include "refactoring.h"
#include "redeclarationchain.h"

/** change definition a::method to b::method (also change nested name spec)
 *  generate friend declaration
 *  change usages
 */
class MoveFunctionRefactoring : public Refactoring
{
    Q_OBJECT;
    Q_DISABLE_COPY(MoveFunctionRefactoring);

    class Callback;

public:
    explicit MoveFunctionRefactoring(const clang::CXXMethodDecl *decl,
                                     clang::ASTContext &astContext);

    virtual llvm::ErrorOr<clang::tooling::Replacements> invoke(RefactoringContext *ctx) override;
    virtual QString name() const override;

    clang::tooling::Replacements doRefactoring(clang::tooling::RefactoringTool &tool,
                                               const std::string &targetRecord);

private:
    std::string m_declaration;
    RedeclarationChain m_declDispatcher;
    bool m_declarationInSourceIsADefinition;
    bool m_isStatic;
};


#endif //KDEV_CLANG_MOVEFUNCTIONREFACTORING_H
