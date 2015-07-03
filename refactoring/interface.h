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

// This header file should be independent of any Clang libraries both
// compile time and run time.
// All of refactorings features will be accessible from here

#ifndef KDEV_CLANG_INTERFACE_H
#define KDEV_CLANG_INTERFACE_H

// C++
#include <vector>
#include <string>
#include <unordered_map>

// Qt
#include <QUrl>
#include <QString>

// KF5
#include <KTextEditor/ktexteditor/cursor.h>

// KDevelop
#include <language/codegen/documentchangeset.h>

namespace KDevelop
{
class IDocumentController;
};

class RefactoringInfo;
class RefactoringContext;

namespace Refactorings
{

struct CompilationDatabase_t;
struct RefactoringsContext_t;

// All std::strings appearing here may be replaced to something different
// if necessary (QString, QUrl, ...)
// All functions provided here get C linkage. These (few) functions as designed
// to provide interoperability with KDevelop based on stable, predictable ABI.
// It can bu used later to dlopen implementation, or as "engine" in standalone
// executable.
// Note: some types are not fully known now. typedefs will change soon


// CompilationDatabase will be used internally, but needs initialization
typedef CompilationDatabase_t *CompilationDatabase;

// All data which can be required for any operation of this library.
// It is CompilationDatabase, ClangTool (with virtual files), ...
typedef RefactoringContext *RefactoringsContext;

// Short information about one particular kind of refactoring.
typedef RefactoringInfo *RefactoringKind;

enum class ProjectKind
{
    CMAKE,      // CMake project are already nice
    NON_CMAKE,  // Non CMake are nice after use of Bear
};  // Where "nice" means prepared compile_commands.json file

/**
 * Prepares and returns compilation database for given project build path.
 * Note: result of this operation is deprecated by any change in project
 * structure (adding new file, removing, renaming, changing flags, ...).
 * In such a case new object must be created using this function.
 *
 * @param buildPath build path for given project (ex. CMAKE_BINARY_DIR)
 * @param kind CMAKE if underlying project is a configured CMake project
 *             with CMAKE_EXPORT_COMPILE_COMMANDS enabled, NON_CMAKE otherwise
 * @param errorMessage will be set to error message if any occurred
 * @return Compilation database for given project
 */
CompilationDatabase createCompilationDatabase(const std::string &buildPath, ProjectKind kind,
                                              QString &errorMessage);

/**
 * Simpler version of the above only for CMake projects as requested in
 * https://git.reviewboard.kde.org/r/123963/#comment55684 and
 * https://git.reviewboard.kde.org/r/123963/#comment55548
 *
 * @return nullptr on error
 */
CompilationDatabase createCMakeCompilationDatabase(const std::string &buildPath);

/**
 * Frees resources used by given compilation database.
 * NOTE: You mustn't release database given to @c createRefactoringsContext
 * as that function takes ownership of compilation database.
 *
 * @param db compilation database to be released
 */
void releaseCompilationDatabase(CompilationDatabase db);

// TODO: mergeCompilationDatabases - handle refactorings between projects

/**
 * Returns list of all sources from given CompilationDatabase
 *
 * @note: dedicated to provide @c sources for @c createRefactoringsContext
 */
std::vector<std::string> sources(CompilationDatabase db);

/**
 * Prepare and return main context for refactorings in KDevelop.
 *
 * @param db Up-to-date compilation database
 * @return Context for future use with refactorings
 */
RefactoringsContext createRefactoringsContext(CompilationDatabase db);

/**
 * Invokes selected refactoring action on selected place in file.
 *
 * @param rc Initialized refactorings context
 * @param refactoring A refactoring kind we are going to apply here
 * @param sourceFile source file name (full path)
 * @param position position in source file which we are going to work on
 */
KDevelop::DocumentChangeSet refactorThis(RefactoringsContext rc, RefactoringKind refactoring,
                                         const QUrl &sourceFile,
                                         const KTextEditor::Cursor &position);
// NOTE: We have to cooperate with cache here (it is guaranteed that sourceFile is in cache,
// as this file is already opened). Maybe it is more reasonable to provide Document* here?
// Especially when DocumentCache middleware will be IDocumentController aware.
// NOTE: position (Cursor) is not nice - will need translation to offset.
// NOTE: We are going to need some GUI handle here. Probably something like QWidget* parent
// will be introduced as additional parameter (but it will bind this interface to Qt Widgets).
// NOTE: Can we block calling thread? I would say no. We are going to need Qt wrapper/continuation
// here.


// TODO: refactorThis(RefactoringsContext, RefactoringKind, Location, ...)
// TODO: refactor(RefactoringsContext, RefactoringKind, What, ...)
// ^^^^ will return DocumentChangeSet

};

#endif //KDEV_CLANG_INTERFACE_H
