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

#ifndef KDEV_CLANG_RENAMEFIELDDECLTUREFACTORING_H
#define KDEV_CLANG_RENAMEFIELDDECLTUREFACTORING_H

#include "refactoring.h"

/**
 * Rename fields with non-external linkage (using declaration location as identifier).
 *
 * @note There are more reliable approaches to this problem...
 */
class RenameFieldDeclTURefactoring : public Refactoring
{
    Q_OBJECT;
    Q_DISABLE_COPY(RenameFieldDeclTURefactoring);
public:
    RenameFieldDeclTURefactoring(const std::string &fileName, unsigned offset,
                                 llvm::StringRef oldName);

    virtual ResultType invoke(RefactoringContext *ctx) override;

    virtual QString name() const override;

private:
    const std::string m_fileName;
    const unsigned m_fileOffset;
    const std::string m_oldFieldDeclName;
};

namespace Refactorings
{
namespace RenameFieldTuDecl
{
/**
 * Essence of this refactoring, used from testing code
 */
int run(const std::string fileName, unsigned fileOffset, const std::string &newName,
        clang::tooling::RefactoringTool &clangTool);
}
}

#endif //KDEV_CLANG_RENAMEFIELDDECLTUREFACTORING_H
