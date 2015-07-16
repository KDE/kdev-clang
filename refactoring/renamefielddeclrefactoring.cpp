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

#include "renamefielddeclrefactoring.h"

// Qt
#include <QString>
#include <QInputDialog>

// KF5
#include <KF5/KI18n/klocalizedstring.h>

// Clang
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "refactoringcontext.h"
#include "documentcache.h"
#include "debug.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace
{

class Renamer : public MatchFinder::MatchCallback
{
public:
    Renamer(const std::string &oldQualName, const std::string &newName, Replacements &replacements)
        : m_oldQualName(oldQualName)
          , m_newName(newName)
          , m_replacements(replacements)
    {
    }

    virtual void run(const MatchFinder::MatchResult &result) override;

    void handleMemberExpr(const MatchFinder::MatchResult &result, const MemberExpr *declRefExpr);

    void handleFieldDecl(const MatchFinder::MatchResult &result, const FieldDecl *varDecl);

private:
    bool applicableTo(const FieldDecl *canonicalFieldDecl,
                      const MatchFinder::MatchResult &result) const;

private:
    const std::string &m_oldQualName;
    const std::string m_newName;
    Replacements &m_replacements;
};

}; // namespace

RenameFieldDeclRefactoring::RenameFieldDeclRefactoring(const std::string &oldName,
                                                       std::string oldQualName)
    : Refactoring(nullptr)
      , m_oldFieldDeclName(oldName)
      , m_oldQualName(std::move(oldQualName))
{
}

llvm::ErrorOr<clang::tooling::Replacements> RenameFieldDeclRefactoring::invoke(
    RefactoringContext *ctx)
{
    auto &clangTool = ctx->cache->refactoringTool();

    const QString oldName = QString::fromStdString(m_oldFieldDeclName);
    const QString newName = QInputDialog::getText(nullptr, i18n("Rename field"),
                                                  i18n("Type new name of field"),
                                                  QLineEdit::Normal,
                                                  oldName);
    if (newName.isEmpty() || newName == oldName) {
        return clangTool.getReplacements();
    }

    refactorDebug() << "Will rename" << m_oldFieldDeclName << "to:" << newName;

    auto memberExprMatcher = memberExpr().bind("MemberExpr");
    auto fieldDeclMatcher = fieldDecl().bind("FieldDecl");

    Renamer renamer(m_oldQualName, newName.toStdString(), clangTool.getReplacements());
    MatchFinder finder;
    finder.addMatcher(memberExprMatcher, &renamer);
    finder.addMatcher(fieldDeclMatcher, &renamer);

    clangTool.run(tooling::newFrontendActionFactory(&finder).get());

    auto result = clangTool.getReplacements();
    clangTool.getReplacements().clear();
    return result;
}

QString RenameFieldDeclRefactoring::name() const
{
    return i18n("rename %1").arg(QString::fromStdString(m_oldQualName));
}

void Renamer::run(const MatchFinder::MatchResult &result)
{
    const MemberExpr *memberExpr = result.Nodes.getStmtAs<MemberExpr>("MemberExpr");
    if (memberExpr) {
        return handleMemberExpr(result, memberExpr);
    }
    const FieldDecl *fieldDecl = result.Nodes.getDeclAs<FieldDecl>("FieldDecl");
    if (fieldDecl) {
        return handleFieldDecl(result, fieldDecl);
    }
    Q_ASSERT(false);
}

void Renamer::handleMemberExpr(const MatchFinder::MatchResult &result,
                               const MemberExpr *memberExpr)
{
    const FieldDecl *fieldDecl = llvm::dyn_cast<FieldDecl>(memberExpr->getMemberDecl());
    if (fieldDecl == nullptr) {
        // memberExpr refers to CXXMethodDecl
        return;
    }
    const FieldDecl *canonicalVarDecl = fieldDecl->getCanonicalDecl();
    if (!applicableTo(canonicalVarDecl, result)) {
        return;
    }
    m_replacements.insert(Replacement(*result.SourceManager,
                                      CharSourceRange::getTokenRange(memberExpr->getMemberLoc()),
                                      m_newName));
    // TODO Clang 3.7: add last parameter result.Context->getLangOpts() above
}

void Renamer::handleFieldDecl(const MatchFinder::MatchResult &result, const FieldDecl *fieldDecl)
{
    const FieldDecl *canonicalFieldDecl = fieldDecl->getCanonicalDecl();
    if (!applicableTo(canonicalFieldDecl, result)) {
        return;
    }
    m_replacements.insert(Replacement(*result.SourceManager,
                                      CharSourceRange::getTokenRange(fieldDecl->getLocation()),
                                      m_newName));
    // TODO Clang 3.7: add last parameter result.Context->getLangOpts() above
}

bool Renamer::applicableTo(const FieldDecl *canonicalFieldDecl,
                           const MatchFinder::MatchResult &result) const
{
    Q_UNUSED(result);
    return (canonicalFieldDecl->getLinkageInternal() == ExternalLinkage &&
            canonicalFieldDecl->getQualifiedNameAsString() == m_oldQualName);
}
