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

// Clang
#include <clang/Lex/Lexer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Refactoring.h>

// KF5
#include <KF5/KI18n/klocalizedstring.h>

#include "changesignaturerefactoring.h"
#include "refactoringcontext.h"
#include "documentcache.h"
#include "changesignaturedialog.h"
#include "changesignaturerefactoringinfopack.h"
#include "changesignaturerefactoringchangepack.h"
#include "declarationcomparator.h"
#include "debug.h"
#include "tudecldispatcher.h"
#include "utils.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace
{
class Translator : public MatchFinder::MatchCallback
{
    using InfoPack = ChangeSignatureRefactoring::InfoPack;
    using ChangePack = ChangeSignatureRefactoring::ChangePack;
public:
    Translator(const DeclarationComparator *declComparator, const InfoPack *infoPack,
               const ChangePack *changePack, Replacements &replacements);
    virtual void run(const MatchFinder::MatchResult &Result) override;

private:
    void handleDeclRefExpr(const DeclRefExpr *declRefExpr, SourceManager *const sourceManager,
                           const LangOptions &langOpts);
    void handleMemberExpr(const MemberExpr *memberExpr, SourceManager *const sourceManager,
                          const LangOptions &langOpts);
    void handleFunctionDecl(const FunctionDecl *functionDecl, SourceManager *const sourceManager,
                            const LangOptions &langOpts);
    void handleCallExpr(const CallExpr *callExpr, SourceManager *const sourceManager,
                        const LangOptions &langOpts);

    bool wantChangeReturnType() const;
    bool wantChangeName() const;

private:
    TUDeclDispatcher m_declDispatcher;
    const InfoPack *m_infoPack;
    const ChangePack *m_changePack;
    Replacements &m_replacements;
};
}

ChangeSignatureRefactoring::ChangeSignatureRefactoring(const FunctionDecl *functionDecl,
                                                       ASTContext *astContext)
    : Refactoring(nullptr)
    , m_infoPack(InfoPack::fromFunctionDecl(functionDecl, astContext))
{
}

ChangeSignatureRefactoring::~ChangeSignatureRefactoring() = default;

Refactoring::ResultType ChangeSignatureRefactoring::invoke(RefactoringContext *ctx)
{
    std::unique_ptr<ChangeSignatureDialog> dialog(
        new ChangeSignatureDialog(m_infoPack.get(), nullptr));
    if (dialog->exec() == 0) {
        return cancelledResult();
    }

    auto infoPack = dialog->infoPack(); // C++14...
    auto changePack = dialog->changePack();
    return ctx->scheduleRefactoring(
        [infoPack, changePack](RefactoringTool &tool)
        {
            Refactorings::ChangeSignature::run(infoPack, changePack, tool);
            return tool.getReplacements();
        }
    );
}

namespace Refactorings
{
namespace ChangeSignature
{
int run(const InfoPack *infoPack, const ChangePack *changePack, RefactoringTool &tool)
{
    auto declRefExprMatcher = declRefExpr(to(functionDecl())).bind("DeclRefExpr");
    auto memberExprMatcher = memberExpr().bind("MemberExpr");
    auto functionDeclMatcher = functionDecl().bind("FunctionDecl");
    auto functionCallMatcher = callExpr().bind("CallExpr");

    Translator translator(&infoPack->declarationComparator(), infoPack, changePack,
                          tool.getReplacements());
    MatchFinder finder;
    finder.addMatcher(declRefExprMatcher, &translator);
    finder.addMatcher(memberExprMatcher, &translator);
    finder.addMatcher(functionDeclMatcher, &translator);
    finder.addMatcher(functionCallMatcher, &translator);

    return tool.run(newFrontendActionFactory(&finder).get());
}
}
}

QString ChangeSignatureRefactoring::name() const
{
    return i18n("Change signature");
}

Translator::Translator(const DeclarationComparator *declComparator, const InfoPack *infoPack,
                       const ChangePack *changePack, Replacements &replacements)
    : m_declDispatcher(declComparator)
    , m_infoPack(infoPack)
    , m_changePack(changePack)
    , m_replacements(replacements)
{
}

