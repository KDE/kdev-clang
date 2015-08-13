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
#include <QAction>
#include <QMenu>

// KF5
#include <KF5/KI18n/klocalizedstring.h>

// KDevelop
#include <interfaces/contextmenuextension.h>
#include <language/interfaces/editorcontext.h>

// Clang
#include <clang/Tooling/Core/Replacement.h>

#include "contextmenumutator.h"
#include "refactoringmanager.h"
#include "kdevrefactorings.h"
#include "actionwatcher.h"
#include "refactoringcontext.h"
#include "utils.h"

using namespace clang;
using namespace clang::tooling;
using namespace KDevelop;

ContextMenuMutator::ContextMenuMutator(ContextMenuExtension &extension, RefactoringManager *parent)
    : QObject(parent)
    , m_placeholder(new QAction(i18n("preparing list..."), this))
{
    extension.addAction(ContextMenuExtension::RefactorGroup, m_placeholder);
}

RefactoringManager *ContextMenuMutator::parent()
{
    return static_cast<RefactoringManager *>(QObject::parent());
}

// Create submenu for refactoring actions if necessary and possible
QWidget *ContextMenuMutator::menuForWidget(QWidget *widget)
{
    QMenu * const menu = dynamic_cast<QMenu *>(widget);
    if (menu) {
        int sectionSize = 0;
        bool found = false;
        for (QAction *a : menu->actions()) {
            if (!a->isSeparator()) {
                sectionSize++;
                if (a == m_placeholder) {
                    found = true;
                }
            } else {
                if (!found) {
                    sectionSize = 0;
                } else {
                    break;
                }
            }
        }
        Q_ASSERT(sectionSize > 0);
        if (sectionSize > 1) {
            // do nothing
            return widget;
        } else {
            // make submenu
            QMenu *submenu = new QMenu(i18n("Refactor"), menu);
            menu->insertMenu(m_placeholder, submenu);
            return submenu;
        }
    } else {
        // We can't create submenu
        return widget;
    }
}

void ContextMenuMutator::endFillingContextMenu(const QVector<Refactoring *> &refactorings)
{
    QList<QAction *> actions;
    auto ctx = parent()->parent()->refactoringContext();
    for (auto refactorAction : refactorings) {
        QAction *action = new QAction(refactorAction->name(), parent());
        refactorAction->setParent(action);  // delete as necessary
        connect(action, &QAction::triggered, [this, ctx, refactorAction]()
        {
            auto result = refactorAction->invoke(ctx);
            if (!result) {
                ctx->reportError(result.getError());
                return;
            }
            auto changes = toDocumentChangeSet(result.get(), ctx->cache,
                                               ctx->cache->refactoringTool().getFiles());
            if (!changes) {
                ctx->reportError(changes.getError());
            } else {
                changes.get().applyAllChanges();
            }
        });
        actions.push_back(action);
    }
    for (QWidget *w : m_placeholder->associatedWidgets()) {
        menuForWidget(w)->insertActions(m_placeholder, actions);
    }
    for (QAction *a : actions) {
        new ActionWatcher(a);
    }
    deleteLater();
}
