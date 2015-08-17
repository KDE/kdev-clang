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

#ifndef KDEV_CLANG_DECLARATIONCOMPARATOR_H
#define KDEV_CLANG_DECLARATIONCOMPARATOR_H

// Clang
#include <clang/AST/DeclBase.h>

/**
 * Interface of comparators capable of deciding whether two declarations refer to the same entity.
 *
 * @note clang-c provides clang_getCursorUSR which does internally name mangling. It could be used
 * to build general purpose comparator provided that it is possible to get to its implementation
 * somehow (C++ implementation is in anonymous namespace but it is likely that Decl* can be
 * "converted" somehow to something similar to cursor (cxcursor::))
 */
class DeclarationComparator
{
public:
    virtual ~DeclarationComparator();

    /**
     * Compares to given @p decl. Returns true if @p decl refer to entity denoted by this comparator
     */
    virtual bool equivalentTo(const clang::Decl *decl) const = 0;
};


#endif //KDEV_CLANG_DECLARATIONCOMPARATOR_H
