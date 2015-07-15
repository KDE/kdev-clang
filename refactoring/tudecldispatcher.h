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

#ifndef KDEV_CLANG_TUDECLDISPATCHER_H
#define KDEV_CLANG_TUDECLDISPATCHER_H

// C++
#include <memory>
#include <unordered_map>

// Clang
#include <clang/AST/DeclBase.h>

class DeclarationComparator;

/**
 * Helps in making decision if given declarations are syntactically equivalent
 */
class TUDeclDispatcher
{
public:
    TUDeclDispatcher(const DeclarationComparator *declComparator);

    bool equivalent(const clang::Decl *decl) const;

    template<class T>
    typename std::enable_if<std::is_base_of<clang::DeclContext, T>::value &&
                            (!std::is_base_of<clang::Decl, T>::value), bool>::type
    equivalent(const T *declContext) const
    {
        // required because there are classes which derive from both Decl and DeclContext
        return equivalentImpl(declContext);
    };

private:
    bool equivalentImpl(const clang::DeclContext *declContext) const;

private:
    const DeclarationComparator *m_declComparator;
    mutable std::unordered_map<const clang::Decl *, bool> m_cache;
};


#endif //KDEV_CLANG_TUDECLDISPATCHER_H
