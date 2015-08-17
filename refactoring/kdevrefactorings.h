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

#ifndef KDEV_CLANG_REFACTORINGSGLUE_H
#define KDEV_CLANG_REFACTORINGSGLUE_H

#ifndef BUILD_REFACTORINGS
#include "kdevrefactorings_disabled.h"
#else

// Qt
#include <QObject>

// KDevelop
#include <interfaces/contextmenuextension.h>
#include <interfaces/context.h>
#include <interfaces/iproject.h>

class ClangSupport;
class RefactoringContext;
class RefactoringManager;

/**
 * Glue between KDevelop interfaces and refactorings implementation.
 * It exists to separate refactorings (requiring Clang and quite fixed environment) and KDevelop.
 *
 * This is "active" version - maintains two main object of refactoring implementation:
 * @c RefactoringContext and @c RefactoringManager. Forwards selected API requests to underlying
 * objects. (currently only @c fillContextMenu)
 */
class KDevRefactorings : public QObject
{
    Q_OBJECT;
    Q_DISABLE_COPY(KDevRefactorings);

public:
    explicit KDevRefactorings(ClangSupport *parent);

    ClangSupport *parent();

    /**
     * Hooked into @fillContextMenu of general @c IPlugin interface (in @c ClangSupport).
     * Its task is to populate context menu with some content (in this case possible refactoring
     * operations)
     */
    void fillContextMenu(KDevelop::ContextMenuExtension &extension, KDevelop::Context *context);

    RefactoringContext *refactoringContext()
    {
        return m_refactoringsContext;
    }

private:
    RefactoringContext * const m_refactoringsContext;
    RefactoringManager * const m_refactoringManager;
};

#endif //BUILD_REFACTORINGS
#endif //KDEV_CLANG_REFACTORINGSGLUE_H
