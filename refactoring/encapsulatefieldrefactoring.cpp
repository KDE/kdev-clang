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
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Refactoring.h>

// KF5
#include <KF5/KI18n/klocalizedstring.h>

#include "encapsulatefieldrefactoring.h"
#include "encapsulatefielddialog.h"
#include "encapsulatefieldrefactoring_changepack.h"
#include "refactoringcontext.h"
#include "tudecldispatcher.h"
#include "utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace std;

namespace
{
class Translator : public MatchFinder::MatchCallback
{
    using ChangePack = EncapsulateFieldRefactoring::ChangePack;
public:
    Translator(Replacements &replacements, const ChangePack *changePack,
               const DeclarationComparator *declDispatcher,
               const DeclarationComparator *recordDeclDispatcher, const string &recordName);

    virtual void run(const MatchFinder::MatchResult &Result) override;
    virtual void onEndOfTranslationUnit() override;

    void handleDeclRefExpr(const DeclRefExpr *declRefExpr, SourceManager *sourceManager,
                           const LangOptions &langOpts);
    void handleMemberExpr(const MemberExpr *memberExpr, SourceManager *sourceManager,
                          const LangOptions &langOpts);
    void handleAssignToDeclRefExpr(const BinaryOperator *assignmentOperator,
                                   SourceManager *sourceManager, const LangOptions &langOpts);
    void handleAssignToMemberExpr(const BinaryOperator *assignmentOperator,
                                  SourceManager *sourceManager, const LangOptions &langOpts);
    void handleAssignOperatorCall(const CXXOperatorCallExpr *operatorCallExpr,
                                  SourceManager *sourceManager, const LangOptions &langOpts);
    void handleAccessSpecDecl(const AccessSpecDecl *accessSpecDecl, SourceManager *sourceManager,
                              const LangOptions &langOpts);
    void handleFieldDecl(const Decl *fieldDecl, SourceManager *sourceManager,
                         const LangOptions &langOpts);

private:
    Replacements &m_replacements;
    const ChangePack *m_changePack;
    TUDeclDispatcher m_declDispatcher;
    TUDeclDispatcher m_recordDeclDispatcher;
    const string &m_recordName;
    vector<tuple<AccessSpecifier, SourceLocation>> m_accessSpecifiers;
    SourceLocation m_lastLocationInRecordDecl;
    SourceLocation m_firstLocationInRecordDecl;
    ASTContext *m_astContext;
    bool m_recordHandled = false;
    bool m_skipRecord = false;
};
}

EncapsulateFieldRefactoring::EncapsulateFieldRefactoring(const DeclaratorDecl *decl)
    : Refactoring(nullptr)
    , m_changePack(ChangePack::fromDeclaratorDecl(decl))
    , m_declDispatcher(decl)
    , m_recordDeclDispatcher(llvm::dyn_cast<Decl>(decl->getDeclContext()))
    , m_recordName(llvm::dyn_cast<RecordDecl>(decl->getDeclContext())->getName())
{
}

EncapsulateFieldRefactoring::~EncapsulateFieldRefactoring() = default;

Refactoring::ResultType EncapsulateFieldRefactoring::invoke(RefactoringContext *ctx)
{
    unique_ptr<EncapsulateFieldDialog> dialog(new EncapsulateFieldDialog(m_changePack.get()));
    if (dialog->exec() == 0) {
        return cancelledResult();
    }

    const ChangePack *changePack = m_changePack.get(); // C++14...
    auto declDispatcher = m_declDispatcher;
    auto recordDeclDispatcher = m_recordDeclDispatcher;
    auto recordName = m_recordName;
    return ctx->scheduleRefactoring(
        [changePack, declDispatcher, recordDeclDispatcher, recordName](RefactoringTool &tool)
        {
            Refactorings::EncapsulateField::run(tool, changePack, &declDispatcher,
                                                &recordDeclDispatcher, recordName);
            return tool.getReplacements();
        }
    );
}

