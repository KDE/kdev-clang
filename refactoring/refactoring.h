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

#ifndef KDEV_CLANG_REFACTORING_H
#define KDEV_CLANG_REFACTORING_H

// Qt
#include <QObject>

// Clang
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/Core/Replacement.h>
#include <clang/Tooling/Refactoring.h>

#include "refactoringinfo.h"
#include "refactoringcontext.h"

class QDialog;

class DocumentCache;

/**
 * Wraps interface of single refactoring action. Objects will be used to perform refactoring action.
 *
 * Currently it is an abstract class deriving from @c QObject and @c RefactoringInfo.
 * @c QObject base is used to manage lifetime of "@c RefactoringInfo part". It may be considered
 * to remove these base classes and extend @c RefactoringInfo interface with factory method for
 * @c Refactoring class and @c QObject base to maintain lifetime of instances. It turned out that
 * @c QObject base is not actually necessary for implementation of refactorings.
 */
class Refactoring : public QObject, public RefactoringInfo
{
    Q_OBJECT;
    Q_DISABLE_COPY(Refactoring);

    /**
     * @c scheduleRefactoring uses busy dialog and @c uiLockerCallback
     */
    friend clang::tooling::Replacements RefactoringContext::scheduleRefactoring(
        std::function<clang::tooling::Replacements(clang::tooling::RefactoringTool &)>);

public:
    Refactoring(QObject *parent);

    /**
     * Entry point for given refactoring action implementation. When user decide to use this
     * refactoring, @c invoke is called with instance of @c RefactoringContext ready to use
     * by implementation. It is called on GUI thread - long operations should be performed
     * in background using @c RefactoringContext::scheduleRefactoring (or other method from
     * @c RefactoringContext). When finished it shall return @c clang::tooling::Replacements
     * (on success) or @c std::error_code (on failure).
     */
    virtual llvm::ErrorOr<clang::tooling::Replacements> invoke(RefactoringContext *ctx) = 0;

protected:
    /**
     * Implementation can use this helper method if user decided to cancel operation to get
     * "marked" retrun value to return from @c invoke (so that it can be used by backend to
     * perceive this intention).
     *
     * @note Current implementation does not mark and return empty @c clang::tooling::Replacements
     */
    static llvm::ErrorOr<clang::tooling::Replacements> cancelledResult();

    /* TODO: interruptedResult - it is desirable to support interruption of refactoring operation.
     * (implementation of interruption mechanism is not trivial task!)
     */

    /**
     * GUI placeholder (modal dialog) when refactoring action takes place and UI should be blocked.
     * @todo button to interrupt (as part of interruption mechanism)
     */
    static QDialog *newBusyDialog();

    /**
     * Creates @c RefactoringContext::schedule compatible callback used in conjunction with modal
     * dialog ("locking ui") and storing result of refactoring action in given
     * @c clang::tooling::Replacements object.
     */
    static std::function<void(clang::tooling::Replacements)> uiLockerCallback(
        QDialog *uiLocker, clang::tooling::Replacements &result);
};


#endif //KDEV_CLANG_REFACTORING_H
