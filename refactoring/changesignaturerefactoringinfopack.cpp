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

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

#include "changesignaturerefactoringinfopack.h"
#include "declarationcomparator.h"
#include "utils.h"

using namespace clang;

std::unique_ptr<const ChangeSignatureRefactoring::InfoPack>
ChangeSignatureRefactoring::InfoPack::fromFunctionDecl(const FunctionDecl *functionDecl,
                                                       ASTContext *astContext)
{
    return std::unique_ptr<const InfoPack>(new InfoPack(functionDecl, astContext));
}

ChangeSignatureRefactoring::InfoPack::InfoPack(const FunctionDecl *functionDecl,
                                               ASTContext *astContext)
    : m_declComparator(::declarationComparator(functionDecl))
{
    // We don't rename constructors, destructors, operators (but we may rename type)
    m_restricted = isa<CXXConstructorDecl>(functionDecl) ||
                   isa<CXXDestructorDecl>(functionDecl) ||
                   isa<CXXConversionDecl>(functionDecl) ||
                   functionDecl->isOverloadedOperator();
    m_functionName = functionDecl->getName();
    m_returnType = toString(functionDecl->getReturnType(), astContext->getLangOpts());
    for (auto p : functionDecl->params()) {
        m_parameters.emplace_back(toString(p->getType(), astContext->getLangOpts()), p->getName());
    }
}