void Translator::run(const MatchFinder::MatchResult &Result)
{
    auto declRef = Result.Nodes.getStmtAs<DeclRefExpr>("DeclRefExpr");
    if (declRef && m_declDispatcher.equivalent(declRef->getDecl())) {
        handleDeclRefExpr(declRef, Result.SourceManager, Result.Context->getLangOpts());
        return;
    }
    auto memberExpr = Result.Nodes.getStmtAs<MemberExpr>("MemberExpr");
    if (memberExpr && m_declDispatcher.equivalent(memberExpr->getMemberDecl())) {
        handleMemberExpr(memberExpr, Result.SourceManager, Result.Context->getLangOpts());
    }
    auto functionDecl = Result.Nodes.getDeclAs<FunctionDecl>("FunctionDecl");
    if (functionDecl && m_declDispatcher.equivalent(functionDecl)) {
        handleFunctionDecl(functionDecl, Result.SourceManager, Result.Context->getLangOpts());
        return;
    }
    auto callExpr = Result.Nodes.getDeclAs<CallExpr>("CallExpr");
    auto callee = callExpr ? callExpr->getCalleeDecl() : nullptr;
    if (callee && m_declDispatcher.equivalent(callee)) {
        handleCallExpr(callExpr, Result.SourceManager, Result.Context->getLangOpts());
        return;
    }
}

void Translator::handleDeclRefExpr(const DeclRefExpr *declRefExpr,
                                   SourceManager *const sourceManager,
                                   const LangOptions &langOpts)
{
    // this handles also non-member function call (as opposed to MemberExpr)
    if (wantChangeName()) {
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getTokenRange(declRefExpr->getLocation()),
                        m_changePack->m_newName));
        // TODO Clang 3.7: use @p langOpts above
    }
}

void Translator::handleMemberExpr(const MemberExpr *memberExpr, SourceManager *const sourceManager,
                                  const LangOptions &langOpts)
{
    // MemberExpr applies only to access through instance (which may be implicit CXXThisExpr)
    // but some instance members may be accessed using different approach (DeclRefExpr) like:
    // &MyClass::myInstanceMethod
    if (wantChangeName()) {
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getTokenRange(memberExpr->getMemberLoc()),
                        m_changePack->m_newName));
        // TODO Clang 3.7: use @p langOpts above
    }
}

void Translator::handleFunctionDecl(const FunctionDecl *functionDecl,
                                    SourceManager *const sourceManager,
                                    const LangOptions &langOpts)
{
    auto functionTypeLoc = functionDecl->getTypeSourceInfo()->getTypeLoc().castAs<FunctionTypeLoc>();
    if (wantChangeReturnType()) {
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getTokenRange(
                functionTypeLoc.getReturnLoc().getSourceRange()), m_changePack->m_newResult));
    }
    if (wantChangeName()) {
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getTokenRange(functionDecl->getLocation()),
                        m_changePack->m_newName));
    }
    // TODO Clang 3.7: use @p langOpts above
    auto paramForPosition = [this, functionDecl, sourceManager, langOpts](unsigned i) -> std::string
    {
        auto newParamIndex = m_changePack->m_paramRefs[i];
        if (newParamIndex >= 0) {
            return codeFromASTNode(functionDecl->getParamDecl(newParamIndex), *sourceManager,
                                   langOpts);
        }
        else {
            const auto &t = m_changePack->m_newParam[-newParamIndex - 1];
            return std::get<0>(t) + " " + std::get<1>(t); // [type] [" "] [name]
            // The above has some inherent limitations... (int (*f())())
        }
    };
    unsigned i = 0;
    for (; (i < functionDecl->getNumParams()) && (i < m_changePack->m_paramRefs.size()); ++i) {
        if (m_changePack->m_paramRefs[i] == static_cast<int>(i)) {
            continue;
            // Because this is no op
        }
        m_replacements.insert(
            Replacement(*sourceManager, functionDecl->getParamDecl(i), paramForPosition(i)));
    }
    if (i < functionDecl->getNumParams()) {
        // We are reducing parameters set -> remove remaining parameters
        SourceLocation locStart =
            i > 0 ?
            functionDecl->getParamDecl(i - 1)->getLocEnd().getLocWithOffset(
                Lexer::MeasureTokenLength(functionDecl->getParamDecl(i - 1)->getLocEnd(),
                                          *sourceManager, langOpts)) :
            functionDecl->getParamDecl(i)->getLocStart();
        m_replacements.insert(
            Replacement(*sourceManager,
                        CharSourceRange::getCharRange(locStart, functionTypeLoc.getRParenLoc()),
                        ""));
    } else if (i < m_changePack->m_paramRefs.size()) {
        // We are extending arguments set -> add new arguments
        bool needComma = i > 0;
        std::string output;
        for (; i < m_changePack->m_paramRefs.size(); ++i) {
            output += "," + paramForPosition(i);
        }
        output += ')'; // We _have_ to eat something in replacement - we eat closing parenthesis
        m_replacements.insert(
            Replacement(*sourceManager, functionTypeLoc.getRParenLoc(),
                        1, // eat closing parenthesis
                        output.substr(static_cast<std::size_t>(!needComma))));
    }
}

