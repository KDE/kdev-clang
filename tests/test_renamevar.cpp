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

#include <functional>
#include <clang/AST/Decl.h>
#include <clang/Tooling/Refactoring.h>
#include <QtTest>
#include "test_renamevar.h"
#include "refactoringenvironment.h"
#include "../refactoring/renamevardeclrefactoring.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestRenameVar)

void TestRenameVar::testFunctionParams()
{
    RefactoringEnvironment env("-std=c++11");
    env.addFile("main.cpp",
                R"(#include <iostream>
                   int main(int argc,char **argv)
                   {
                     std::cout<<argc<<std::endl;
                     return 0;
                   })");
    auto declCmp = declarationComparator(
        [](const Decl *decl)
        {
            const ParmVarDecl *varDecl = llvm::dyn_cast<ParmVarDecl>(decl);
            if (varDecl == nullptr) {
                return false;
            }
            return varDecl->getCanonicalDecl()->getName() == "argc";
        });
    auto d = declCmp.get();
    auto replacements = env.runTool(
        [d](RefactoringTool &tool)
        {
            return Refactorings::RenameVarDecl::run(d, "counter", tool);
        });
    env.verifyResult(replacements, "main.cpp",
                     R"(#include <iostream>
                        int main(int counter,char **argv)
                        {
                          std::cout<<counter<<std::endl;
                          return 0;
                        })");
}

void TestRenameVar::testGlobalVariable()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("header.h", "extern int x;");
    env.addFile("source.cpp",
                R"(#include "header.h"
                   int x = 5;
                   void f(int x) { ::x=x; })");
    auto declCmp = declarationComparator(
        [](const Decl *decl)
        {
            const VarDecl *varDecl = llvm::dyn_cast<VarDecl>(decl);
            if (varDecl == nullptr) {
                return false;
            }
            return varDecl->getCanonicalDecl()->hasExternalFormalLinkage();
        });
    auto result = env.runTool(
        [&declCmp](RefactoringTool &tool)
        {
            Refactorings::RenameVarDecl::run(declCmp.get(), "MyNewElaboratedName", tool);
        });
    env.verifyResult(result, "header.h", "extern int MyNewElaboratedName;");
    env.verifyResult(result, "source.cpp",
                     R"(#include "header.h"
                        int MyNewElaboratedName = 5;
                        void f(int x) { ::MyNewElaboratedName=x; })");
}
