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

#include "interface.h"

class ClangSupport;

/**
 * Glue between KDevelop (QObject based) interfaces and refactorings (interface)
 */
class KDevRefactorings : public QObject
{
    Q_OBJECT;
    Q_DISABLE_COPY(KDevRefactorings);

public:
    explicit KDevRefactorings(ClangSupport *parent);

    void fillContextMenu(KDevelop::ContextMenuExtension &extension, KDevelop::Context *context);

    // TODO: Handle configuration of projects to regenerate CompilationDatabase
    // TODO: Handle above + changes in files (also creation) to update/regenerate RefactoringsContext

private: // (slots)
    // Only one project for now
    void projectOpened(KDevelop::IProject* project);
    void projectConfigured(KDevelop::IProject* project);

    // TODO: (async)
    void createRefactoringsContext();

private:
    Refactorings::RefactoringsContext m_refactoringsContext = nullptr;
};

#endif //BUILD_REFACTORINGS
#endif //KDEV_CLANG_REFACTORINGSGLUE_H
