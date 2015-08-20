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
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include "movefunctionrefactoring.h"
#include "declarationcomparator.h"
#include "tudecldispatcher.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace
{

class TargetRecordComparator : public DeclarationComparator
{
public:
    explicit TargetRecordComparator(const string &recordName);

    virtual bool equivalentTo(const Decl *decl) const override;

private:
    string m_recordName;
};

}

MoveFunctionRefactoring::MoveFunctionRefactoring(const CXXMethodDecl *decl, ASTContext &astContext)
    : Refactoring(nullptr)
    , m_declDispatcher(declarationComparator(decl))
{
    // decl is in source RecordDecl
    decl = decl->getCanonicalDecl();
    m_declarationInSourceIsADefinition =
        (decl->getDeclContext() == decl->getLexicalDeclContext()) &&
        decl->isThisDeclarationADefinition();
    // in such a case there is only one declaration
    m_declaration = codeFromASTNode(decl, astContext.getSourceManager(), astContext.getLangOpts());
    // NOTE: Hosted implementation would also translate DelcRefExprs and MemeberExprs inside
    // definition to reflect change of scope (refer to old scope instead of new one)
    // NOTE: current implementation will match above AST nodes anyway
    m_isStatic = decl->isStatic();
}

namespace
{

enum class Error
{
    NonStatic,
    NoTargetRecordFound,
};

class NonStaticErrorCategory : public std::error_category
{
public:
    virtual const char *name() const noexcept override;
    virtual string message(int) const noexcept override;
};

const char *NonStaticErrorCategory::name() const noexcept
{
    return "NonStaticErrorCategory";
}

string NonStaticErrorCategory::message(int) const noexcept
{
    return i18n(
        "Selected function is non-static (instance). Only moving of static functions is supported."
            " Use \"make static\" refactoring.").toStdString();
}

class NoTargetRecordFoundErrorCategory : public std::error_category
{
public:
    virtual const char *name() const noexcept override;
    virtual string message(int) const noexcept override;
};

const char *NoTargetRecordFoundErrorCategory::name() const noexcept
{
    return "NoTargetRecordFoundErrorCategory";
}

string NoTargetRecordFoundErrorCategory::message(int) const noexcept
{
    return i18n("Unable to locate target record").toStdString();
    // And also unable to provide more information here - error_category is stateless
}

static const NonStaticErrorCategory nonStaticErrorCategory{};
static const NoTargetRecordFoundErrorCategory noTargetRecordFoundErrorCategory{};

std::error_code make_error_code(Error error)
{
    switch (error) {
    case Error::NonStatic:
        return error_code(0, nonStaticErrorCategory);
    case Error::NoTargetRecordFound:
        return error_code(0, noTargetRecordFoundErrorCategory);
    default:
        Q_ASSERT_X(false, "make_error_code(Error)", "Non exhaustive match");
    }
}

}

namespace std
{

template<>
struct is_error_code_enum<Error> : public true_type
{
};

}

Refactoring::ResultType MoveFunctionRefactoring::invoke(RefactoringContext *ctx)
{
    if (!m_isStatic) {
        return Error::NonStatic;
    }
    if (m_declarationInSourceIsADefinition) {
        ctx->reportInformation(i18n("Definition of this function is inside of a class. "
                                        "Transformation of its implementation is not supported. "
                                        "Code may require manual fixes after refactoring."));
    }
    QString targetRecordName = QInputDialog::getText(
        nullptr, i18n("Move"), i18n("Type fully qualified name of target record"));
    if (targetRecordName.isEmpty()) {
        return cancelledResult();
    }
    return ctx->scheduleRefactoringWithError(
        [this, targetRecordName](RefactoringTool &tool)
        {
            return doRefactoring(tool, targetRecordName.toStdString());
        }
    );
}

QString MoveFunctionRefactoring::name() const
{
    return i18n("Move");
}

class MoveFunctionRefactoring::Callback : public MatchFinder::MatchCallback
{
public:
    Callback(Replacements &replacements, const string &recordName, const string &declarationCode,
             TUDeclDispatcher &declDispatcher, TUDeclDispatcher &targetRecordDispatcher,
             bool declarationIsADefinition);

    virtual void run(const MatchFinder::MatchResult &result) override;

