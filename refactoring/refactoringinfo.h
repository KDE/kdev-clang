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

#ifndef KDEV_CLANG_REFACTORINGINFO_H
#define KDEV_CLANG_REFACTORINGINFO_H

//Qt
#include <QString>

/**
 * This interface can be used to obtain basic information about some particular Refactoring action
 * implemented by this tool.
 *
 * This interface is designed to be used by UI builder to display user readable information
 * and optionally to create instance of @c Refactoring.
 *
 * @note Currently @c Refactoring derives from this interface. It may be considered in future
 * to add builder method to this interface returning instance of @c Refactoring instead.
 */
class RefactoringInfo
{
public:
    virtual ~RefactoringInfo();

    /**
     * Returns short human readable "name" of a refactoring action.
     * Can be used to populate context-menu...
     *
     * @return Short human readable description
     */
    virtual QString name() const = 0;
};


#endif //KDEV_CLANG_REFACTORINGINFO_H
