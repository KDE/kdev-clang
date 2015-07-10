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

#include "debug.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class Renamer : public MatchFinder::MatchCallback
{
public:
    Renamer(const std::string &filename, unsigned offset, const std::string &oldQualName,
            const std::string &newName, Replacements &replacements)
        : m_filename(filename)
          , m_offset(offset)
          , m_oldQualName(oldQualName)
          , m_newName(newName)
          , m_replacements(replacements)
    {
    }

    virtual void run(const MatchFinder::MatchResult &result) override;

    void handleDeclRefExpr(const MatchFinder::MatchResult &result, const DeclRefExpr *declRefExpr);

    void handleVarDecl(const MatchFinder::MatchResult &result, const VarDecl *varDecl);

private:
    bool applicableTo(const VarDecl *canonicalVarDecl,
                      const MatchFinder::MatchResult &result) const;

private:
    const std::string &m_filename;
    const unsigned m_offset;
    const std::string &m_oldQualName;
    const std::string m_newName;
    Replacements &m_replacements;
};


RenameVarDeclRefactoring::RenameVarDeclRefactoring(const std::string &fileName, unsigned offset,
                                                   const std::string &declName,
                                                   std::string oldQualName, QObject *parent)
    : Refactoring(parent)
      , m_fileName(fileName)
      , m_offset(offset)
      , m_oldVarDeclName(declName)
      , m_oldQualName(std::move(oldQualName))
{
}

llvm::ErrorOr<clang::tooling::Replacements> RenameVarDeclRefactoring::invoke(
    RefactoringContext *ctx)
{
    auto clangTool = ctx->cache->refactoringTool();

    const QString oldName = QString::fromStdString(m_oldVarDeclName);
    const QString newName = QInputDialog::getText(nullptr, i18n("Rename variable"),
                                                  i18n("Type new name of variable"),
                                                  QLineEdit::Normal,
                                                  oldName);
    if (newName.isEmpty() || newName == oldName) {
        return clangTool.getReplacements();
    }

    refactorDebug() << "Will rename" << m_oldVarDeclName.c_str() << "to:" << newName;

    auto declRefMatcher = declRefExpr().bind("DeclRef");
    auto varDeclMatcher = varDecl().bind("VarDecl");

    Renamer renamer(m_fileName, m_offset, m_oldQualName, newName.toStdString(),
                    clangTool.getReplacements());
    MatchFinder finder;
    finder.addMatcher(declRefMatcher, &renamer);
    finder.addMatcher(varDeclMatcher, &renamer);

    clangTool.run(tooling::newFrontendActionFactory(&finder).get());

    auto result = clangTool.getReplacements();
    clangTool.getReplacements().clear();
    return result;
}

QString RenameVarDeclRefactoring::name() const
{
    return i18n("rename [%1]").arg(
        QString::fromStdString(m_oldQualName.empty() ? m_oldVarDeclName : m_oldQualName));
}

void Renamer::run(const MatchFinder::MatchResult &result)
{
    const DeclRefExpr *declRefExpr = result.Nodes.getStmtAs<DeclRefExpr>("DeclRef");
    if (declRefExpr) {
        return handleDeclRefExpr(result, declRefExpr);
    }
    const VarDecl *varDecl = result.Nodes.getDeclAs<VarDecl>("VarDecl");
    if (varDecl) {
        return handleVarDecl(result, varDecl);
    }
    Q_ASSERT(false);

    // getLocation() returns begin of name, the rest - begin of declaration (type)

    // in general canonical declaration is the first declaration in TU
    // it may or may not be equal to canonical declaration in another TU
    // e.g. both forward declare extern variable in two different locations in files
}

void Renamer::handleDeclRefExpr(const MatchFinder::MatchResult &result,
                                const DeclRefExpr *declRefExpr)
{
    const VarDecl *varDecl = llvm::dyn_cast<VarDecl>(declRefExpr->getDecl());
    if (varDecl == nullptr) {
        return;
    }
    const VarDecl *canonicalVarDecl = varDecl->getCanonicalDecl();
    if (!applicableTo(canonicalVarDecl, result)) {
        return;
    }
    m_replacements.insert(Replacement(*result.SourceManager,
                                      CharSourceRange::getTokenRange(declRefExpr->getLocation()),
                                      m_newName));
    // TODO Clang 3.7: add last parameter result.Context->getLangOpts() above
}

void Renamer::handleVarDecl(const MatchFinder::MatchResult &result, const VarDecl *varDecl)
{
    const VarDecl *canonicalVarDecl = varDecl->getCanonicalDecl();
    if (!applicableTo(canonicalVarDecl, result)) {
        return;
    }
    m_replacements.insert(Replacement(*result.SourceManager, varDecl->getLocation(),
                                      Lexer::MeasureTokenLength(
                                          varDecl->getLocation(), *result.SourceManager,
                                          result.Context->getLangOpts()),
                                      m_newName));
    // TODO Clang 3.7: add last parameter result.Context->getLangOpts() above
}

bool Renamer::applicableTo(const VarDecl *canonicalVarDecl,
                           const MatchFinder::MatchResult &result) const
{
    return (canonicalVarDecl->getLinkageInternal() == ExternalLinkage &&
            canonicalVarDecl->getQualifiedNameAsString() == m_oldQualName) ||
           isLocationEqual(m_filename, m_offset, canonicalVarDecl->getSourceRange().getBegin(),
                           *result.SourceManager);
}