    bool foundTargetRecord() const
    {
        return m_foundTargetRecord;
    }

private:
    void handleMethodDecl(const CXXMethodDecl *methodDecl, SourceManager *sourceManager,
                          ASTContext *astContext);
    void handleTargetRecordDecl(const RecordDecl *targetRecordDecl, SourceManager *sourceManager,
                                ASTContext *astContext);
    void handleDeclRefExpr(const DeclRefExpr *declRefExpr, SourceManager *sourceManager,
                           ASTContext *astContext);
    void handleMemberExpr(const MemberExpr *memberExpr, SourceManager *sourceManager,
                          ASTContext *astContext);

private:
    Replacements &m_replacements;
    const string &m_targetRecordName;
    const string &m_declaration;
    TUDeclDispatcher &m_declDispatcher;
    TUDeclDispatcher &m_targetRecordDispatcher;
    bool m_declarationIsADefinition;
    bool m_foundTargetRecord = false;
};

Refactoring::ResultType MoveFunctionRefactoring::doRefactoring(RefactoringTool &tool,
                                                               const string &targetRecord)
{
    auto methodDeclMatcher = methodDecl().bind("CXXMethodDecl");
    auto targetRecordDeclMatcher = recordDecl().bind("RecordDecl");
    auto declRefExprMatcher = declRefExpr().bind("DeclRefExpr");
    auto memberExprMatcher = memberExpr().bind("MemberExpr");
    Replacements replacements;
    TUDeclDispatcher declDispatcher(m_declDispatcher.get());
    TargetRecordComparator targetRecordComparator(targetRecord);
    TUDeclDispatcher targetRecordDispatcher(&targetRecordComparator);
    Callback callback(replacements, targetRecord, m_declaration, declDispatcher,
                      targetRecordDispatcher, m_declarationInSourceIsADefinition);
    MatchFinder finder;
    finder.addMatcher(methodDeclMatcher, &callback);
    finder.addMatcher(targetRecordDeclMatcher, &callback);
    finder.addMatcher(declRefExprMatcher, &callback);
    finder.addMatcher(memberExprMatcher, &callback);
    tool.run(newFrontendActionFactory(&finder).get());
    if (callback.foundTargetRecord()) {
        return replacements;
    } else {
        return Error::NoTargetRecordFound;
    }
}

TargetRecordComparator::TargetRecordComparator(const string &recordName)
    : m_recordName(recordName)
{
}

bool TargetRecordComparator::equivalentTo(const clang::Decl *decl) const
{
    auto recordDecl = llvm::dyn_cast<RecordDecl>(decl);
    if (!recordDecl) {
        return false;
    }
    return recordDecl->getQualifiedNameAsString() == m_recordName;
}

MoveFunctionRefactoring::Callback::Callback(Replacements &replacements, const string &recordName,
                                            const string &declarationCode,
                                            TUDeclDispatcher &declDispatcher,
                                            TUDeclDispatcher &targetRecordDispatcher,
                                            bool declarationIsADefinition)
    : m_replacements(replacements)
    , m_targetRecordName(recordName)
    , m_declaration(declarationCode)
    , m_declDispatcher(declDispatcher)
    , m_targetRecordDispatcher(targetRecordDispatcher)
    , m_declarationIsADefinition(declarationIsADefinition)
{
}

void MoveFunctionRefactoring::Callback::run(const MatchFinder::MatchResult &result)
{
    auto methodDecl = result.Nodes.getNodeAs<CXXMethodDecl>("CXXMethodDecl");
    if (methodDecl && m_declDispatcher.equivalent(methodDecl)) {
        handleMethodDecl(methodDecl, result.SourceManager, result.Context);
        return;
    }
    auto recordDecl = result.Nodes.getNodeAs<RecordDecl>("RecordDecl");
    if (recordDecl && m_targetRecordDispatcher.equivalent(recordDecl)) {
        handleTargetRecordDecl(recordDecl, result.SourceManager, result.Context);
        return;
    }
    auto declRefExpr = result.Nodes.getNodeAs<DeclRefExpr>("DeclRefExpr");
    if (declRefExpr && m_declDispatcher.equivalent(declRefExpr->getDecl())) {
        handleDeclRefExpr(declRefExpr, result.SourceManager, result.Context);
        return;
    }
    auto memberExpr = result.Nodes.getNodeAs<MemberExpr>("MemberExpr");
    if (memberExpr && m_declDispatcher.equivalent(memberExpr->getMemberDecl())) {
        handleMemberExpr(memberExpr, result.SourceManager, result.Context);
        return;
    }
    Q_ASSERT(methodDecl || recordDecl || declRefExpr || memberExpr); // any of * not null
}

