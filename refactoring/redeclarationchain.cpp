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

// Clang
#include <clang/AST/Decl.h>

#include "redeclarationchain.h"

using namespace clang;

RedeclarationChain::RedeclarationChain(const clang::Decl *declNode)
{
    Q_ASSERT((llvm::dyn_cast<NamedDecl>(declNode) == nullptr) ||
             (llvm::dyn_cast<NamedDecl>(declNode)->getLinkageInternal() != ExternalLinkage));
    for (auto decl : declNode->redecls()) {
        auto loc = lexicalLocation(decl);
        if (loc.fileName.empty()) {
            continue;
        }
        m_declarationsLocations.insert(std::move(loc));
    }
}

bool RedeclarationChain::intersects(const RedeclarationChain &other) const
{
    for (auto loc : m_declarationsLocations) {
        if (other.m_declarationsLocations.find(loc) != other.m_declarationsLocations.end()) {
            return true;
        }
    }
    return false;
}

bool RedeclarationChain::equivalentTo(const Decl *decl) const
{
    if (const NamedDecl *namedDecl = llvm::dyn_cast<NamedDecl>(decl)) {
        auto linkage = namedDecl->getLinkageInternal();
        if (linkage == ExternalLinkage) {
            return false;
        }
    }
    return intersects(RedeclarationChain(decl));
}
