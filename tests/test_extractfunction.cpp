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
#include "test_extractfunction.h"
#include "refactoringenvironment.h"
#include "../refactoring/extractfunctionrefactoring.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestExtractFunction)


void TestExtractFunction::testExtractNonMember()
{
    RefactoringEnvironment env("-std=c++11");
    string filename = "source.cpp";
    string code = R"(int global = 5;
                     int f(int a)
                     {
                       int b = 2;
                       return global+a*b;
                     })";
    env.addFile(filename, code);

    Replacements replacements;
    env.findNode<BinaryOperator>(
        [](const BinaryOperator *op)
        {
            return op->getOpcode() == BinaryOperatorKind::BO_Add;
        },
        [&replacements](const Expr *expr, ASTContext *astContext)
        {
            auto refactoring = ExtractFunctionRefactoring::create(expr, astContext,
                                                                  &astContext->getSourceManager());

            replacements = refactoring->doRefactoring("newName");
        }
    );

    env.verifyResult(replacements, filename,
                     R"(int global = 5;
                        int newName(int b, int a)
                        {
                          return global+a*b;
                        }
                        int f(int a)
                        {
                          int b = 2;
                          return newName(b, a);
                        })");
}

void TestExtractFunction::testExtractStaticFromInstance()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("source.h",
                  R"(int global = 5;
                     class C
                     {
                       int f(int a);
                     };)");
    env.addFile("source.cpp",
                R"(#include "source.h"
                   int C::f(int a)
                   {
                     int b = 2;
                     return global+a*b;
                   })");

    Replacements replacements;
    env.findNode<BinaryOperator>(
        [](const BinaryOperator *op)
        {
            return op->getOpcode() == BinaryOperatorKind::BO_Add;
        },
        [&replacements](const Expr *expr, ASTContext *astContext)
        {
            auto refactoring = ExtractFunctionRefactoring::create(expr, astContext,
                                                                  &astContext->getSourceManager());

            replacements = refactoring->doRefactoring("newName");
        }
    );

    env.verifyResult(replacements, "source.h",
                     R"(int global = 5;
                        class C
                        {
                          static int newName(int b, int a);
                          int f(int a);
                        };)");
    env.verifyResult(replacements, "source.cpp",
                     R"(#include "source.h"
                        int C::newName(int b, int a)
                        {
                          return global+a*b;
                        }
                        int C::f(int a)
                        {
                          int b = 2;
                          return newName(b, a);
                        })");
}

void TestExtractFunction::testExtractInstanceFromInstance()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("source.h",
                  R"(int global = 5;
                     class C
                     {
                       int a;
                       int f();
                     };)");
    env.addFile("source.cpp",
                R"(#include "source.h"
                   int C::f()
                   {
                     int b = 2;
                     return global+a*b;
                   })");

    Replacements replacements;
    env.findNode<BinaryOperator>(
        [](const BinaryOperator *op)
        {
            return op->getOpcode() == BinaryOperatorKind::BO_Add;
        },
        [&replacements](const Expr *expr, ASTContext *astContext)
        {
            auto refactoring = ExtractFunctionRefactoring::create(expr, astContext,
                                                                  &astContext->getSourceManager());

            replacements = refactoring->doRefactoring("newName");
        }
    );

    env.verifyResult(replacements, "source.h",
                     R"(int global = 5;
                        class C
                        {
                          int a;
                          int newName(int b);
                          int f();
                        };)");
    env.verifyResult(replacements, "source.cpp",
                     R"(#include "source.h"
                        int C::newName(int b)
                        {
                          return global+a*b;
                        }
                        int C::f()
                        {
                          int b = 2;
                          return newName(b);
                        })");
}