namespace Refactorings
{
namespace EncapsulateField
{
using ChangePack = EncapsulateFieldRefactoring::ChangePack;

int run(RefactoringTool &tool, const ChangePack *changePack,
        const DeclarationComparator *declDispatcher,
        const DeclarationComparator *recordDeclDispatcher, const string &recordName)
{
    auto pureDeclRefExpr = declRefExpr().bind("DeclRefExpr");   // static
    auto pureMemberExpr = memberExpr().bind("MemberExpr");      // instance

    auto assignDeclRefExpr = binaryOperator(hasOperatorName("="), hasLHS(declRefExpr()))
        .bind("AssignDeclRefExpr");
    auto assignMemberExpr = binaryOperator(hasOperatorName("="), hasLHS(memberExpr()))
        .bind("AssignMemberExpr");
    auto assignCXXOperatorCallExpr =
        operatorCallExpr(
            hasOverloadedOperatorName("="),
            argumentCountIs(2),
            hasArgument(0, anyOf(
                declRefExpr(),
                memberExpr()))).bind("AssignCXXOperatorCallExpr");

    auto nonAssignDeclRefExpr =
        declRefExpr(expr().bind("DeclRefExpr"),
                    unless(anyOf(
                        hasParent(
                            binaryOperator(hasOperatorName("="),
                                           hasLHS(equalsBoundNode("DeclRefExpr")))),
                        hasParent(
                            operatorCallExpr(hasOverloadedOperatorName("="),
                                             argumentCountIs(2),
                                             hasArgument(0, equalsBoundNode("DeclRefExpr")))))));
    auto nonAssignMemberExpr =
        memberExpr(expr().bind("MemberExpr"),
                   unless(anyOf(
                       hasParent(
                           binaryOperator(hasOperatorName("="),
                                          hasLHS(equalsBoundNode("MemberExpr")))),
                       hasParent(
                           operatorCallExpr(hasOverloadedOperatorName("="),
                                            argumentCountIs(2),
                                            hasArgument(0, equalsBoundNode("MemberExpr")))))));
    // Name clash above is by design
    // Note: It may be considered to support also CompoundAssignOperator
    // rewriting a+=b to setA(getA()+b), but such implementation would have to explicitly list
    // all operators

    auto accessSpec = accessSpecDecl().bind("AccessSpecDecl");
    auto fieldDecl = decl().bind("FieldDecl");

    MatchFinder finder;
    Translator callback(tool.getReplacements(), changePack, declDispatcher,
                        recordDeclDispatcher, recordName);
    // Choose appropriate set depending on static/instance and get/set
    if (changePack->createSetter()) {
        finder.addMatcher(nonAssignDeclRefExpr, &callback);
        finder.addMatcher(nonAssignMemberExpr, &callback);
        finder.addMatcher(assignDeclRefExpr, &callback);
        finder.addMatcher(assignMemberExpr, &callback);
        finder.addMatcher(assignCXXOperatorCallExpr, &callback);
    } else {
        finder.addMatcher(pureDeclRefExpr, &callback);
        finder.addMatcher(pureMemberExpr, &callback);
    }
    finder.addMatcher(accessSpec, &callback);
    finder.addMatcher(fieldDecl, &callback);
    return tool.run(newFrontendActionFactory(&finder).get());
}
}
}

QString EncapsulateFieldRefactoring::name() const
{
    return i18n("encapsulate [%1]").arg(QString::fromStdString(m_changePack->fieldName()));
}

Translator::Translator(Replacements &replacements, const ChangePack *changePack,
                       const DeclarationComparator *declDispatcher,
                       const DeclarationComparator *recordDeclDispatcher, const string &recordName)
    : m_replacements(replacements)
    , m_changePack(changePack)
    , m_declDispatcher(declDispatcher)
    , m_recordDeclDispatcher(recordDeclDispatcher)
    , m_recordName(recordName)
{
}