namespace
{

class TransformImplementationVisitor : public RecursiveASTVisitor<TransformImplementationVisitor>
{
public:
    TransformImplementationVisitor(Replacements &replacements,
                                   const string &targetNestedNameSpecifier,
                                   const CXXMethodDecl *sourceMethodDecl,
                                   const SourceManager &sourceManger);

    bool VisitDeclRefExpr(const DeclRefExpr *declRefExpr);
    bool VisitMemberExpr(const MemberExpr *memberExpr);

private:

private:
    Replacements &m_replacements;
    const CXXMethodDecl *m_sourceMethodDecl;
    const RecordDecl *m_sourceRecordDecl;
    string m_sourceNestedNameSpecifier;
    string m_targetNestedNameSpecifier;
    unique_ptr<DeclarationComparator> m_sourceMethodDeclChain;
    unique_ptr<DeclarationComparator> m_sourceRecordDeclChain;
    TUDeclDispatcher m_sourceMethodDeclDispatcher;
    TUDeclDispatcher m_sourceRecordDeclDispatcher;
    const SourceManager &sourceManager;
};

}

void MoveFunctionRefactoring::Callback::handleMethodDecl(const CXXMethodDecl *methodDecl,
                                                         SourceManager *sourceManager,
                                                         ASTContext *astContext)
{
    Q_UNUSED(astContext);
    if (methodDecl->getDeclContext() == methodDecl->getLexicalDeclContext()) {
        // inside old class - remove body
        if (methodDecl->isThisDeclarationADefinition()) {
            m_replacements.insert(Replacement(*sourceManager, methodDecl->getBody(), ";"));
        }
        // leave semicolon after declaration
        auto begin = methodDecl->getLocStart();
        auto end = methodDecl->getTypeSourceInfo()->getTypeLoc().castAs<FunctionTypeLoc>()
            .getBeginLoc();
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getCharRange(begin, end), "friend "));
        m_replacements.insert(Replacement(*sourceManager, methodDecl->getLocation(), 0,
                                          "::" + m_targetRecordName + "::"));
        // leave friend declaration
        // FIXME: transform removed body (in handleTargetRecordDecl) (replacements on removed code)
    } else {
        // adjust parent decl context (by changing nested name specifier) or fix friend declaration
        // workaround seemingly bug in Clang - use "::" from code instead
        m_replacements.insert(
            Replacement(*sourceManager, CharSourceRange::getCharRange(
                methodDecl->getQualifierLoc().getSourceRange()), "::" + m_targetRecordName));
        Q_ASSERT(methodDecl->isThisDeclarationADefinition());
        // declaration of static method outside of a class definition as a definition
        TransformImplementationVisitor visitor(m_replacements, "::" + m_targetRecordName + "::",
                                               methodDecl, *sourceManager);
        visitor.TraverseStmt(methodDecl->getBody());
    }
}

TransformImplementationVisitor::TransformImplementationVisitor(
    Replacements &replacements, const string &targetNestedNameSpecifier,
    const CXXMethodDecl *sourceMethodDecl,
    const SourceManager &sourceManger)
    : m_replacements(replacements)
    , m_sourceMethodDecl(sourceMethodDecl)
    , m_sourceRecordDecl(sourceMethodDecl->getParent())
    , m_sourceNestedNameSpecifier("::" + m_sourceRecordDecl->getQualifiedNameAsString() + "::")
    , m_targetNestedNameSpecifier(targetNestedNameSpecifier)
    , m_sourceMethodDeclChain(declarationComparator(sourceMethodDecl))
    , m_sourceRecordDeclChain(declarationComparator(m_sourceRecordDecl))
    , m_sourceMethodDeclDispatcher(m_sourceMethodDeclChain.get())
    , m_sourceRecordDeclDispatcher(m_sourceRecordDeclChain.get())
    , sourceManager(sourceManger)
{
    // references to m_sourceRecordDecl->getQualifiedNameAsString() are not fully stable
    // it is also limitation of this refactoring - source class must also be visible everywhere
    // (have "clean" qualifier)
}

