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

// KF5
#include <KTextEditor/View>

// Clang
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>

#include "refactoringmanager.h"
#include "kdevrefactorings.h"
#include "refactoringcontext.h"
#include "documentcache.h"
#include "utils.h"
#include "renamevardeclrefactoring.h"
#include "renamefielddeclrefactoring.h"
#include "renamefielddeclturefactoring.h"
#include "declarationcomparator.h"
#include "changesignaturerefactoring.h"
#include "encapsulatefieldrefactoring.h"
#include "extractvariablerefactoring.h"
#include "debug.h"

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace
{

class ExplorerASTConsumer;

class ExplorerActionFactory;

class ExplorerRecursiveASTVisitor : public RecursiveASTVisitor<ExplorerRecursiveASTVisitor>
{
public:
    ExplorerRecursiveASTVisitor(ExplorerASTConsumer &ASTConsumer)
        : m_ASTConsumer(ASTConsumer)
    {
    }

    bool VisitDeclRefExpr(DeclRefExpr *declRefExpr);

    bool VisitVarDecl(VarDecl *varDecl);

    bool VisitFieldDecl(FieldDecl *fieldDecl);

    bool VisitMemberExpr(MemberExpr *memberExpr);

    bool VisitFunctionDecl(FunctionDecl *functionDecl);

    // TODO: unify renaming refactorings - they have _very_ similar implementation

private:
    bool isInRange(SourceRange range) const;

    bool isInRange(SourceLocation location) const;

    template<class Node>
    llvm::StringRef fileName(const Node &node) const;

    template<class Node>
    unsigned fileOffset(const Node &node) const;

    Refactoring *renameVarDeclRefactoring(const VarDecl *varDecl) const;

    Refactoring *renameFieldDeclRefactoring(const FieldDecl *fieldDecl) const;

    Refactoring *changeSignatureRefactoring(const FunctionDecl *functionDecl) const;

    Refactoring *encapsulateFieldRefactoring(const DeclaratorDecl *decl) const;

    /// Request ClangTool to stop after this translation unit
    void done();

    void addRefactoring(Refactoring *refactoring);

private:
    ExplorerASTConsumer &m_ASTConsumer;
};

class ExplorerASTConsumer : public ASTConsumer
{
    friend class ExplorerRecursiveASTVisitor;

public:
    ExplorerASTConsumer(ExplorerActionFactory &factory, CompilerInstance &CI);

    virtual void HandleTranslationUnit(ASTContext &Ctx) override;

private:
    ExplorerRecursiveASTVisitor m_visitor;
    ExplorerActionFactory &m_factory;
    CompilerInstance &m_CI;
};

class ExplorerAction : public ASTFrontendAction
{
public:
    ExplorerAction(ExplorerActionFactory &factory)
        : m_factory(factory)
    {
    }

protected:
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                           StringRef InFile) override;

private:
    ExplorerActionFactory &m_factory;
};

class ExplorerActionFactory : public FrontendActionFactory
{
    friend class ExplorerRecursiveASTVisitor;

    friend class ExplorerASTConsumer;

public:
    ExplorerActionFactory(const std::string &fileName, unsigned offset)
        : m_fileName(fileName)
        , m_offset(offset)
    {
    }

    virtual clang::FrontendAction *create();

    bool wantStop() const
    {
        return m_stop;
    }

    std::vector<Refactoring *> m_refactorings;
private:
    const std::string &m_fileName;
    const unsigned m_offset;
    bool m_stop = false;  // no need to parse more source files
};

class ExprRangeRefactorings : public MatchFinder::MatchCallback
{
public:
    ExprRangeRefactorings(const string &fileName, const unsigned int rangeBegin,
                          const unsigned int rangeEnd);

    const std::vector<Refactoring *> &refactorings() const
    {
        return m_refactorings;
    }

    virtual void run(const MatchFinder::MatchResult &result) override;

