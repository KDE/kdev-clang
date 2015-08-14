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

#ifndef KDEV_CLANG_REFACTORINGCONTEXT_H
#define KDEV_CLANG_REFACTORINGCONTEXT_H

// Qt
#include <QObject>
#include <QTimer>

// KF5
#include <KTextEditor/ktexteditor/cursor.h>

// LLVM
#include <llvm/Support/ErrorOr.h>

// Clang
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Refactoring.h>

namespace KDevelop
{
class IDocumentController;

class IProject;
};

class KDevRefactorings;

class DocumentCache;

// TODO: join with DocumentCache, handle CompilationDatabase here
class RefactoringContext : public QObject
{
    Q_OBJECT;
    Q_DISABLE_COPY(RefactoringContext);

    class Worker;

public:
    RefactoringContext(KDevRefactorings *parent);

    KDevRefactorings *parent();

    llvm::ErrorOr<unsigned> offset(const std::string &sourceFile,
                                   const KTextEditor::Cursor &position) const;

    void reportError(const QString &errorMessage);
    void reportError(const std::error_code &error);
    void reportInformation(const QString &information);

    template<typename Task, typename Callback>
    void schedule(Task task, Callback callback);
    template<typename Task, typename Callback>
    void scheduleOnSingleFile(Task task, const std::string &filename, Callback callback);
    // Convenience method on top of @c schedule
    clang::tooling::Replacements scheduleRefactoring(
        std::function<clang::tooling::Replacements(clang::tooling::RefactoringTool &)> task);

private: // (slots)
    // Only one project for now
    void projectOpened(KDevelop::IProject *project);
    void projectConfigured(KDevelop::IProject *project);

private slots:
    // Directly call callback on this thread
    void invokeCallback(std::function<void()> callback);

private:
    template<typename Task, typename Callback>
    static std::function<void(clang::tooling::RefactoringTool,
                              std::function<void(std::function<void()>)>)> composeTask(
        Task task, Callback callback);

public:
    std::unique_ptr<clang::tooling::CompilationDatabase> database;
    DocumentCache *cache;

private:
    Worker *m_worker;
};

Q_DECLARE_METATYPE(std::function<void()>);

template<typename Task, typename Callback>
std::function<void(clang::tooling::RefactoringTool,
                   std::function<void(std::function<void()>)>)> RefactoringContext::composeTask(
    Task task, Callback callback)
{
    return [task, callback](
        clang::tooling::RefactoringTool tool,
        std::function<void(std::function<void()>)> callbackScheduler)
    {
        auto result = task(tool);
        // By value! (TODO C++14: move result to lambda instead of copying)
        auto callbackInvoker = [callback, result]
        {
            callback(std::move(result));
        };
        callbackScheduler(callbackInvoker);
    };
};

#include "refactoringcontext_worker.h"

template<typename Task, typename Callback>
void RefactoringContext::schedule(Task task, Callback callback)
{
    auto composedTask = composeTask(task, callback);
    auto worker = m_worker;
#if(QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    QTimer::singleShot(0, m_worker, [worker, composedTask]
    {
        worker->invoke(composedTask);
    });
#else
    QMetaObject::invokeMethod(
        m_worker, "invoke",
        Q_ARG(std::function<void(clang::tooling::RefactoringTool & ,
                  std::function<void(std::function<void()>)>)>, composedTask));
#endif
};

template<typename Task, typename Callback>
void RefactoringContext::scheduleOnSingleFile(Task task, const std::string &filename,
                                              Callback callback)
{
    auto composedTask = composeTask(task, callback);
    auto worker = m_worker;
#if(QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    QTimer::singleShot(0, m_worker, [worker, composedTask, filename]
    {
        worker->invokeOnSingleFile(composedTask, filename);
    });
#else
    QMetaObject::invokeMethod(
        m_worker, "invokeOnSingleFile",
        Q_ARG(std::function<void(clang::tooling::RefactoringTool & ,
                  std::function<void(std::function<void()>)>)>, composedTask),
        Q_ARG(std::string, filename));
#endif
}


#endif //KDEV_CLANG_REFACTORINGCONTEXT_H
