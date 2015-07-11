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

#ifndef KDEV_CLANG_REFACTORINGENVIRONMENT_H
#define KDEV_CLANG_REFACTORINGENVIRONMENT_H

#include <string>
#include <vector>
#include <clang/AST/DeclBase.h>
#include <clang/Tooling/Refactoring.h>

class DeclarationComparator;

class RefactoringEnvironment
{
public:
    RefactoringEnvironment();
    RefactoringEnvironment(const std::string &defaultFlags);

    // as TU
    void addFile(const std::string &name, const std::string &code);
    void addFile(const std::string &name, const std::string &code, const std::string &flags);
    void setDefaultFlags(const std::string &flags);

    // not TU
    void addHeader(const std::string &name, const std::string &code);

    // Note: registering file and header with the same name is undefined behavior

    clang::tooling::Replacements runTool(
        std::function<void(clang::tooling::RefactoringTool &)> runner);
    // Or return Replacements immediately

    void verifyResult(const clang::tooling::Replacements &replacements, const std::string &fileName,
                      const std::string &newCode);

private:
    std::string m_flags;
    // file name, code, compile flags
    std::vector<std::tuple<std::string, std::string, std::string>> m_files;
    std::vector<std::tuple<std::string, std::string>> m_headers;
};

std::unique_ptr<DeclarationComparator> declarationComparator(
    std::function<bool(const clang::Decl *decl)> equivalentTo);

std::string serialize(const clang::tooling::Replacements &replacements);

#endif //KDEV_CLANG_REFACTORINGENVIRONMENT_H
