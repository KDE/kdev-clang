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

// Qt
#include <QInputDialog>

// KF5
#include <KLocalizedString>

// Clang
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "instancetostaticrefactoring.h"
#include "tudecldispatcher.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace
{

class LookForCXXThisExprVisitor : public RecursiveASTVisitor<LookForCXXThisExprVisitor>
{
public:
    explicit LookForCXXThisExprVisitor();

    bool VisitCXXThisExpr(const CXXThisExpr *);

    bool usesThisPtr() const
    {
        return m_usesThisPtr;
    }

private:
    bool m_usesThisPtr = false;
};

}

InstanceToStaticRefactoring::InstanceToStaticRefactoring(const CXXMethodDecl *decl)
    : Refactoring(nullptr)
    , m_declDispatcher(declarationComparator(decl))
{
    for (FunctionDecl *redecl : decl->redecls()) {
        if (redecl->isThisDeclarationADefinition()) {
            LookForCXXThisExprVisitor visitor;
            visitor.TraverseDecl(redecl);
            m_usesThisPtr = visitor.usesThisPtr();
            m_usesThisPtrStateKnown = m_usesThisPtr;
        }
    }
}

Refactoring::ResultType InstanceToStaticRefactoring::invoke(RefactoringContext *ctx)
{
    QString nameForThisPtr;
    if (m_usesThisPtr) {
        nameForThisPtr = QInputDialog::getText(nullptr, i18n("Name for this"),
                                               i18n("Type name for <code>this</code> pointer"),
                                               QLineEdit::Normal, QStringLiteral("self"));
        if (nameForThisPtr.isEmpty()) {
            return cancelledResult();
        }
    }
    auto replacements = ctx->scheduleRefactoring(
        [this, nameForThisPtr](RefactoringTool &tool)
        {
            return doRefactoring(tool, nameForThisPtr.toStdString());
        }
    );

    if (m_usesThisPtr != m_usesThisPtrStateKnown) {
        ctx->reportInformation(i18n("You may consider removing unneccessary <code>%1</code>"
                                        " argument using Change Signature refactoring")
                                   .arg(nameForThisPtr));
        // Single pass + heuristic was unable to prove that "this" pointer is not used and thus
        // introduced "self" pointer as a replacement. Second pass proved that it was in fact
        // unnecessary.
    }

    return replacements;
}


class InstanceToStaticRefactoring::InstanceToStaticCallback : public MatchFinder::MatchCallback
{
    class TranslateCXXThisExprs;

    friend class TranslateCXXThisExprs;

public:
    explicit InstanceToStaticCallback(InstanceToStaticRefactoring *refactoring,
                                      const std::string &nameForThisPtr);

    virtual void run(const MatchFinder::MatchResult &result) override;

private:
    void handleMethodDecl(const CXXMethodDecl *methodDecl, ASTContext *astContext,
                          SourceManager *sourceManager);
    void handleCallExpr(const CXXMemberCallExpr *callExpr, ASTContext *astContext,
                        SourceManager *sourceManager);
public:
    Replacements replacements;

private:
    InstanceToStaticRefactoring *m_refactoring;
    string m_nameForThisPtr;
    TUDeclDispatcher m_declDispatcher;
};


clang::tooling::Replacements InstanceToStaticRefactoring::doRefactoring(
    RefactoringTool &tool, const string &nameForThisPtr)
{
    auto methodDeclMatcher = methodDecl().bind("MethodDecl");
    auto callExprMatcher = memberCallExpr().bind("CallExpr");
    MatchFinder finder;
    InstanceToStaticCallback callback(this, nameForThisPtr);
    finder.addMatcher(methodDeclMatcher, &callback);
    finder.addMatcher(callExprMatcher, &callback);
    tool.run(newFrontendActionFactory(&finder).get());
    return move(callback.replacements);
}

QString InstanceToStaticRefactoring::name() const
{
    return i18n("Make static");
}

LookForCXXThisExprVisitor::LookForCXXThisExprVisitor()
{
}

bool LookForCXXThisExprVisitor::VisitCXXThisExpr(const CXXThisExpr *)
{
    m_usesThisPtr = true;
    return true;
}

InstanceToStaticRefactoring::InstanceToStaticCallback::InstanceToStaticCallback(
    InstanceToStaticRefactoring *refactoring, const string &nameForThisPtr)
    : m_refactoring(refactoring)
    , m_nameForThisPtr(nameForThisPtr)
    , m_declDispatcher(refactoring->m_declDispatcher.get())
{
}

void InstanceToStaticRefactoring::InstanceToStaticCallback::run(
    const MatchFinder::MatchResult &result)
{
    auto methodDecl = result.Nodes.getNodeAs<CXXMethodDecl>("MethodDecl");
    if (methodDecl && m_declDispatcher.equivalent(methodDecl)) {
        handleMethodDecl(methodDecl, result.Context, result.SourceManager);
        return;
    }
    auto callExpr = result.Nodes.getNodeAs<CXXMemberCallExpr>("CallExpr");
    if (callExpr && m_declDispatcher.equivalent(callExpr->getMethodDecl())) {
        handleCallExpr(callExpr, result.Context, result.SourceManager);
        return;
    }
}

