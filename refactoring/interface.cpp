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

#include "interface.h"

// KF5
#include <KI18n/klocalizedstring.h>

// Clang
#include <clang/Tooling/Refactoring.h>

#include "utils.h"
#include "refactoringcontext.h"
#include "documentcache.h"
#include "refactoringinfo.h"

#include "nooprefactoring.h"
#include "renamevardeclrefactoring.h"

#include "../util/clangdebug.h"

namespace Refactorings
{

//////////////////////// Compilation Database

// Maybe skip this opaque pointer? (but would leak details...)
struct CompilationDatabase_t
{
    std::unique_ptr<clang::tooling::CompilationDatabase> database;
};

CompilationDatabase createCompilationDatabase(const std::string &buildPath, ProjectKind kind,
                                              QString &errorMessage)
{
    if (kind != ProjectKind::CMAKE) {
        errorMessage = i18n("Only CMake projects are supported for now");
        return nullptr;
    }
    auto result = makeCompilationDatabaseFromCMake(buildPath, errorMessage);
    if (result == nullptr) {
        return nullptr;
    }
    return new CompilationDatabase_t{std::move(result)};
};

CompilationDatabase createCMakeCompilationDatabase(const std::string &buildPath)
{
    QString error;  // ignored, required by Clang API
    auto cd = makeCompilationDatabaseFromCMake(buildPath, error);
    if (cd == nullptr) {
        return nullptr;
    } else {
        return new CompilationDatabase_t{std::move(cd)};
    }
}

void releaseCompilationDatabase(CompilationDatabase db)
{
    delete db;
}

///////////////// Refactoring Context

RefactoringsContext createRefactoringsContext(CompilationDatabase db)
{
    auto result = new RefactoringContext(std::move(db->database));
    releaseCompilationDatabase(db);
    return result;
}

KDevelop::DocumentChangeSet refactorThis(RefactoringsContext rc, RefactoringKind refactoringKind,
                                         const QUrl &sourceFile,
                                         const KTextEditor::Cursor &position)
{
    Q_ASSERT(rc);
    Refactoring *refactoring = dynamic_cast<Refactoring *>(refactoringKind);
    Q_ASSERT(refactoring);
    // FIXME: introduce RefactoringManager to maintain pool of refactorings and such translations

    auto result = refactoring->invoke(rc);
    // TODO: introduce union type for {Location, What}

    if (!result) {
        // TODO: notify
        clangDebug() << "Refactoring failed: " << result.getError().message().c_str();
        return KDevelop::DocumentChangeSet();
    }

    auto changes = toDocumentChangeSet(result.get(), rc->cache, rc->cache->refactoringTool().getFiles());
    if (!changes) {
        // TODO: notify
        clangDebug() << "Translation Replacements to DocumentChangeSet failed " <<
                     result.getError().message().c_str();
        return KDevelop::DocumentChangeSet();
    }
    return changes.get();
}

// TODO: consider removing this function
std::vector<std::string> sources(CompilationDatabase db)
{
    return db->database->getAllFiles();
}

};
