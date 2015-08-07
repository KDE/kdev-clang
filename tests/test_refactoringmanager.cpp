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
#include <refactoring/renamevardeclrefactoring.h>
#include <refactoring/encapsulatefieldrefactoring.h>
#include <refactoring/extractvariablerefactoring.h>
#include "test_refactoringmanager.h"
#include "refactoringenvironment.h"
#include "../refactoring/refactoringmanager.h"
#include "debug.h"
#include "../refactoring/changesignaturerefactoring.h"
#include "../refactoring/renamefielddeclrefactoring.h"
#include "../refactoring/extractfunctionrefactoring.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestRefactoringManager)


void TestRefactoringManager::testSinglePosition()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(#include <iostream>
                     int main(int argc,char **argv)
                     {
                       std::cout<<argc<<std::endl;
                       return 0;
                     })";
    env.addFile("main.cpp", code);
    unsigned offset = static_cast<unsigned>(code.find("argc<"));
    env.runTool(
        [offset](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/main.cpp", offset, QThread::currentThread(),
                                                tool);
            QVERIFY(refactorings.length() == 1);
            QVERIFY(typeid(*refactorings[0]) == typeid(RenameVarDeclRefactoring));
        });
}

void TestRefactoringManager::testFunctionDeclarationArgumentPosition()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(#include <iostream>
                     int main(int argc,char **argv)
                     {
                       std::cout<<argc<<std::endl;
                       return 0;
                     })";
    env.addFile("main.cpp", code);
    unsigned offset = static_cast<unsigned>(code.find("argc,"));
    env.runTool(
        [offset](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/main.cpp", offset, QThread::currentThread(),
                                                tool);
            QVERIFY(refactorings.length() == 2);
            QVERIFY(typeid(*refactorings[0]) == typeid(ChangeSignatureRefactoring));
            QVERIFY(typeid(*refactorings[1]) == typeid(RenameVarDeclRefactoring));
        });
}

void TestRefactoringManager::testRecord()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(class C
                     {
                       int a;
                     };)";
    env.addFile("main.cpp", code);
    unsigned offset = static_cast<unsigned>(code.find("a;"));
    env.runTool(
        [offset](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/main.cpp", offset, QThread::currentThread(),
                                                tool);
            QVERIFY(refactorings.length() == 2);
            QVERIFY(typeid(*refactorings[0]) == typeid(RenameFieldDeclRefactoring));
            QVERIFY(typeid(*refactorings[1]) == typeid(EncapsulateFieldRefactoring));
        });
}

void TestRefactoringManager::testExprRanges()
{
    RefactoringEnvironment env("-std=c++11");
    string code = R"(int f(int a)
                     {
                       return 1+2*a;
                     })";
    env.addFile("main.cpp", code);
    unsigned offsetBegin = static_cast<unsigned>(code.find("2*"));
    unsigned offsetEnd = static_cast<unsigned>(code.find("a;"));
    env.runTool(
        [offsetBegin, offsetEnd](RefactoringTool &tool)
        {
            auto refactorings = refactoringsFor("/main.cpp", offsetBegin, offsetEnd,
                                                QThread::currentThread(), tool);
            QVERIFY(refactorings.length() == 2);
            QVERIFY(typeid(*refactorings[0]) == typeid(ExtractVariableRefactoring));
            QVERIFY(typeid(*refactorings[1]) == typeid(ExtractFunctionRefactoring));
        });
}