    virtual void onEndOfTranslationUnit() override;

private:
    bool isInRange(const MatchFinder::MatchResult &result, SourceRange range) const;
    bool isInRange(const MatchFinder::MatchResult &result,
                   SourceLocation location) const;

private:
    const std::string &m_fileName;
    const Expr *m_expr = nullptr;
    ASTContext *m_astContext = nullptr;
    SourceManager *m_sourceManager = nullptr;
    std::vector<Refactoring *> m_refactorings;
    const unsigned m_rangeBegin;
    const unsigned m_rangeEnd;
};

}

#include "contextmenumutator.h"

RefactoringManager::RefactoringManager(KDevRefactorings *parent)
    : QObject(parent)
{
    qRegisterMetaType<ContextMenuMutator *>();
    qRegisterMetaType<QVector<Refactoring *>>();
}

KDevRefactorings *RefactoringManager::parent()
{
    return static_cast<KDevRefactorings *>(QObject::parent());
}

void RefactoringManager::fillContextMenu(KDevelop::ContextMenuExtension &extension,
                                         KDevelop::EditorContext *context)
{
    const string filename = context->url().toLocalFile().toStdString();
    unsigned offset;
    {
        auto _offset = parent()->refactoringContext()->offset(filename, context->position());
        if (!_offset) {
            parent()->refactoringContext()->reportError(_offset.getError());
            return;
        }
        offset = _offset.get();
    }
    auto selection = context->view()->selectionRange();

    auto mutator = new ContextMenuMutator(extension, this);
    auto endMutating = [mutator](const QVector<Refactoring *> &result)
    {
        mutator->endFillingContextMenu(result);
    };
    QThread *mainThread = thread(); // Only for lambda below
    if (!selection.isValid()) {
        parent()->refactoringContext()->scheduleOnSingleFile(
            [filename, offset, mainThread](RefactoringTool &clangTool)
            {
                auto faf = cpp::make_unique<ExplorerActionFactory>(filename, offset);
                clangTool.run(faf.get());
                for (Refactoring *r : faf->m_refactorings) {
                    r->moveToThread(mainThread);
                }
                return QVector<Refactoring *>::fromStdVector(faf->m_refactorings);
            }, filename, endMutating);
    } else {
        auto offset1 = parent()->refactoringContext()->offset(filename, selection.start());
        auto offset2 = parent()->refactoringContext()->offset(filename, selection.end());
        if (!offset1) {
            parent()->refactoringContext()->reportError(offset1.getError());
            return;
        } else if (!offset2) {
            parent()->refactoringContext()->reportError(offset2.getError());
            return;
        }
        parent()->refactoringContext()->scheduleOnSingleFile(
            [filename, offset1, offset2, mainThread](RefactoringTool &tool)
            {
                auto exprMatcher = expr().bind("Expr");
                ExprRangeRefactorings refactorings(filename, offset1.get(), offset2.get());
                MatchFinder finder;
                finder.addMatcher(exprMatcher, &refactorings);
                tool.run(newFrontendActionFactory(&finder).get());

                QVector<Refactoring *> result =
                    QVector<Refactoring *>::fromStdVector(refactorings.refactorings());
                for (auto refactoring : result) {
                    refactoring->moveToThread(mainThread);
                }
                return result;
            }, filename, endMutating);
    }
}


ExplorerASTConsumer::ExplorerASTConsumer(ExplorerActionFactory &factory, CompilerInstance &CI)
    : m_visitor(*this)
    , m_factory(factory)
    , m_CI(CI)
{
}

std::unique_ptr<ASTConsumer> ExplorerAction::CreateASTConsumer(CompilerInstance &CI,
                                                               StringRef InFile)
{
    Q_UNUSED(InFile);
    // try to fail here if work is done
    if (m_factory.wantStop()) {
        return nullptr;
    } else {
        return std::unique_ptr<ASTConsumer>(new ExplorerASTConsumer(m_factory, CI));
    }
}