void Translator::handleCallExpr(const CallExpr *callExpr, SourceManager *const sourceManager,
                                const LangOptions &langOpts)
{
    auto exprForPosition = [this, callExpr, sourceManager, langOpts](unsigned i) -> std::string
    {
        auto newParamIndex = m_changePack->m_paramRefs[i];
        if (newParamIndex >= 0) {
            return codeFromASTNode(callExpr->getArg(newParamIndex), *sourceManager, langOpts);
        }
        else {
            return " $ NEW PARAMETER $ ";
            // TODO: Consider extending GUI to get something usable above, like default expression
            // to provide in such cases.
        }
    };
    unsigned i = 0;
    for (; (i < callExpr->getNumArgs()) && (i < m_changePack->m_paramRefs.size()); ++i) {
        if (m_changePack->m_paramRefs[i] == static_cast<int>(i)) {
            continue;
            // Because this is no op
        }
        m_replacements.insert(Replacement(*sourceManager, callExpr->getArg(i), exprForPosition(i)));
    }
    if (i < callExpr->getNumArgs()) {
        // We are reducing arguments set -> remove remaining arguments
        SourceLocation locStart =
            i > 0 ?
            callExpr->getArg(i - 1)->getLocEnd().getLocWithOffset(
                Lexer::MeasureTokenLength(callExpr->getArg(i - 1)->getLocEnd(), *sourceManager,
                                          langOpts)) :
            callExpr->getArg(i)->getLocStart();
        m_replacements.insert(
            Replacement(*sourceManager,
                        CharSourceRange::getCharRange(locStart, callExpr->getRParenLoc()), ""));
    } else if (i < m_changePack->m_paramRefs.size()) {
        // We are extending arguments set -> add new arguments
        bool needComma = i > 0;
        std::string output;
        for (; i < m_changePack->m_paramRefs.size(); ++i) {
            output += "," + exprForPosition(i);
        }
        output += ')'; // We _have_ to eat something in replacement - we eat closing parenthesis
        m_replacements.insert(
            Replacement(*sourceManager, callExpr->getRParenLoc(), 1, // eat closing parenthesis
                        output.substr(static_cast<std::size_t>(!needComma))));
    }
}

bool Translator::wantChangeReturnType() const
{
    return !(m_infoPack->isRestricted() || m_changePack->m_newResult.empty() ||
             m_changePack->m_newResult == m_infoPack->returnType());
}

bool Translator::wantChangeName() const
{
    return !(m_infoPack->isRestricted() || m_changePack->m_newName.empty() ||
             m_changePack->m_newName == m_infoPack->functionName());
}