void Translator::run(const MatchFinder::MatchResult &Result)
{
    // Name clash is used here
    auto declRef = Result.Nodes.getNodeAs<DeclRefExpr>("DeclRefExpr");
    if (declRef && m_declDispatcher.equivalent(declRef->getDecl())) {
        handleDeclRefExpr(declRef, Result.SourceManager, Result.Context->getLangOpts());
        return;
    }
    auto memberExpr = Result.Nodes.getNodeAs<MemberExpr>("MemberExpr");
    if (memberExpr && m_declDispatcher.equivalent(memberExpr->getMemberDecl())) {
        handleMemberExpr(memberExpr, Result.SourceManager, Result.Context->getLangOpts());
        return;
    }
    auto assignDeclRefExpr = Result.Nodes.getNodeAs<BinaryOperator>("AssignDeclRefExpr");
    if (assignDeclRefExpr && m_declDispatcher.equivalent(
        llvm::dyn_cast<DeclRefExpr>(assignDeclRefExpr->getLHS())->getDecl())) {
        handleAssignToDeclRefExpr(assignDeclRefExpr, Result.SourceManager,
                                  Result.Context->getLangOpts());
        return;
    }
    auto assignMemberExpr = Result.Nodes.getNodeAs<BinaryOperator>("AssignMemberExpr");
    if (assignMemberExpr && m_declDispatcher.equivalent(
        llvm::dyn_cast<MemberExpr>(assignMemberExpr->getLHS())->getMemberDecl())) {
        handleAssignToMemberExpr(assignMemberExpr, Result.SourceManager,
                                 Result.Context->getLangOpts());
        return;
    }
    auto assignOperatorCall = Result.Nodes.getNodeAs<CXXOperatorCallExpr>(
        "AssignCXXOperatorCallExpr");
    if (assignOperatorCall) {
        auto assignee = assignOperatorCall->getArg(0);
        const Decl *decl;
        if (llvm::isa<MemberExpr>(assignee)) {
            decl = llvm::dyn_cast<MemberExpr>(assignee)->getMemberDecl();
        } else {
            Q_ASSERT(llvm::isa<DeclRefExpr>(assignee));
            decl = llvm::dyn_cast<DeclRefExpr>(assignee)->getDecl();
        }
        if (m_declDispatcher.equivalent(decl)) {
            handleAssignOperatorCall(assignOperatorCall, Result.SourceManager,
                                     Result.Context->getLangOpts());
        }
        return;
    }
    auto accessSpecDecl = Result.Nodes.getNodeAs<AccessSpecDecl>("AccessSpecDecl");
    if (accessSpecDecl && m_recordDeclDispatcher.equivalent(accessSpecDecl->getDeclContext())) {
        if (!m_skipRecord) {
            handleAccessSpecDecl(accessSpecDecl, Result.SourceManager,
                                 Result.Context->getLangOpts());
        }
        return;
    }
    auto fieldDecl = Result.Nodes.getNodeAs<Decl>("FieldDecl");
    if (fieldDecl && m_declDispatcher.equivalent(fieldDecl)) {
        if (!m_skipRecord) {
            handleFieldDecl(fieldDecl, Result.SourceManager, Result.Context->getLangOpts());
        }
        return;
    }
}

void Translator::handleDeclRefExpr(const DeclRefExpr *declRefExpr, SourceManager *sourceManager,
                                   const LangOptions &langOpts)
{
    m_replacements.insert(
        Replacement(*sourceManager, CharSourceRange::getTokenRange(declRefExpr->getLocation()),
                    m_changePack->getterName() + "()"));
    // TODO Clang 3.7: use @p langOpts above
}

void Translator::handleMemberExpr(const MemberExpr *memberExpr, SourceManager *sourceManager,
                                  const LangOptions &langOpts)
{
    m_replacements.insert(
        Replacement(*sourceManager, CharSourceRange::getTokenRange(memberExpr->getMemberLoc()),
                    m_changePack->getterName() + "()"));
    // TODO Clang 3.7: use @p langOpts above
}

void Translator::handleAssignToDeclRefExpr(const BinaryOperator *assignmentOperator,
                                           SourceManager *sourceManager,
                                           const LangOptions &langOpts)
{
    m_replacements.insert(
        Replacement(*sourceManager,
                    CharSourceRange::getTokenRange(
                        llvm::dyn_cast<DeclRefExpr>(assignmentOperator->getLHS())->getLocation(),
                        assignmentOperator->getLocEnd()),
                    m_changePack->setterName() + "(" +
                    codeFromASTNode(assignmentOperator->getRHS(), *sourceManager, langOpts) + ")"));
}