class InstanceToStaticRefactoring::InstanceToStaticCallback::TranslateCXXThisExprs
    : public RecursiveASTVisitor<TranslateCXXThisExprs>
{
public:
    TranslateCXXThisExprs(InstanceToStaticCallback &callback, ASTContext *astContext,
                          SourceManager *sourceManager, const string &nameForThisPtr);

    bool VisitCXXThisExpr(const CXXThisExpr *thisExpr);

    bool usesThisPtr() const
    {
        return m_usesThisPtr;
    }

public:
    InstanceToStaticCallback &callback;

private:
    ASTContext *m_astContext;
    SourceManager *m_sourceManager;
    string m_nameForThisPtr;
    bool m_usesThisPtr = false;
};

void InstanceToStaticRefactoring::InstanceToStaticCallback::handleMethodDecl(
    const CXXMethodDecl *methodDecl, ASTContext *astContext, SourceManager *sourceManager)
{
    // TODO: handle recursion (improve reasoning about use of CXXThisExpr, also above)
    if (methodDecl->isThisDeclarationADefinition()) {
        TranslateCXXThisExprs visitor(*this, astContext, sourceManager, m_nameForThisPtr);
        visitor.TraverseDecl(const_cast<CXXMethodDecl *>(methodDecl));
        m_refactoring->m_usesThisPtrStateKnown = visitor.usesThisPtr();
    }
    if (methodDecl->getLexicalParent() == methodDecl->FunctionDecl::getParent()) {
        replacements.insert(Replacement(*sourceManager, methodDecl->getLocStart(), 0, "static "));
    }
    auto thisType = methodDecl->getThisType(*astContext).getUnqualifiedType();
    auto thisTypeS = toString(thisType, astContext->getLangOpts());
    auto selfArgS = thisTypeS + " " + m_nameForThisPtr;
    if (methodDecl->getNumParams() > 0) {
        selfArgS += ", ";
    }
    auto funTypeLoc = methodDecl->getTypeSourceInfo()->getTypeLoc().castAs<FunctionTypeLoc>();
    if (m_refactoring->m_usesThisPtr) {
        replacements.insert(
            Replacement(*sourceManager, funTypeLoc.getLParenLoc().getLocWithOffset(1), 0,
                        selfArgS));
    }
    if (methodDecl->isConst()) {
        // try to remove const qualifier
        // Clang does not provide source location of cv-qualifiers - this is heuristic solution
        replacements.insert(Replacement(*sourceManager, CharSourceRange::getTokenRange(
            funTypeLoc.getRParenLoc().getLocWithOffset(1), funTypeLoc.getEndLoc()), ""));
    }
}

InstanceToStaticRefactoring::InstanceToStaticCallback::TranslateCXXThisExprs::TranslateCXXThisExprs(
    InstanceToStaticCallback &callback, ASTContext *astContext, SourceManager *sourceManager,
    const string &nameForThisPtr)
    : callback(callback)
    , m_astContext(astContext)
    , m_sourceManager(sourceManager)
    , m_nameForThisPtr(nameForThisPtr)
{
}

bool InstanceToStaticRefactoring::InstanceToStaticCallback::TranslateCXXThisExprs::VisitCXXThisExpr(
    const CXXThisExpr *thisExpr)
{
    m_usesThisPtr = true;
    string selfS = m_nameForThisPtr + "->";
    if (thisExpr->isImplicit()) {
        callback.replacements.insert(
            Replacement(*m_sourceManager, thisExpr->getLocStart(), 0, selfS));
    } else {
        callback.replacements.insert(Replacement(*m_sourceManager, thisExpr, m_nameForThisPtr));
    }
    return true;
}

void InstanceToStaticRefactoring::InstanceToStaticCallback::handleCallExpr(
    const CXXMemberCallExpr *callExpr, ASTContext *astContext, SourceManager *sourceManager)
{
    if (!m_refactoring->m_usesThisPtr) {
        return;
        // No need to introduce "this" argument on call site
    }
    const MemberExpr *memberExpr = llvm::dyn_cast<MemberExpr>(callExpr->getCallee());
    Q_ASSERT(memberExpr);
    const Expr *thisExpr = memberExpr->getBase();
    SourceLocation location;
    if (callExpr->getNumArgs() == 0) {
        location = callExpr->getRParenLoc();
    } else {
        location = callExpr->getArg(0)->getLocStart();
    }
    string thisExprS = thisExpr->isImplicitCXXThis() ?
                       "this" :
                       codeFromASTNode(thisExpr, *sourceManager, astContext->getLangOpts());
    if (!memberExpr->isArrow()) {
        thisExprS = "&" + thisExprS;
    }
    if (callExpr->getNumArgs() > 0) {
        thisExprS += ", ";
    }
    replacements.insert(Replacement(*sourceManager, location, 0, thisExprS));
    // We don't change base of MemberExpr - it is not strictly required and could break code
    // (e.g. types without name or inaccessible name)
}

