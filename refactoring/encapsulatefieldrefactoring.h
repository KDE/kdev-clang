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

#ifndef KDEV_CLANG_ENCAPSULATEFIELDREFACTORING_H
#define KDEV_CLANG_ENCAPSULATEFIELDREFACTORING_H

// Clang
#include <clang/AST/DeclBase.h>

#include "refactoring.h"
#include "redeclarationchain.h"

class DeclarationComparator;

/**
 * Encapsulates field - changes access to "private", generates accessor (and optionally mutator) and
 * tries to adapt existing code to use of these new functions.
 */
class EncapsulateFieldRefactoring : public Refactoring
{
    Q_OBJECT;
    Q_DISABLE_COPY(EncapsulateFieldRefactoring);
public:
    class ChangePack;

    EncapsulateFieldRefactoring(const clang::DeclaratorDecl *decl, clang::ASTContext *astContext);
    ~EncapsulateFieldRefactoring();

    virtual ResultType invoke(RefactoringContext *ctx);
    virtual QString name() const;

private:
    std::unique_ptr<ChangePack> m_changePack;
    std::unique_ptr<DeclarationComparator> m_declDispatcher;
    std::unique_ptr<DeclarationComparator> m_recordDeclDispatcher;
    std::string m_recordName;
};

namespace Refactorings
{
namespace EncapsulateField
{
using ChangePack = EncapsulateFieldRefactoring::ChangePack;

/**
 * Essence of this refactoring, used from testing code
 */
int run(clang::tooling::RefactoringTool &tool, const ChangePack *changePack,
        const DeclarationComparator *declDispatcher,
        const DeclarationComparator *recordDeclDispatcher, const std::string &recordName);
}
}

#endif //KDEV_CLANG_ENCAPSULATEFIELDREFACTORING_H
