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

// KDevelop
#include <language/interfaces/editorcontext.h>

#include "kdevrefactorings.h"
#include "refactoringcontext.h"
#include "refactoringmanager.h"

#include "../clangsupport.h"


using namespace KDevelop;

KDevRefactorings::KDevRefactorings(ClangSupport *parent)
    : QObject(parent)
    , m_refactoringsContext(new RefactoringContext(this))
    , m_refactoringManager(new RefactoringManager(this))
{
}

ClangSupport *KDevRefactorings::parent()
{
    return static_cast<ClangSupport *>(QObject::parent());
}

void KDevRefactorings::fillContextMenu(ContextMenuExtension &extension, Context *context)
{
    if (EditorContext *ctx = dynamic_cast<EditorContext *>(context)) {
        if (m_refactoringsContext->isInitialized()) {
            m_refactoringManager->fillContextMenu(extension, ctx);
        }
    } else {
        // I assume the above works anytime we ask for context menu for code
        Q_ASSERT(!context->hasType(Context::CodeContext));
    }
}
