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

#include "refactoringcontext.h" // NOTE: we have circular dependency here
// Above must be included before...
#include "refactoringcontext_worker.h"
#include "refactoring.h"
#include "refactoringmanager.h"
#include "documentcache.h"

RefactoringContext::Worker::Worker(
    RefactoringContext *refactoringContext)
    : QThread(nullptr)
    , m_parent(refactoringContext)
{
#if(QT_VERSION < QT_VERSION_CHECK(5, 4, 0))
    qRegisterMetaType<std::function<void(clang::tooling::RefactoringTool &,
                                         std::function<void(std::function<void()>)>)>>();
    qRegisterMetaType<std::string>();
#endif
    moveToThread(this);
    setObjectName("RefactoringContext - Worker");
    connect(m_parent, &QObject::destroyed, this, [this]
    {
        m_parent = nullptr;
        exit();
    });
}

void RefactoringContext::Worker::invoke(
    std::function<void(clang::tooling::RefactoringTool &,
                       std::function<void(std::function<void()>)>)> task)
{
    task(m_parent->cache->refactoringTool(), [this](std::function<void()> resultCallback)
    {
        emit taskFinished(resultCallback);
    });
}

void RefactoringContext::Worker::invokeOnSingleFile(
    std::function<void(clang::tooling::RefactoringTool &,
                       std::function<void(std::function<void()>)>)> task,
    const std::string &filename)
{
    auto tool = m_parent->cache->refactoringToolForFile(filename);
    task(tool, [this](std::function<void()> resultCallback)
    {
        emit taskFinished(resultCallback);
    });
}