void ExplorerASTConsumer::HandleTranslationUnit(ASTContext &Ctx)
{
    m_visitor.TraverseTranslationUnitDecl(Ctx.getTranslationUnitDecl());
}

clang::FrontendAction *ExplorerActionFactory::create()
{
    return new ExplorerAction(*this);
}

bool ExplorerRecursiveASTVisitor::isInRange(SourceRange range) const
{
    return ::isInRange(m_ASTConsumer.m_factory.m_fileName, m_ASTConsumer.m_factory.m_offset, range,
                       m_ASTConsumer.m_CI.getSourceManager());
}

bool ExplorerRecursiveASTVisitor::isInRange(SourceLocation location) const
{
    return isInRange(tokenRangeToCharRange(location, m_ASTConsumer.m_CI));
}

template<class Node>
llvm::StringRef ExplorerRecursiveASTVisitor::fileName(const Node &node) const
{
    auto file = m_ASTConsumer.m_CI.getSourceManager().getFilename(
        node->getSourceRange().getBegin());
    Q_ASSERT(!file.empty());
    return file;
}

template<class Node>
unsigned ExplorerRecursiveASTVisitor::fileOffset(const Node &node) const
{
    return m_ASTConsumer.m_CI.getSourceManager().getFileOffset(node->getSourceRange().getBegin());
}

void ExplorerRecursiveASTVisitor::done()
{
    m_ASTConsumer.m_factory.m_stop = true;
}

void ExplorerRecursiveASTVisitor::addRefactoring(Refactoring *refactoring)
{
    m_ASTConsumer.m_factory.m_refactorings.push_back(refactoring);
}

////////////////////// DECISIONS ARE MADE BELOW ///////////////////////

Refactoring *ExplorerRecursiveASTVisitor::renameVarDeclRefactoring(const VarDecl *varDecl) const
{
    auto name = varDecl->getName().str();
    return new RenameVarDeclRefactoring(declarationComparator(varDecl), name);
}

bool ExplorerRecursiveASTVisitor::VisitDeclRefExpr(DeclRefExpr *declRefExpr)
{
    auto range = tokenRangeToCharRange(declRefExpr->getLocation(), m_ASTConsumer.m_CI);
    if (isInRange(range)) {
        done();
        const VarDecl *varDecl = llvm::dyn_cast<VarDecl>(declRefExpr->getDecl());
        if (!varDecl) {
            refactorDebug() << "Found DeclRefExpr, but its declaration is not VarDecl";
            return true;
        }
        addRefactoring(renameVarDeclRefactoring(varDecl));
        // other options here...
    }
    return true;
}

bool ExplorerRecursiveASTVisitor::VisitVarDecl(VarDecl *varDecl)
{
    auto range = tokenRangeToCharRange(varDecl->getLocation(), m_ASTConsumer.m_CI);
    if (isInRange(range)) {
        done();
        addRefactoring(renameVarDeclRefactoring(varDecl));
        if (varDecl->isStaticDataMember()) {
            addRefactoring(encapsulateFieldRefactoring(varDecl));
        }
        // other options here...
    }
    return true;
}

Refactoring *ExplorerRecursiveASTVisitor::renameFieldDeclRefactoring(
    const FieldDecl *fieldDecl) const
{
    auto canonicalDecl = fieldDecl->getCanonicalDecl();
    if (canonicalDecl->getLinkageInternal() == ExternalLinkage) {
        // Rename basing on qualified name
        std::string qualName;
        qualName = canonicalDecl->getQualifiedNameAsString();
        refactorDebug() << "Renaming field using" << qualName << "as qualified name";
        auto name = canonicalDecl->getName().str();
        return new RenameFieldDeclRefactoring(name, std::move(qualName));
    } else {
        // Rename based on canonical declaration location
        refactorDebug() << "Renaming TU field" << canonicalDecl->getName();
        return new RenameFieldDeclTURefactoring(fileName(canonicalDecl), fileOffset(canonicalDecl),
                                                canonicalDecl->getName());
    }
}

