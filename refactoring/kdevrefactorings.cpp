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

#include "kdevrefactorings.h"

// Qt
#include <QAction>
#include <QTimer>

// KF5
#include <KJob>

// KDevelop
#include <language/interfaces/editorcontext.h>
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <project/interfaces/ibuildsystemmanager.h>
#include <project/interfaces/iprojectbuilder.h>
#include <project/projectmodel.h>

#include "interface.h"
#include "refactoringmanager.h"
#include "debug.h"

#include "../clangsupport.h"


using namespace KDevelop;
using namespace Refactorings;

KDevRefactorings::KDevRefactorings(ClangSupport *parent)
        : QObject(parent)
{
    m_refactoringManager = new RefactoringManager(this);
    connect(parent->core()->projectController(), &IProjectController::projectOpened,
            this, &KDevRefactorings::projectOpened);
    // handle projects() - alread opened projects

}

void KDevRefactorings::fillContextMenu(ContextMenuExtension &extension, Context *context)
{
    if (EditorContext *ctx = dynamic_cast<EditorContext *>(context)) {
        // NOTE: I assume ctx->positions() is "here" position
        auto refactorings = m_refactoringManager->allApplicableRefactorings(m_refactoringsContext,
                                                                            ctx->url(),
                                                                            ctx->position());
        for (auto refactorAction : refactorings) {
            QAction *action = new QAction(refactorAction->name(), this);
            refactorAction->setParent(action);  // delete as necessary
            connect(action, &QAction::triggered, [this, refactorAction, ctx]()
            {
                // TODO: don't use refactorThis
                auto changes = refactorThis(m_refactoringsContext, refactorAction,
                                            ctx->url(),
                                            ctx->position());
                // FIXME:
                // use background thread
                // ... provided with ability to show GUI
                // show busy indicator

                changes.applyAllChanges();
                // NOTE: cache of ClangTool and FileSystem must be updated after application to
                // reflect changes in files
            });

            extension.addAction(ContextMenuExtension::RefactorGroup, action);
        }
    } else {
        // I assume the above works anytime we ask for context menu for code
        Q_ASSERT(!context->hasType(Context::CodeContext));
    }
}

void KDevRefactorings::projectOpened(IProject *project)
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

void KDevRefactorings::projectConfigured(IProject *project)
{
    Q_ASSERT(project);
    refactorDebug() << project->name() << "configured";
    auto buildSystemManager = project->buildSystemManager();
    Path buildPath = buildSystemManager->buildDirectory(project->projectItem());
    refactorDebug() << "build path:" << buildPath;

    // FIXME: do it async (in background)
    // FIXME: handle non-CMake project
    QString errorMessage;
    auto compilationDatabase = Refactorings::createCompilationDatabase(
            buildPath.toLocalFile().toStdString(), Refactorings::ProjectKind::CMAKE, errorMessage);
    if (!compilationDatabase) {
        // TODO: show message for that
        refactorDebug() << "Cannot create compilation database for" << project->name() << ":" <<
                        errorMessage;
        return;
    }
    auto refactoringsContext = Refactorings::createRefactoringsContext(compilationDatabase);
    Q_ASSERT(refactoringsContext);
    m_refactoringsContext = refactoringsContext;
    refactorDebug() << "RefactoringsContext sucessfully (re)generated!";
}
