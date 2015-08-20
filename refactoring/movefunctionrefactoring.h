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

/**
 * Move static functions from one class to another.
 * Changes definition of A::method to B::method (references to A scope), generates friend
 * delcaration in A (to allow references from new B::method), changes usages (call B::method
 * instead of A::method).
 * Current implementation is unable to transform function definition from class definition (inline).
 * In such case manual fixes may be required.
 */
class MoveFunctionRefactoring : public Refactoring
{
    Q_OBJECT;
    Q_DISABLE_COPY(MoveFunctionRefactoring);

    class Callback;

public:
    explicit MoveFunctionRefactoring(const clang::CXXMethodDecl *decl,
                                     clang::ASTContext &astContext);

    virtual ResultType invoke(RefactoringContext *ctx) override;
    virtual QString name() const override;

    /**
     * Essence of this refactoring
     */
    ResultType doRefactoring(clang::tooling::RefactoringTool &tool,
                             const std::string &targetRecord);

private:
    std::string m_declaration;
    std::unique_ptr<DeclarationComparator> m_declDispatcher;
    bool m_declarationInSourceIsADefinition;
    bool m_isStatic;
};


#endif //KDEV_CLANG_MOVEFUNCTIONREFACTORING_H
