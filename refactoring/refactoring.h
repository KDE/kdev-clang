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

#ifndef KDEV_CLANG_REFACTORING_H
#define KDEV_CLANG_REFACTORING_H

#include "refactoringinfo.h"

// Qt
#include <QObject>
#include <QUrl>

// KF5
#include <KTextEditor/ktexteditor/cursor.h>

// Clang
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Refactoring.h>

#include "interface.h"

class DocumentCache;

/**
 * Wraps interface of single refactoring action. Objects will be used to perform refactoring action
 */
class Refactoring : public QObject, public RefactoringInfo
{
    Q_OBJECT;
    Q_DISABLE_COPY(Refactoring);
    // TODO: interface as needed
public:
    Refactoring(QObject *parent);

    virtual llvm::ErrorOr<clang::tooling::Replacements> invoke(RefactoringContext *ctx) = 0;
    // TODO: {Location,What} union

protected:
    static llvm::ErrorOr<clang::tooling::Replacements> cancelledResult();  // Returned on cancel
};


#endif //KDEV_CLANG_REFACTORING_H
