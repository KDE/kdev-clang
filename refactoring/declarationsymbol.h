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

#ifndef KDEV_CLANG_DECLARATIONSYMBOL_H
#define KDEV_CLANG_DECLARATIONSYMBOL_H

// C++
#include <vector>

// Qt
#include <QtGlobal>

// Clang
#include <clang/AST/DeclBase.h>

#include "declarationcomparator.h"

class SymbolPart;

/**
 * Contains syntactic symbol declaration. Designed to operate on symbols with External Linkage.
 * Can be used everywhere (for all kinds of symbols, not only external), but is slow and its
 * behavior depends on rewrite of scoping and redeclaration rules.
 *
 * Try to keep in sync with C++ std
 */
class DeclarationSymbol : public DeclarationComparator
{
    Q_DISABLE_COPY(DeclarationSymbol);
public:
    // But for now only named decls can be marshalled this way
    // (external linkage of symbol without a name? no way, i guess)
    DeclarationSymbol(const clang::NamedDecl *decl);

    ~DeclarationSymbol();

    virtual bool equivalentTo(const clang::Decl *decl) const override;

private:
    std::vector<std::unique_ptr<SymbolPart>> m_parts;
};


#endif //KDEV_CLANG_DECLARATIONSYMBOL_H
