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
#include <clang/AST/DeclCXX.h>
#include <clang/Tooling/Refactoring.h>
#include <QtTest>
#include "test_instancetostatic.h"
#include "refactoringenvironment.h"
#include "../refactoring/instancetostaticrefactoring.h"
#include "../refactoring/refactoringmanager.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestInstanceToStatic)


void TestInstanceToStatic::testNoThisPtr()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(int global = 5;
                     class C
                     {
                       int field;
                       int f(int var);
                       int g() { return f(global); }
                     };)";
    env.addHeader("source.h", code);
    env.addFile("source.cpp",
                R"(#include "source.h"
                   int C::f(int var)
                   {
                     return var;
                   })");

    unsigned offset = static_cast<unsigned>(code.find("f("));

    Replacements replacements;
    env.runTool(
        [&replacements, offset](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/source.h", offset, QThread::currentThread(),
                                                tool);
            InstanceToStaticRefactoring *refactoring = nullptr;
            for (Refactoring *r : refactorings) {
                if (typeid(*r) == typeid(InstanceToStaticRefactoring)) {
                    refactoring = static_cast<InstanceToStaticRefactoring *>(r);
                }
            }
            if (refactoring) {
                replacements = refactoring->doRefactoring(tool, "myName");
            }
        });

    env.verifyResult(replacements, "source.h",
                     R"(int global = 5;
                        class C
                        {
                          int field;
                          static int f(int var);
                          int g() { return f(global); }
                        };)");
    env.verifyResult(replacements, "source.cpp",
                     R"(#include "source.h"
                        int C::f(int var)
                        {
                          return var;
                        })");
}

void TestInstanceToStatic::testUseThisPtr()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(int global = 5;
                     class C
                     {
                       int field;
                       int f(int var);
                       int g() { return f(global); }
                     };)";
    env.addHeader("source.h", code);
    env.addFile("source.cpp",
                R"(#include "source.h"
                   int C::f(int var)
                   {
                     return var+field;
                   })");

    unsigned offset = static_cast<unsigned>(code.find("f("));

    Replacements replacements;
    env.runTool(
        [&replacements, offset](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/source.h", offset, QThread::currentThread(),
                                                tool);
            InstanceToStaticRefactoring *refactoring = nullptr;
            for (Refactoring *r : refactorings) {
                if (typeid(*r) == typeid(InstanceToStaticRefactoring)) {
                    refactoring = static_cast<InstanceToStaticRefactoring *>(r);
                }
            }
            if (refactoring) {
                replacements = refactoring->doRefactoring(tool, "myName");
            }
        });

    env.verifyResult(replacements, "source.h",
                     R"(int global = 5;
                        class C
                        {
                            int field;
                            static int f(C * myName, int var);
                            int g()
                            {
                                return f(this, global);
                            }
                        };)");
    env.verifyResult(replacements, "source.cpp",
                     R"(#include "source.h"
                        int C::f(C * myName, int var)
                        {
                          return var+myName->field;
                        })");
}
