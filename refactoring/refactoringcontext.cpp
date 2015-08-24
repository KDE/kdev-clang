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

// Qt
#include <QMessageBox>

// KF5
#include <KJob>
#include <KLocalizedString>

// KDevelop
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <project/interfaces/ibuildsystemmanager.h>
#include <project/interfaces/iprojectbuilder.h>
#include <project/projectmodel.h>

#include "refactoringcontext.h"
#include "kdevrefactorings.h"
#include "documentcache.h"
#include "refactoringcontext_worker.h"
#include "refactoring.h"
#include "utils.h"
#include "debug.h"

using namespace KDevelop;
using namespace clang;
using namespace clang::tooling;

RefactoringContext::RefactoringContext(KDevRefactorings *parent)
    : QObject(parent)
{
    qRegisterMetaType<std::function<void()>>();
    cache = new DocumentCache(this);
    m_worker = new Worker(this);

    connect(m_worker, &Worker::taskFinished, this, &RefactoringContext::invokeCallback);
    // Will not call-back if this RefactoringContext have been destroyed concurrently

    connect(ICore::self()->projectController(), &IProjectController::projectOpened,
            this, &RefactoringContext::projectOpened);
    // handle projects() - alread opened projects

    m_worker->start();
}

KDevRefactorings *RefactoringContext::parent()
{
    return static_cast<KDevRefactorings *>(QObject::parent());
}

bool RefactoringContext::isInitialized() const
{
    return static_cast<bool>(database);
}

llvm::ErrorOr<unsigned> RefactoringContext::offset(const std::string &sourceFile,
                                                   const KTextEditor::Cursor &position) const
{
    return toOffset(sourceFile, position, cache->refactoringTool(), cache);
}

void RefactoringContext::reportError(const QString &errorMessage)
{
    QMessageBox::warning(nullptr, i18n("Error Occurred"), errorMessage);
}

void RefactoringContext::reportError(const std::error_code &error)
{
    reportError(QString::fromStdString(error.message()));
}

void RefactoringContext::reportInformation(const QString &information)
{
    QMessageBox::information(nullptr, i18n("Information"), information);
}

void RefactoringContext::projectOpened(IProject *project)
{
    Q_ASSERT(project);
    refactorDebug() << project->name() << "opened";
    auto projectBuilder = project->buildSystemManager()->builder();
    // IProjectBuilder declares signal configured(), but is unused...

    // FIXME: configure only when necessary (and always when necessary)
    auto configureJob = projectBuilder->configure(project);
    connect(configureJob, &KJob::result, this, [this, project]()
    {
        projectConfigured(project);
    });
    configureJob->start();
    // wait for above connection to trigger further actions
}

void RefactoringContext::projectConfigured(IProject *project)
{
    Q_ASSERT(project);
    refactorDebug() << project->name() << "configured";
    auto buildSystemManager = project->buildSystemManager();
    Path buildPath = buildSystemManager->buildDirectory(project->projectItem());
    refactorDebug() << "build path:" << buildPath;

    // FIXME: do it async (in background)
    // FIXME: handle non-CMake project
    QString errorMessage;
    database = makeCompilationDatabaseFromCMake(buildPath.toLocalFile().toStdString(),
                                                errorMessage);
    if (!database) {
        // TODO: show message for that
        refactorDebug() << "Cannot create compilation database for" << project->name() << ":" <<
                        errorMessage;
        return;
    }
    refactorDebug() << "RefactoringsContext sucessfully (re)generated!";
}

Replacements RefactoringContext::scheduleRefactoring(
    std::function<Replacements(RefactoringTool &)> task)
{
    auto newTask = [task](RefactoringTool &tool) -> llvm::ErrorOr<Replacements>
    {
        return task(tool);
    };
    auto result = scheduleRefactoringWithError(newTask);
    return result.get();
}

llvm::ErrorOr<Replacements> RefactoringContext::scheduleRefactoringWithError(
    std::function<llvm::ErrorOr<Replacements>(RefactoringTool &)> task)
{
    QDialog *uiLocker = Refactoring::newBusyDialog();
    llvm::ErrorOr<Replacements> result = Replacements{};
    schedule(task, Refactoring::uiLockerCallback(uiLocker, result));
    uiLocker->exec();
    return result;
}

void RefactoringContext::invokeCallback(std::function<void()> callback)
{
    callback();
}
