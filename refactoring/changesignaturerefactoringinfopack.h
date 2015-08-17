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

#ifndef KDEV_CLANG_CHANGESIGNATUREREFACTORING_INFOPACK_H
#define KDEV_CLANG_CHANGESIGNATUREREFACTORING_INFOPACK_H

// C++
#include <string>

// Clang
#include <clang/AST/DeclBase.h>

#include "changesignaturerefactoring.h"
#include "declarationcomparator.h"

class DeclarationComparator;

/**
 * Wraps information about function signature in Change Signature refactoring.
 */
class ChangeSignatureRefactoring::InfoPack
{
public:
    static std::unique_ptr<const InfoPack> fromFunctionDecl(
        const clang::FunctionDecl *functionDecl);

    const std::string &functionName() const
    {
        return m_functionName;
    }

    const std::string &returnType() const
    {
        return m_returnType;
    }

    bool isRestricted() const
    {
        return m_restricted;
    }

    const std::vector<std::tuple<std::string, std::string>> &parameters() const
    {
        return m_parameters;
    }

    const DeclarationComparator &declarationComparator() const
    {
        return *m_declComparator;
    }

private:
    InfoPack(const clang::FunctionDecl *functionDecl);

private:
    std::string m_functionName;
    std::string m_returnType;
    bool m_restricted;  // constructor, destructor, operator
    std::vector<std::tuple<std::string, std::string>> m_parameters;
    // TODO: we also need locations ?
    std::unique_ptr<DeclarationComparator> m_declComparator;
};

#endif //KDEV_CLANG_CHANGESIGNATUREREFACTORING_INFOPACK_H