void Translator::handleAssignToMemberExpr(const BinaryOperator *assignmentOperator,
                                          SourceManager *sourceManager,
                                          const LangOptions &langOpts)
{
    m_replacements.insert(
        Replacement(*sourceManager,
                    CharSourceRange::getTokenRange(
                        llvm::dyn_cast<MemberExpr>(assignmentOperator->getLHS())->getMemberLoc(),
                        assignmentOperator->getLocEnd()),
                    m_changePack->setterName() + "(" +
                    codeFromASTNode(assignmentOperator->getRHS(), *sourceManager, langOpts) + ")"));
}

void Translator::handleAssignOperatorCall(const CXXOperatorCallExpr *operatorCallExpr,
                                          SourceManager *sourceManager,
                                          const LangOptions &langOpts)
{
    auto assignee = operatorCallExpr->getArg(0);
    SourceLocation location;
    if (llvm::isa<MemberExpr>(assignee)) {
        location = llvm::dyn_cast<MemberExpr>(assignee)->getMemberLoc();
    } else {
        Q_ASSERT(llvm::isa<DeclRefExpr>(assignee));
        location = llvm::dyn_cast<DeclRefExpr>(assignee)->getLocation();
    }
    m_replacements.insert(
        Replacement(*sourceManager,
                    CharSourceRange::getTokenRange(location, operatorCallExpr->getLocEnd()),
                    m_changePack->setterName() + "(" +
                    codeFromASTNode(operatorCallExpr->getArg(1), *sourceManager, langOpts) + ")"));
}

void Translator::onEndOfTranslationUnit()
{
    if (!m_skipRecord && m_recordHandled) {
        m_skipRecord = true;

        // insert code
        auto accessComparator = [](AccessSpecifier spec)
        {
            return [spec](const tuple<AccessSpecifier, SourceLocation> &t)
            {
                return get<0>(t) == spec;
            };
        };
        auto locationFromIterator = [this](
            decltype(m_accessSpecifiers)::reverse_iterator i)
        {
            if (i != m_accessSpecifiers.rbegin()) {
                return get<1>(*(i - 1));
            } else {
                return m_lastLocationInRecordDecl;
            }
        };
        SourceLocation publicLocation;
        SourceLocation protectedLocation;
        SourceLocation privateLocation = !m_accessSpecifiers.empty() ? get<1>(m_accessSpecifiers[0])
                                                                     : m_lastLocationInRecordDecl;
        auto i = find_if(m_accessSpecifiers.rbegin(), m_accessSpecifiers.rend(),
                         accessComparator(AccessSpecifier::AS_public));
        if (i != m_accessSpecifiers.rend()) {
            publicLocation = locationFromIterator(i);
        }
        i = find_if(m_accessSpecifiers.rbegin(), m_accessSpecifiers.rend(),
                    accessComparator(AccessSpecifier::AS_protected));
        if (i != m_accessSpecifiers.rend()) {
            protectedLocation = locationFromIterator(i);
        }
        i = find_if(m_accessSpecifiers.rbegin(), m_accessSpecifiers.rend(),
                    accessComparator(AccessSpecifier::AS_private));
        if (i != m_accessSpecifiers.rend()) {
            privateLocation = locationFromIterator(i);
        }

        if (m_changePack->getterAccess() == AS_public ||
            (m_changePack->createSetter() && m_changePack->setterAccess() == AS_public)) {
            string change;
            if (m_changePack->getterAccess() == AS_public) {
                change = m_changePack->accessorCode();
            }
            if (m_changePack->createSetter() && m_changePack->setterAccess() == AS_public) {
                change += "\n" + m_changePack->mutatorCode();
            }
            if (publicLocation.isInvalid()) {
                publicLocation = m_firstLocationInRecordDecl;
                change = "public:\n" + change + "private:\n";
            }
            m_replacements.insert(
                Replacement(m_astContext->getSourceManager(), publicLocation, 0, change + "\n"));
        }
        if (m_changePack->getterAccess() == AS_protected ||
            (m_changePack->createSetter() && m_changePack->setterAccess() == AS_protected)) {
            string change;
            if (m_changePack->getterAccess() == AS_protected) {
                change = m_changePack->accessorCode();
            }
            if (m_changePack->createSetter() && m_changePack->setterAccess() == AS_protected) {
                change += "\n" + m_changePack->mutatorCode();
            }
            if (protectedLocation.isInvalid()) {
                if (publicLocation.isInvalid()) {
                    publicLocation = m_firstLocationInRecordDecl;
                }
                protectedLocation = publicLocation;
                change = "protected:\n" + change + "private:\n";
            }
            m_replacements.insert(
                Replacement(m_astContext->getSourceManager(), protectedLocation, 0, change + "\n"));
        }
        if (m_changePack->getterAccess() == AS_private ||
            (m_changePack->createSetter() && m_changePack->setterAccess() == AS_private)) {
            string change;
            if (m_changePack->getterAccess() == AS_private) {
                change = m_changePack->accessorCode();
            }
            if (m_changePack->createSetter() && m_changePack->setterAccess() == AS_private) {
                change += "\n" + m_changePack->mutatorCode();
            }
            // It is always possible to get private location (FIXME: but beware difference struct/class)
            m_replacements.insert(
                Replacement(m_astContext->getSourceManager(), privateLocation, 0, change + "\n"));
        }

        m_accessSpecifiers.clear();
        m_accessSpecifiers.shrink_to_fit(); // This vector will not be used anymore
    }
}

