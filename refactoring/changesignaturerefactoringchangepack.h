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

#ifndef KDEV_CLANG_CHANGESIGNATUREREFACTORING_CHANGEPACK_H
#define KDEV_CLANG_CHANGESIGNATUREREFACTORING_CHANGEPACK_H

// C++
#include <vector>

#include "changesignaturerefactoring.h"

/**
 * Wraps information about changes to be applied in Change Signature refactoring.
 *
 * @note This form of changeset preserves source information where possible
 */
class ChangeSignatureRefactoring::ChangePack
{
public:
    ChangePack(const InfoPack *infoPack);

    // non-negative value - param from InfoPack at given position
    // nagative value - param from m_newParam at (-v)-1 position
    std::vector<int> m_paramRefs;
    std::vector<std::tuple<std::string, std::string>> m_newParam;
    // TODO: real changing of existing parameters (preserve information)
    // These are empty if not changed
    std::string m_newResult;
    std::string m_newName;
};


#endif //KDEV_CLANG_CHANGESIGNATUREREFACTORING_CHANGEPACK_H
