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
#include "test_encapsulatefield.h"
#include "refactoringenvironment.h"
#include "../refactoring/encapsulatefieldrefactoring.h"
#include "../refactoring/encapsulatefieldrefactoring_changepack.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;

QTEST_GUILESS_MAIN(TestEncapsulateField)

void TestEncapsulateField::testSimpleConstRef()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("klass.h",
                  R"(class C
                     {
                     public:
                       int x;
                     };)");
    env.addFile("use.cpp",
                R"(#include "klass.h"
                   int f()
                   {
                     C c;
                     c.x=5;
                     return c.x*2;
                   })");
    auto replacements = env.runTool(
        [](RefactoringTool &tool)
        {
            using ChangePack = EncapsulateFieldRefactoring::ChangePack;
            ChangePack changePack("", "int", "x", "accessor", "mutator", "C", AS_public, AS_public,
                                  true, false);
            auto declDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<FieldDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<FieldDecl>(decl)->getName() == "x";
                });
            auto recordDeclDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<CXXRecordDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<CXXRecordDecl>(decl)->getName() == "C";
                }
            );
            Refactorings::EncapsulateField::run(tool, &changePack, declDispatcher.get(),
                                                recordDeclDispatcher.get(), "C");
        });
    env.verifyResult(replacements, "klass.h",
                     R"(class C
                        {
                        public:
                        private:
                            int x;
                        public:
                            const int& accessor() const
                            {
                                return x;
                            }

                            void mutator(const int &x)
                            {
                                this->x = x;
                            }
                        };)");
    env.verifyResult(replacements, "use.cpp",
                     R"(#include "klass.h"
                        int f()
                        {
                            C c;
                            c.mutator(5);
                            return c.accessor()*2;
                        })");
}

void TestEncapsulateField::testStaticByValue()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("klass.h",
                  R"(class C
                     {
                     public:
                       static int x;
                     };)");
    env.addFile("use.cpp",
                R"(#include "klass.h"
                   int f()
                   {
                     C c;
                     C::x=5;
                     return C::x*2;
                   })");
    auto replacements = env.runTool(
        [](RefactoringTool &tool)
        {
            using ChangePack = EncapsulateFieldRefactoring::ChangePack;
            ChangePack changePack("", "int", "x", "accessor", "mutator", "C", AS_public, AS_public,
                                  true, true);
            auto declDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<VarDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<VarDecl>(decl)->getName() == "x";
                });
            auto recordDeclDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<CXXRecordDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<CXXRecordDecl>(decl)->getName() == "C";
                }
            );
            Refactorings::EncapsulateField::run(tool, &changePack, declDispatcher.get(),
                                                recordDeclDispatcher.get(), "C");
        });
    env.verifyResult(replacements, "klass.h",
                     R"(class C
                        {
                        public:
                        private:
                            static int x;
                        public:
                            static const int& accessor()
                            {
                                return x;
                            }

                            static void mutator(const int &x)
                            {
                                C::x = x;
                            }
                        };)");
    env.verifyResult(replacements, "use.cpp",
                     R"(#include "klass.h"
                        int f()
                        {
                            C c;
                            C::mutator(5);
                            return C::accessor()*2;
                        })");
}

void TestEncapsulateField::testStaticNoMutator()
{
    RefactoringEnvironment env("-std=c++11");
    env.addHeader("klass.h",
                  R"(class C
                     {
                     public:
                       static int x;
                     };)");
    env.addFile("use.cpp",
                R"(#include "klass.h"
                   int f()
                   {
                     C c;
                     C::x=5;
                     return C::x*2;
                   })");
    auto replacements = env.runTool(
        [](RefactoringTool &tool)
        {
            using ChangePack = EncapsulateFieldRefactoring::ChangePack;
            ChangePack changePack("", "int", "x", "accessor", "mutator", "C", AS_public, AS_none,
                                  false, true);
            auto declDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<VarDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<VarDecl>(decl)->getName() == "x";
                });
            auto recordDeclDispatcher = declarationComparator(
                [](const Decl *decl)
                {
                    if (!llvm::isa<CXXRecordDecl>(decl)) {
                        return false;
                    }
                    return llvm::dyn_cast<CXXRecordDecl>(decl)->getName() == "C";
                }
            );
            Refactorings::EncapsulateField::run(tool, &changePack, declDispatcher.get(),
                                                recordDeclDispatcher.get(), "C");
        });
    env.verifyResult(replacements, "klass.h",
                     R"(class C
                        {
                        public:
                        private:
                            static int x;
                        public:
                            static const int& accessor()
                            {
                                return x;
                            }
                        };)");
    env.verifyResult(replacements, "use.cpp",
                     R"(#include "klass.h"
                        int f()
                        {
                            C c;
                            C::accessor()=5;
                            return C::accessor()*2;
                        })");
}