bool TransformImplementationVisitor::VisitDeclRefExpr(const DeclRefExpr *declRefExpr)
{
    if (m_sourceMethodDeclDispatcher.equivalent(declRefExpr->getDecl())) {
        // recursion - redirect to new method (if required)
        // NOTE: this work can be duplicated by handlers in Callback
        auto qualifierLoc = declRefExpr->getQualifierLoc();
        if (qualifierLoc) {
            string trimmedQualifier = m_targetNestedNameSpecifier.substr(
                0, m_targetNestedNameSpecifier.length() - 2);
            // workaround seemingly bug in Clang - use "::" from code instead
            m_replacements.insert(Replacement(sourceManager, CharSourceRange::getCharRange(
                qualifierLoc.getSourceRange()), trimmedQualifier));
            // if have qualifier - use new one instead
        }
    } else if (m_sourceRecordDeclDispatcher.equivalent(declRefExpr->getDecl()->getDeclContext())) {
        if (!declRefExpr->getQualifierLoc()) {
            m_replacements.insert(Replacement(sourceManager, declRefExpr->getLocation(), 0,
                                              m_sourceNestedNameSpecifier));
        }
    }
    return true;
}

bool TransformImplementationVisitor::VisitMemberExpr(const MemberExpr *memberExpr)
{
    // It is rare case but possible
    // recursion is redirected
    // NOTE: this work can be duplicated by handlers in Callback
    if (m_sourceMethodDeclDispatcher.equivalent(memberExpr->getMemberDecl())) {
        m_replacements.insert(Replacement(sourceManager,
                                          CharSourceRange::getCharRange(memberExpr->getLocStart(),
                                                                        memberExpr->getMemberLoc()),
                                          m_targetNestedNameSpecifier));
    }
    // in other cases MemberExpr serves as qualifier for DeclRefExprs
    return true;
}

void MoveFunctionRefactoring::Callback::handleTargetRecordDecl(const RecordDecl *targetRecordDecl,
                                                               SourceManager *sourceManager,
                                                               ASTContext *astContext)
{
    Q_UNUSED(astContext);
    if (!targetRecordDecl->isThisDeclarationADefinition()) {
        return;
    }
    string code = "\npublic:\n\t";
    code += m_declaration;
    if (!m_declarationIsADefinition) {
        code += ";";
    }
    code += "\n";
    m_replacements.insert(Replacement(*sourceManager, targetRecordDecl->getRBraceLoc(), 0, code));
    m_foundTargetRecord = true;
}

void MoveFunctionRefactoring::Callback::handleDeclRefExpr(const DeclRefExpr *declRefExpr,
                                                          SourceManager *sourceManager,
                                                          ASTContext *astContext)
{
    Q_UNUSED(astContext);
    if (m_declDispatcher.equivalent(declRefExpr->getDecl())) {
        auto qualifierLoc = declRefExpr->getQualifierLoc();
        if (qualifierLoc) {
            // if have qualifier - use new one instead
            // workaround seemingly bug in Clang - use "::" from code instead
            m_replacements.insert(Replacement(*sourceManager, CharSourceRange::getCharRange(
                qualifierLoc.getSourceRange()), "::" + m_targetRecordName));
        } else {
            // if not - create new
            m_replacements.insert(
                Replacement(*sourceManager, declRefExpr->getLocation(), 0,
                            "::" + m_targetRecordName + "::"));
        }
    }
}

void MoveFunctionRefactoring::Callback::handleMemberExpr(const MemberExpr *memberExpr,
                                                         SourceManager *sourceManager,
                                                         ASTContext *astContext)
{
    Q_UNUSED(astContext);
    // It is rare case but possible (for example result of "make static" refactoring
    if (m_declDispatcher.equivalent(memberExpr->getMemberDecl())) {
        m_replacements.insert(Replacement(*sourceManager,
                                          CharSourceRange::getCharRange(memberExpr->getLocStart(),
                                                                        memberExpr->getMemberLoc()),
                                          "::" + m_targetRecordName + "::"));
    }
}
