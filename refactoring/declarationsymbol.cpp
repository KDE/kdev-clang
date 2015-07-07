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
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ASTContext.h>

#include "declarationsymbol.h"

using namespace clang;

class SymbolPart
{
public:
    virtual ~SymbolPart();

    // Tests if we can acknowledge that this part is syntactically equivalent to @p other
    virtual bool canBe(const SymbolPart &other) const = 0;

    // flatten anonymous in anonymous
    virtual bool noScope() const
    {
        return false;
    }
};

namespace
{
class ClassTemplateSpecializationSymbolPart : public SymbolPart
{
public:
    ClassTemplateSpecializationSymbolPart(const ClassTemplateSpecializationDecl *spec);

    virtual bool canBe(const SymbolPart &other) const override;

private:
    std::string m_name;
    std::vector<std::string> m_argumentList;
};

class NamespaceSymbolPart : public SymbolPart
{
public:
    NamespaceSymbolPart(const NamespaceDecl *namespaceDecl);

    virtual bool canBe(const SymbolPart &other) const override;

    virtual bool noScope() const override;

private:
    std::string m_name;
};

class RecordDeclSymbolPart : public SymbolPart
{
public:
    RecordDeclSymbolPart(const RecordDecl *namespaceDecl);

    virtual bool canBe(const SymbolPart &other) const override;

    virtual bool noScope() const override;

private:
    std::string m_name;
};

class FunctionSymbolPart : public SymbolPart
{
public:
    FunctionSymbolPart(const FunctionDecl *function);

    virtual bool canBe(const SymbolPart &other) const override;

private:
    std::string m_name;
    std::vector<std::string> m_parameters;
};

class NameSymbolPart : public SymbolPart
{
public:
    NameSymbolPart(const DeclContext *declContext);

    NameSymbolPart(const NamedDecl *namedDecl);

    virtual bool canBe(const SymbolPart &other) const override;

    virtual bool noScope() const override;

private:
    std::string m_name;
};

}

DeclarationSymbol::DeclarationSymbol(const clang::NamedDecl *decl)
{
    const DeclContext *ctx = decl->getCanonicalDecl()->getDeclContext();

    // Collect all enclosing decl contexts
    std::vector<const DeclContext *> contexts;
    for (; ctx; ctx = ctx->getParent()) {
        contexts.push_back(ctx);
    }

    for (auto i = contexts.rbegin(); i != contexts.rend(); ++i) {
        // We have a few kind of declaration contexts
        if (const ClassTemplateSpecializationDecl *spec =
            llvm::dyn_cast<ClassTemplateSpecializationDecl>(*i)) {
            m_parts.emplace_back(new ClassTemplateSpecializationSymbolPart(spec));
        } else if (const NamespaceDecl *namespaceDecl = llvm::dyn_cast<NamespaceDecl>(*i)) {
            m_parts.emplace_back(new NamespaceSymbolPart(namespaceDecl));
        } else if (const RecordDecl *recordDecl = llvm::dyn_cast<RecordDecl>(*i)) {
            m_parts.emplace_back(new RecordDeclSymbolPart(recordDecl));
        } else if (const FunctionDecl *function = llvm::dyn_cast<FunctionDecl>(*i)) {
            m_parts.emplace_back(new FunctionSymbolPart(function));
        } else {
            m_parts.emplace_back(new NameSymbolPart(*i));
        }
    }

    if (const FunctionDecl *function = llvm::dyn_cast<FunctionDecl>(decl)) {
        m_parts.emplace_back(new FunctionSymbolPart(function));
    } else {
        m_parts.emplace_back(new NameSymbolPart(decl));
    }
}

DeclarationSymbol::~DeclarationSymbol() = default;

