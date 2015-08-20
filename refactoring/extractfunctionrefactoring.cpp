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

// C++
#include <queue>
#include <unordered_map>

// Qt
#include <QInputDialog>

// KF5
#include <KLocalizedString>

// Clang
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include "extractfunctionrefactoring.h"
#include "utils.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

namespace
{
// Collects all uses of declarations from given DeclContext
class UsesFromDeclContext : public RecursiveASTVisitor<UsesFromDeclContext>
{
public:
    UsesFromDeclContext(const DeclContext *context);

    bool VisitDeclRefExpr(const DeclRefExpr *declRef);
    bool VisitCXXThisExpr(const CXXThisExpr *cxxThisExpr);

    const DeclContext *const context;
    // These will need to be passed explicitly
    unordered_map<const ValueDecl *, const DeclRefExpr *> usedDecls;
    bool usesThis = false;
};

}

ExtractFunctionRefactoring::ExtractFunctionRefactoring(vector<Task> tasks)
    : Refactoring(nullptr)
    , m_tasks(move(tasks))
{
}

// NOTE: It may be desirable to adjust accessibility of generated member (if the generated is a member)
ExtractFunctionRefactoring *ExtractFunctionRefactoring::create(const clang::Expr *expr,
                                                               clang::ASTContext *astContext,
                                                               clang::SourceManager *sourceManager)
{
    const FunctionDecl *declContext = nullptr;
    using DynTypedNode = ast_type_traits::DynTypedNode;
    // Find closest DeclContext (BFS, getParents() as edges)
    queue<DynTypedNode> queue; // {parent, node}
    for (auto p : astContext->getParents(*expr)) {
        queue.push(p);
    }
    while (!queue.empty()) {
        auto front = queue.front();
        queue.pop();
        // DeclContext below doesn't work. why?
        if ((declContext = front.get<FunctionDecl>())) {
            decltype(queue)().swap(queue);  // break
        } else {
            for (auto p : astContext->getParents(front)) {
                queue.push(p);
            }
        }
    }
    if (declContext) {
        UsesFromDeclContext visitor(declContext);
        // matchers provide const nodes, visitors consume non-const
        visitor.TraverseStmt(const_cast<Expr *>(expr));
        string arguments = "(";
        string invocation = "(";
        for (auto entry : visitor.usedDecls) {
            arguments += toString(entry.second->getType(), astContext->getLangOpts()) + " " +
                         entry.first->getName().str();
            arguments += ", ";
            invocation += codeFromASTNode(entry.second, *sourceManager, astContext->getLangOpts());
            invocation += ", ";
        }
        if (arguments.length() > 1) {
            arguments = arguments.substr(0, arguments.length() - 2);
        }
        arguments += ")";
        if (invocation.length() > 1) {
            invocation = invocation.substr(0, invocation.length() - 2);
        }
        invocation += ")";
        vector<Task> tasks;
        {
            Replacement pattern(*sourceManager, expr, "");
            tasks.emplace_back(
                pattern.getFilePath(), pattern.getOffset(), pattern.getLength(),
                [invocation](const string &name)
                {
                    return name + invocation;
                });
        }
        string returnType = toString(expr->getType(), astContext->getLangOpts());
        const CXXMethodDecl *asMethod = llvm::dyn_cast<CXXMethodDecl>(declContext);
        const bool wantStatic = asMethod && (!visitor.usesThis);
        for (const FunctionDecl *decl : declContext->redecls()) {
            Replacement pattern(*sourceManager, decl->getLocStart(), 0, "");
            std::function<string(const string &)> replacement;
            auto qualifier = decl->getQualifierLoc();
            string qualifierS;
            if (qualifier) {
                qualifierS = codeFromASTNode(&qualifier, *sourceManager, astContext->getLangOpts());
            }
            auto lexicalParent = decl->getLexicalDeclContext();
            Q_ASSERT(lexicalParent);
            const string staticS = wantStatic && lexicalParent->isRecord() ? "static " : "";
            if (decl->isThisDeclarationADefinition()) {
                // emit full definition
                string exprString = codeFromASTNode(expr, *sourceManager,
                                                    astContext->getLangOpts());
                replacement = [staticS, returnType, qualifierS, exprString, arguments](
                    const string &name)
                {
                    string result = staticS + returnType + " " + qualifierS + name;
                    result += arguments + "\n";
                    result += "{\n";
                    result += "\treturn " + exprString + ";\n";
                    result += "}\n\n";
                    return result;
                };
            } else {
                // emit only forward declaration
                replacement = [staticS, returnType, qualifierS, arguments](const string &name)
                {
                    return staticS + returnType + " " + qualifierS + name + arguments + ";\n";
                };
            }
            tasks.emplace_back(pattern.getFilePath(), pattern.getOffset(), 0, replacement);
        }
        return new ExtractFunctionRefactoring(move(tasks));
    } else {
        return nullptr;
    }
}

Refactoring::ResultType ExtractFunctionRefactoring::invoke(RefactoringContext *ctx)
{
    // This is local refactoring, all context dependent operations done in RefactoringManager
    Q_UNUSED(ctx);

    QString funName = QInputDialog::getText(nullptr, i18n("Function Name"),
                                            i18n("Type name of new function"));
    if (funName.isEmpty()) {
        return cancelledResult();
    }
    return doRefactoring(funName.toStdString());
}


Replacements ExtractFunctionRefactoring::doRefactoring(const std::string &name)
{
    Replacements result;
    for (const Task &task : m_tasks) {
        result.insert(Replacement(task.filename, task.offset, task.length, task.replacement(name)));
    }
    return result;
}

QString ExtractFunctionRefactoring::name() const
{
    return i18n("Extract function");
}

UsesFromDeclContext::UsesFromDeclContext(const DeclContext *context)
    : context(context)
{
}

bool UsesFromDeclContext::VisitDeclRefExpr(const DeclRefExpr *declRef)
{
    auto ctx = declRef->getDecl()->getDeclContext();
    while (ctx && ctx != context) {
        ctx = ctx->getParent();
    }
    if (ctx) {
        // infer type _now_ (not to have 'auto' type specifier in generated function declaration)
        usedDecls[declRef->getDecl()] = declRef;
    }
    return true;
}

bool UsesFromDeclContext::VisitCXXThisExpr(const CXXThisExpr *cxxThisExpr)
{
    Q_UNUSED(cxxThisExpr);
    usesThis = true;
    return true;
}

ExtractFunctionRefactoring::Task::Task(const std::string &filename, unsigned offset,
                                       unsigned length,
                                       const std::function<std::string(
                                           const std::string &)> &replacement)
    : filename(filename)
    , offset(offset)
    , length(length)
    , replacement(replacement)
{
}
