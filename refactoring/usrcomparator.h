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

#ifndef KDEV_CLANG_USRCOMPARATOR_H
#define KDEV_CLANG_USRCOMPARATOR_H

// Clang
#include <clang/AST/DeclBase.h>

#include "declarationcomparator.h"

/**
 * Comparator based on Clang Unified Symbol Resolution.
 * I expect this simple class to be "one to rule all" implementation of @c DeclarationComparator
 */
class UsrComparator : public DeclarationComparator
{
public:
    /**
     * Create instance. May fail (return @c nullptr) if @c clang::index::generateUSRForDecl refuse
     * to generate mangled name.
     */
    static std::unique_ptr<UsrComparator> create(const clang::Decl *decl);

    virtual bool equivalentTo(const clang::Decl *decl) const override;

private:
    UsrComparator();

private:
    llvm::SmallVector<char, 256> m_mangledName;
};


#endif //KDEV_CLANG_USRCOMPARATOR_H
