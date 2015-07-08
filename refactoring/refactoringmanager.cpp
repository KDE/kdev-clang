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

#include "refactoringmanager.h"

// Clang
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Tooling.h>

#include "refactoringcontext.h"
#include "documentcache.h"
#include "utils.h"
#include "renamevardeclrefactoring.h"
#include "renamefielddeclrefactoring.h"
#include "renamefielddeclturefactoring.h"
#include "declarationcomparator.h"
#include "changesignaturerefactoring.h"
#include "debug.h"

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::tooling;

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

    Refactoring *refactoringForVarDecl(const VarDecl *varDecl) const;

    Refactoring *refactoringForFieldDecl(const FieldDecl *fieldDecl) const;

    Refactoring *refactoringForFunctionDecl(const FunctionDecl *functionDecl) const;

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
        : m_fileName(fileName),
          m_offset(offset)
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

}

RefactoringManager::RefactoringManager(QObject *parent)
    : QObject(parent)
{
}

std::vector<Refactoring *> RefactoringManager::allApplicableRefactorings(RefactoringContext *ctx,
                                                                         const QUrl &sourceFile,
                                                                         const KTextEditor::Cursor &location)
{
    const string filename = sourceFile.toLocalFile().toStdString();
    auto clangTool = ctx->cache->refactoringToolForFile(filename);
    unsigned offset;
    {
        auto _offset = ctx->offset(filename, location);
        if (!_offset) {
            // TODO: notify user
            refactorDebug() << "Unable to translate cursor position to offset in file:" <<
                            _offset.getError().message();
            return {};
        }
        offset = _offset.get();
    }
    auto faf = cpp::make_unique<ExplorerActionFactory>(filename, offset);
    clangTool.run(faf.get());
    return std::move(faf->m_refactorings);
}


ExplorerASTConsumer::ExplorerASTConsumer(ExplorerActionFactory &factory, CompilerInstance &CI)
    : m_visitor(*this),
      m_factory(factory),
      m_CI(CI)
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

Refactoring *ExplorerRecursiveASTVisitor::refactoringForVarDecl(const VarDecl *varDecl) const
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
        addRefactoring(refactoringForVarDecl(varDecl));
        // other options here...
    }
    return true;
}

bool ExplorerRecursiveASTVisitor::VisitVarDecl(VarDecl *varDecl)
{
    auto range = tokenRangeToCharRange(varDecl->getLocation(), m_ASTConsumer.m_CI);
    if (isInRange(range)) {
        done();
        addRefactoring(refactoringForVarDecl(varDecl));
        // other options here...
    }
    return true;
}

Refactoring *ExplorerRecursiveASTVisitor::refactoringForFieldDecl(const FieldDecl *fieldDecl) const
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
        addRefactoring(refactoringForFieldDecl(fieldDecl));
        // other options here...
    }
    return true;
}

bool ExplorerRecursiveASTVisitor::VisitMemberExpr(MemberExpr *memberExpr)
{
    if (isInRange(memberExpr->getMemberLoc())) {
        done();
        const FieldDecl *fieldDecl = llvm::dyn_cast<FieldDecl>(memberExpr->getMemberDecl());
        if (!fieldDecl) {
            // can be CXXMethodDecl as well, but that case is (will be) handled separately
            return true;
        }
        addRefactoring(refactoringForFieldDecl(fieldDecl));
        // other options here...
    }
    return true;
}

Refactoring *ExplorerRecursiveASTVisitor::refactoringForFunctionDecl(
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
        addRefactoring(refactoringForFunctionDecl(functionDecl));
        // other options here...
    }

    return true;
}

