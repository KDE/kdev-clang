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

#ifndef KDEV_CLANG_REFACTORINGMANAGER_H
#define KDEV_CLANG_REFACTORINGMANAGER_H

// Qt
#include <QObject>
#include <QVector>

// KDevelop
#include <interfaces/contextmenuextension.h>
#include <language/interfaces/editorcontext.h>

// C++
#include <memory>

#include "refactoring.h"

class KDevRefactorings;

class RefactoringManager : public QObject
{
    Q_OBJECT;
    Q_DISABLE_COPY(RefactoringManager);

public:
    RefactoringManager(KDevRefactorings *parent);

    void fillContextMenu(KDevelop::ContextMenuExtension &extension,
                         KDevelop::EditorContext *context);

    KDevRefactorings *parent();

private:

};

// Manager workaround for single position
QVector<Refactoring *> refactoringsFor(const std::string &filename, unsigned offset,
                                       QThread *targetThread,
                                       clang::tooling::RefactoringTool &tool);

// Manager workaround for range
QVector<Refactoring *> refactoringsFor(const std::string &filename, unsigned offsetBegin,
                                       unsigned offsetEnd, QThread *targetThread,
                                       clang::tooling::RefactoringTool &tool);

Q_DECLARE_METATYPE(QVector<Refactoring *>);

#endif //KDEV_CLANG_REFACTORINGMANAGER_H