bool DeclarationSymbol::equivalentTo(const clang::Decl *decl) const
{
    const NamedDecl *namedDecl = llvm::dyn_cast<NamedDecl>(decl);
    if (namedDecl == nullptr) {
        return false;
    } else if (namedDecl->getLinkageInternal() != ExternalLinkage) {
        return false;
    } else {
        DeclarationSymbol sym(namedDecl);
        if (sym.m_parts.size() != m_parts.size()) {
            return false;
        } else {
            for (unsigned i = 0; i < m_parts.size(); ++i) {
                // Maybe should use noScope
                if (!m_parts[i]->canBe(*sym.m_parts[i])) {
                    return false;
                }
            }
            return true;
        }
    }
}

SymbolPart::~SymbolPart() = default;

ClassTemplateSpecializationSymbolPart::ClassTemplateSpecializationSymbolPart(
    const ClassTemplateSpecializationDecl *spec)
    : m_name(spec->getName())
{
    const TemplateArgumentList &args = spec->getTemplateArgs();
    for (const auto &arg : args.asArray()) {
        std::string result;
        llvm::raw_string_ostream stream(result);
        arg.print(spec->getASTContext().getPrintingPolicy(), stream);
        m_argumentList.push_back(stream.str());
    }
}

bool ClassTemplateSpecializationSymbolPart::canBe(const SymbolPart &other) const
{
    const ClassTemplateSpecializationSymbolPart *o =
        dynamic_cast<const ClassTemplateSpecializationSymbolPart *>(&other);
    if (o == nullptr) {
        return false;
    }
    return (m_name == o->m_name) && (m_argumentList == o->m_argumentList);
}

NamespaceSymbolPart::NamespaceSymbolPart(const NamespaceDecl *namespaceDecl)
{
    if (namespaceDecl->isAnonymousNamespace()) {
        m_name = "";
    } else {
        m_name = namespaceDecl->getName();
    }
}

bool NamespaceSymbolPart::canBe(const SymbolPart &other) const
{
    const NamespaceSymbolPart *o = dynamic_cast<const NamespaceSymbolPart *>(&other);
    if (o == nullptr) {
        return false;
    }
    return m_name == o->m_name;
}

bool NamespaceSymbolPart::noScope() const
{
    return m_name.empty();
}

RecordDeclSymbolPart::RecordDeclSymbolPart(const RecordDecl *record)
{
    m_name = record->getName();
}

bool RecordDeclSymbolPart::canBe(const SymbolPart &other) const
{
    const RecordDeclSymbolPart *o = dynamic_cast<const RecordDeclSymbolPart *>(&other);
    if (o == nullptr) {
        return false;
    }
    return m_name == o->m_name;
}

bool RecordDeclSymbolPart::noScope() const
{
    return m_name.empty();
}

FunctionSymbolPart::FunctionSymbolPart(const FunctionDecl *function)
{
    m_name = function->getName();
    unsigned size = function->getNumParams();
    for (unsigned i = 0; i < size; ++i) {
        m_parameters.push_back(
            function->getParamDecl(i)->getType().getCanonicalType().getAsString());
    }
}

bool FunctionSymbolPart::canBe(const SymbolPart &other) const
{
    const FunctionSymbolPart *o = dynamic_cast<const FunctionSymbolPart *>(&other);
    if (o == nullptr) {
        return false;
    }
    return (m_name == o->m_name) && (m_parameters == o->m_parameters);
}

NameSymbolPart::NameSymbolPart(const DeclContext *declContext)
{
    const NamedDecl *namedDecl = llvm::dyn_cast<NamedDecl>(declContext);
    if (namedDecl == nullptr) {
        return;
    }
    m_name = namedDecl->getName();
}

NameSymbolPart::NameSymbolPart(const NamedDecl *namedDecl)
{
    m_name = namedDecl->getName();
}

bool NameSymbolPart::canBe(const SymbolPart &other) const
{
    const NameSymbolPart *o = dynamic_cast<const NameSymbolPart *>(&other);
    if (o == nullptr) {
        return false;
    }
    return m_name == o->m_name;
}

bool NameSymbolPart::noScope() const
{
    return m_name.empty();
}