void Translator::handleAccessSpecDecl(const AccessSpecDecl *accessSpecDecl,
                                      SourceManager *sourceManager, const LangOptions &langOpts)
{
    Q_UNUSED(sourceManager);
    m_recordHandled = true;
    m_accessSpecifiers.emplace_back(accessSpecDecl->getAccess(), accessSpecDecl->getLocStart());
}

static string describeAccessSpecifier(AccessSpecifier accessSpecifier)
{
    switch (accessSpecifier) {
    case AS_public:
        return "public";
    case AS_protected:
        return "protected";
    case AS_private:
        return "private";
    default:
        Q_ASSERT(false && "This code is not meant to work on AS_none access specifier");
        return "none";
    }
}

namespace clang
{
namespace arcmt
{
namespace trans
{
// From ARCMigrate
SourceLocation findSemiAfterLocation(SourceLocation loc, ASTContext &Ctx,
                                     bool IsDecl = false);
}
}
}

using clang::arcmt::trans::findSemiAfterLocation;

void Translator::handleFieldDecl(const Decl *fieldDecl, SourceManager *sourceManager,
                                 const LangOptions &langOpts)
{
    m_recordHandled = true;
    auto oldAccess = fieldDecl->getAccess();
    if (oldAccess != AS_private) {
        string oldAccessSpec = describeAccessSpecifier(oldAccess) + ":";
        m_replacements.insert(
            Replacement(*sourceManager,
                        findSemiAfterLocation(fieldDecl->getLocEnd(), fieldDecl->getASTContext(),
                                              true),
                        1, ";\n\n" + oldAccessSpec));
        m_replacements.insert(
            Replacement(*sourceManager, fieldDecl->getLocStart(), 0, "private:\n"));
        // skipping whitespaces (to some extent...) above would be nice, but also unwisely complicated
    }
    auto recordDecl = llvm::dyn_cast<RecordDecl>(fieldDecl->getDeclContext());
    m_lastLocationInRecordDecl = recordDecl->getRBraceLoc();
    for (auto decl : recordDecl->decls()) {
        if (decl->getLocStart() != recordDecl->getLocStart()) {
            m_firstLocationInRecordDecl = decl->getLocStart();
            break;
        }
    }
    if (m_firstLocationInRecordDecl.isInvalid()) {
        m_firstLocationInRecordDecl = m_lastLocationInRecordDecl;
    }
    m_astContext = &fieldDecl->getASTContext();
}

