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

// Clang
#include <clang/AST/Decl.h>

#include "encapsulatefieldrefactoring_changepack.h"
#include "utils.h"

using namespace std;
using namespace clang;


EncapsulateFieldRefactoring::ChangePack::ChangePack(const std::string &fieldDescription,
                                                    const std::string &fieldType,
                                                    const std::string &fieldName,
                                                    const std::string &getterName,
                                                    const std::string &setterName,
                                                    const std::string &recordName,
                                                    clang::AccessSpecifier getterAccess,
                                                    clang::AccessSpecifier setterAccess,
                                                    bool createSetter, bool isStatic)
    : m_fieldDescription(fieldDescription)
    , m_fieldType(fieldType)
    , m_fieldName(fieldName)
    , m_getterName(getterName)
    , m_setterName(setterName)
    , m_recordName(recordName)
    , m_getterAccess(getterAccess)
    , m_setterAccess(setterAccess)
    , m_createSetter(createSetter)
    , m_isStatic(isStatic)
{
    m_accessorCode = accessorImplementation();
    m_mutatorCode = mutatorImplementation();
}

unique_ptr<EncapsulateFieldRefactoring::ChangePack>
EncapsulateFieldRefactoring::ChangePack::fromDeclaratorDecl(const DeclaratorDecl *decl)
{
    auto currentAccess = decl->getAccess();
    const auto &srcMgr = decl->getASTContext().getSourceManager();
    const auto &langOpts = decl->getASTContext().getLangOpts();
    string fieldDescription = codeFromASTNode(decl, srcMgr, langOpts);
    auto fieldQualType = decl->getType().getNonReferenceType();
    fieldQualType.removeLocalConst();
    auto fieldTypeString = fieldQualType.getAsString();
    auto fieldName = decl->getName();
    string getterName = suggestGetterName(fieldName);
    string setterName = suggestSetterName(fieldName);
    string recordName = llvm::dyn_cast<RecordDecl>(decl->getDeclContext())->getName();
    return unique_ptr<ChangePack>(
        new ChangePack{fieldDescription, fieldTypeString, fieldName, getterName, setterName,
                       recordName, currentAccess, currentAccess, true,
                       !llvm::isa<FieldDecl>(decl)});
}

std::string EncapsulateFieldRefactoring::ChangePack::accessorImplementation() const
{
    string result;
    if (isStatic()) {
        result = "static ";
    }
    result += "const " + fieldType() + "&";
    result += " " + getterName() + "()";
    if (!isStatic()) {
        result += " const\n";
    } else {
        result += "\n";
    }
    result += "{\n";
    result += "\treturn " + fieldName() + ";\n";
    result += "}\n";
    return result;
};

std::string EncapsulateFieldRefactoring::ChangePack::mutatorImplementation() const
{
    string result;
    if (isStatic()) {
        result = "static ";
    }
    result += "void " + setterName() + "(const " + fieldType() +
              " &" + fieldName() + ")\n";
    result += "{\n";
    result += "\t";
    if (!isStatic()) {
        result += "this->";
    } else {
        result += m_recordName + "::";
    }
    result += fieldName() + " = " + fieldName() + ";\n";
    result += "}\n";
    return result;
};
