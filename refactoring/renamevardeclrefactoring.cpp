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

#include "renamevardeclrefactoring.h"

// Qt
#include <QInputDialog>

// KF5
#include <KI18n/klocalizedstring.h>

// Clang
#include <clang/Lex/Lexer.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <refactoringcontext.h>

#include "documentcache.h"
#include "utils.h"

#include "../util/clangdebug.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class Renamer : public MatchFinder::MatchCallback
{
public:
    Renamer(const std::string &filename, unsigned offset, const std::string &newName,
            Replacements &replacements)
        : m_filename(filename)
          , m_offset(offset)
          , m_newName(newName)
          , m_replacements(replacements)
    {
    }

    virtual void run(MatchFinder::MatchResult const &result) override;

    const VarDecl *foundDeclaration() const
    {
        return m_foundDeclaration;
    }

private:
    const std::string m_filename;
    const unsigned m_offset;
    const std::string m_newName;
    Replacements &m_replacements;

private:
    const VarDecl *m_foundDeclaration = nullptr;
};


RenameVarDeclRefactoring::RenameVarDeclRefactoring(const std::string &fileName, unsigned offset,
                                                   const std::string &declName, QObject *parent)
    : Refactoring(parent)
      , m_fileName(fileName)
      , m_offset(offset)
      , m_oldVarDeclName(declName)
{
}

llvm::ErrorOr<clang::tooling::Replacements> RenameVarDeclRefactoring::invoke(
    RefactoringContext *ctx)
{
    auto clangTool = ctx->cache->refactoringTool();

    // FIXME: provide old name as default
    const QString newName = QInputDialog::getText(nullptr, i18n("Rename variable"),
                                                  i18n("Type new name of variable"));
    if (newName.isEmpty()) {
        return clangTool.getReplacements();
    }

    clangDebug() << "Will rename" << m_oldVarDeclName.c_str() << "to:" << newName;

    auto matcher = declRefExpr().bind("DeclRef");

    Renamer renamer(m_fileName, m_offset, newName.toStdString(), clangTool.getReplacements());
    MatchFinder finder;
    finder.addMatcher(matcher, &renamer);

    clangTool.run(tooling::newFrontendActionFactory(&finder).get());

    auto result = clangTool.getReplacements();
    clangTool.getReplacements().clear();
    return result;
}

QString RenameVarDeclRefactoring::name() const
{
    return i18n("rename");
}

void Renamer::run(const MatchFinder::MatchResult &result)
{
    // FIXME: collect all DeclRefExpr for this DeclRefExpr/VarDecl
    // FIXME: collect all VarDecl for this DeclRefExpr/VarDecl
    // FIXME: compare canonical VarDecl's
    const DeclRefExpr *declRefExpr = result.Nodes.getStmtAs<DeclRefExpr>("DeclRef");
    if (!declRefExpr) {
        return;
    }
    const VarDecl *varDecl = llvm::dyn_cast<VarDecl>(declRefExpr->getDecl()); // NamedDecl?
    if (!varDecl) {
        return;
    }
    CharSourceRange range = CharSourceRange::getTokenRange(varDecl->getSourceRange());
    auto begin = result.SourceManager->getDecomposedLoc(range.getBegin());
    auto end = result.SourceManager->getDecomposedLoc(range.getEnd().getLocWithOffset(
        Lexer::MeasureTokenLength(range.getEnd(), *result.SourceManager,
                                  result.Context->getLangOpts())));
    auto fileEntry = result.SourceManager->getFileEntryForID(begin.first);
    if (!llvm::sys::fs::equivalent(fileEntry->getName(), m_filename)) {
        return;
    }
    varDecl->dump();
    clangDebug() << begin.second << m_offset << end.second;
    if (begin.second <= m_offset && m_offset <= end.second) {
        if (!m_foundDeclaration) {
            m_foundDeclaration = varDecl;
            m_replacements.insert(Replacement(*result.SourceManager, varDecl->getLocation(),
                                              varDecl->getIdentifier()->getLength(), m_newName));
            // TODO Clang 3.7: add last parameter result.Context->getLangOpts() above
            // TODO: consider using Lexer::MeasureTokenLength above

            // getLocation() returns begin of name, the rest - begin of declaration (type)
            // consider reduction of acceptable range to avoid interference with other refactorings
            // e.g. int myNumber = myFavouriteInteger();
            // could allow renaming of type (int), variable name (myNumber), function name
            // (myFavouriteInteger).
        }
        m_replacements.insert(Replacement(*result.SourceManager, declRefExpr, m_newName));
    }
}
