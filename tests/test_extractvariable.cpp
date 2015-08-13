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
#include "test_extractvariable.h"
#include "refactoringenvironment.h"
#include "../refactoring/extractvariablerefactoring.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestExtractVariable)


void TestExtractVariable::testExtractLocal()
{
    RefactoringEnvironment env("-std=c++11");
    string filename = "source.cpp";
    string code = R"(int f()
                     {
                       return 2+2*2;
                     })";
    env.addFile(filename, code);

    string expression = "2+2";
    auto expressionOffset = static_cast<unsigned>(code.find(expression));
    auto variablePlacementOffset = static_cast<unsigned>(code.find("return"));

    auto replacements = Refactorings::ExtractVariable::run(
        "/" + filename, expression, "/" + filename, "int", "mySum", expressionOffset,
        static_cast<unsigned>(expression.size()), variablePlacementOffset);

    env.verifyResult(replacements, filename,
                     R"(int f()
                        {
                            int mySum = 2+2;
                            return mySum*2;
                        })");
}

void TestExtractVariable::testExtractField()
{
    RefactoringEnvironment env("-std=c++11");
    string filename = "class.h";
    string code = R"(class C
                     {
                       int field = 2+2*2;
                     })";
    env.addFile(filename, code);

    string expression = "2+2";
    auto expressionOffset = static_cast<unsigned>(code.find(expression));
    auto variablePlacementOffset = static_cast<unsigned>(code.find("int field"));

    auto replacements = Refactorings::ExtractVariable::run(
        "/" + filename, expression, "/" + filename, "int", "mySum", expressionOffset,
        static_cast<unsigned>(expression.size()), variablePlacementOffset);

    env.verifyResult(replacements, filename,
                     R"(class C
                        {
                            int mySum = 2+2;
                            int field = mySum*2;
                        })");
}