bool ExplorerRecursiveASTVisitor::VisitFieldDecl(FieldDecl *fieldDecl)
{
    if (isInRange(fieldDecl->getLocation())) {
        done();
        addRefactoring(renameFieldDeclRefactoring(fieldDecl));
        addRefactoring(encapsulateFieldRefactoring(fieldDecl));
        // other options here...
    }
    return true;
}

bool ExplorerRecursiveASTVisitor::VisitMemberExpr(MemberExpr *memberExpr)
{
    if (isInRange(memberExpr->getMemberLoc())) {
        done();
        const FieldDecl *fieldDecl = llvm::dyn_cast<FieldDecl>(memberExpr->getMemberDecl());
        if (fieldDecl) {
            addRefactoring(renameFieldDeclRefactoring(fieldDecl));
        }
        // other options here...
    }
    return true;
}

Refactoring *ExplorerRecursiveASTVisitor::changeSignatureRefactoring(
    const FunctionDecl *functionDecl) const
{
    auto canonicalDecl = functionDecl->getCanonicalDecl();
    return new ChangeSignatureRefactoring(canonicalDecl);
}

bool ExplorerRecursiveASTVisitor::VisitFunctionDecl(FunctionDecl *functionDecl)
{
    const TypeLoc loc = functionDecl->getTypeSourceInfo()->getTypeLoc();
    Q_ASSERT(loc);
    auto range = tokenRangeToCharRange(SourceRange(functionDecl->getLocStart(), loc.getEndLoc()),
                                       m_ASTConsumer.m_CI);
    if (isInRange(range)) {
        done();
        addRefactoring(changeSignatureRefactoring(functionDecl));
        // other options here...
    }

    return true;
}

Refactoring *ExplorerRecursiveASTVisitor::encapsulateFieldRefactoring(
    const DeclaratorDecl *decl) const
{
    return new EncapsulateFieldRefactoring(decl);
}

ExprRangeRefactorings::ExprRangeRefactorings(const string &fileName, const unsigned int rangeBegin,
                                             const unsigned int rangeEnd)
    : m_fileName(fileName)
    , m_rangeBegin(rangeBegin)
    , m_rangeEnd(rangeEnd)
{
}

bool ExprRangeRefactorings::isInRange(const MatchFinder::MatchResult &result,
                                      SourceRange range) const
{
    SourceRange charRange = tokenRangeToCharRange(range, *result.SourceManager,
                                                  result.Context->getLangOpts());
    return ::isInRange(m_fileName, m_rangeBegin, charRange, *result.SourceManager) &&
           ::isInRange(m_fileName, m_rangeEnd, charRange, *result.SourceManager);
}

bool ExprRangeRefactorings::isInRange(const MatchFinder::MatchResult &result,
                                      SourceLocation location) const
{
    return isInRange(result, tokenRangeToCharRange(location, *result.SourceManager,
                                                   result.Context->getLangOpts()));
}

void ExprRangeRefactorings::run(const MatchFinder::MatchResult &result)
{
    if (!m_expr && m_astContext) {
        // refactorings populated, ignore rest
        return;
    }
    const Expr *expr = result.Nodes.getNodeAs<Expr>("Expr");
    if (isInRange(result, expr->getSourceRange())) {
        m_expr = expr; // overridden by most descent node (can be easily extended to return all...)
        m_sourceManager = result.SourceManager;
        m_astContext = result.Context;
    }
}

void ExprRangeRefactorings::onEndOfTranslationUnit()
{
    if (m_expr) {
        m_refactorings.push_back(
            new ExtractVariableRefactoring(m_expr, m_astContext, m_sourceManager));
    }
    m_expr = nullptr;
}
