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

#ifndef KDEV_CLANG_INSTANCETOSTATICREFACTORING_H
#define KDEV_CLANG_INSTANCETOSTATICREFACTORING_H

#include "refactoring.h"
#include "redeclarationchain.h"

/**
 * Changes instance method to static method. Detects if "this" pointer is used from definition
 * (also implicitly) and if so introduces new parameter - "explicit" "this" pointer. Changes calls
 * appropriately.
 *
 * @note detection of "this" works only if definition is accessible in TU from which instance of
 * this class is made.
 * @note Definition of class shall be in only one file (header).
 * @note detection of "this" does not work with recursive functions - in such cases "this" will
 * always be made explicit.
 */
class InstanceToStaticRefactoring : public Refactoring
{
    Q_OBJECT;
    Q_DISABLE_COPY(InstanceToStaticRefactoring);

    class InstanceToStaticCallback;
    friend class InstanceToStaticCallback;

public:
    explicit InstanceToStaticRefactoring(const clang::CXXMethodDecl *decl);

    virtual llvm::ErrorOr<clang::tooling::Replacements> invoke(RefactoringContext *ctx);
    virtual QString name() const;

    /**
     * Essence of this refactoring, used from testing code
     */
    clang::tooling::Replacements doRefactoring(clang::tooling::RefactoringTool &tool,
                                               const std::string &nameForThisPtr);

private:
    RedeclarationChain m_declDispatcher;
    bool m_usesThisPtr = true; // safe default
    bool m_usesThisPtrStateKnown = false;
};


#endif //KDEV_CLANG_INSTANCETOSTATICREFACTORING_H
