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

#ifndef KDEV_CLANG_REFACTORINGMANAGER_WORKER_H
#define KDEV_CLANG_REFACTORINGMANAGER_WORKER_H

// Clang
#include <clang/Tooling/Refactoring.h>

// Qt
#include <QThread>

#include "refactoringcontext.h"

/**
 * This is background worker for refactoring actions
 */
class RefactoringContext::Worker : public QThread
{
    Q_OBJECT;
    Q_DISABLE_COPY(Worker);
public:
    Worker(RefactoringContext *parent);

public slots:
    void invoke(std::function<void(clang::tooling::RefactoringTool &,
                                   std::function<void(std::function<void()>)>)> task);
    void invokeOnSingleFile(std::function<void(clang::tooling::RefactoringTool &,
                                               std::function<void(std::function<void()>)>)> task,
                            const std::string &filename);

signals:
    void taskFinished(std::function<void()> resultCallback);

private:
    RefactoringContext *m_parent;
};

#if(QT_VERSION < QT_VERSION_CHECK(5, 4, 0))
Q_DECLARE_METATYPE(std::function<void(clang::tooling::RefactoringTool & ,
                       std::function<void(std::function<void()>)>)>);
Q_DECLARE_METATYPE(std::string);
#endif

#endif //KDEV_CLANG_REFACTORINGMANAGER_WORKER_H
