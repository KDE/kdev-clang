/*
   Copyright 2007 David Nolden <david.nolden.kdevelop@art-master.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "codegenhelper.h"

#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/typealiastype.h>
#include <language/duchain/types/typesystem.h>
#include <language/duchain/types/referencetype.h>
#include <language/duchain/types/pointertype.h>
#include <language/duchain/ducontext.h>

using namespace KDevelop;

namespace {
IndexedTypeIdentifier stripPrefixIdentifiers(const IndexedTypeIdentifier& id, const QualifiedIdentifier& strip);

Identifier stripPrefixIdentifiers(const Identifier& id, const QualifiedIdentifier& strip)
{
    Identifier ret(id);
    ret.clearTemplateIdentifiers();
    for (unsigned int a = 0; a < id.templateIdentifiersCount(); ++a) {
        ret.appendTemplateIdentifier(stripPrefixIdentifiers(id.templateIdentifier(a), strip));
    }

    return ret;
}

IndexedTypeIdentifier stripPrefixIdentifiers(const IndexedTypeIdentifier& id, const QualifiedIdentifier& strip)
{
    QualifiedIdentifier oldId(id.identifier().identifier());
    QualifiedIdentifier qid;

    int commonPrefix = 0;
    for (; commonPrefix < oldId.count() - 1 && commonPrefix < strip.count(); ++commonPrefix) {
        if (strip.at(commonPrefix).toString() != oldId.at(commonPrefix).toString()) {
            break;
        }
    }

    for (int a = commonPrefix; a < oldId.count(); ++a) {
        qid.push(stripPrefixIdentifiers(oldId.at(a), strip));
    }

    IndexedTypeIdentifier ret(id);
    ret.setIdentifier(qid);
    return ret;
}

int reservedIdentifierCount(const QString &name)
{
    QStringList l = name.split(QLatin1String("::"));
    int ret = 0;
    foreach(const QString &s, l)
    if (s.startsWith(QLatin1Char('_'))) {
        ++ret;
    }

    return ret;
}

uint buildIdentifierForType(const AbstractType::Ptr& type, IndexedTypeIdentifier& id, uint pointerLevel, TopDUContext* top)
{
    if (!type) {
        return pointerLevel;
    }
    TypePtr<ReferenceType> refType = type.cast<ReferenceType>();
    if (refType) {
        id.setIsReference(true);
        if (refType->modifiers() & AbstractType::ConstModifier) {
            id.setIsConstant(true);
        }

        return buildIdentifierForType(refType->baseType(), id, pointerLevel, top);
    }
    TypePtr<PointerType> pointerType = type.cast<PointerType>();

    if (pointerType) {
        ++pointerLevel;
        uint maxPointerLevel = buildIdentifierForType(pointerType->baseType(), id, pointerLevel, top);
        if (type->modifiers() & AbstractType::ConstModifier) {
            id.setIsConstPointer(maxPointerLevel - pointerLevel, true);
        }
        if (static_cast<uint>(id.pointerDepth()) < pointerLevel) {
            id.setPointerDepth(pointerLevel);
        }

        return maxPointerLevel;
    }

    IdentifiedType* idType = dynamic_cast<IdentifiedType*>(type.data());
    if (idType) {
        Declaration* decl = idType->declaration(top);
        if (decl) {
            id.setIdentifier(decl->qualifiedIdentifier());
        } else {
            id.setIdentifier(idType->qualifiedIdentifier());
        }
    } else {
        //Just create it as an expression
        AbstractType::Ptr useTypeText = type;
        if (type->modifiers() & AbstractType::ConstModifier) {
            //Remove the 'const' modifier, as it will be added to the type-identifier below
            useTypeText = type->indexed().abstractType();
            useTypeText->setModifiers(useTypeText->modifiers() & (~AbstractType::ConstModifier));
        }
        id.setIdentifier(QualifiedIdentifier(useTypeText->toString(), true));
    }
    if (type->modifiers() & AbstractType::ConstModifier) {
        id.setIsConstant(true);
    }
    if (type->modifiers() & AbstractType::VolatileModifier) {
        id.setIsVolatile(true);
    }
    return pointerLevel;
}

IndexedTypeIdentifier identifierForType(const AbstractType::Ptr& type, TopDUContext* top)
{
    IndexedTypeIdentifier ret;
    buildIdentifierForType(type, ret, 0, top);
    return ret;
}

IndexedTypeIdentifier removeTemplateParameters(const IndexedTypeIdentifier& identifier, int behindPosition);

Identifier removeTemplateParameters(const Identifier& id, int behindPosition)
{
    Identifier ret(id);

    ret.clearTemplateIdentifiers();
    for (unsigned int a = 0; a < id.templateIdentifiersCount(); ++a) {
        IndexedTypeIdentifier replacement = removeTemplateParameters(id.templateIdentifier(a), behindPosition);
        if (( int ) a < behindPosition) {
            ret.appendTemplateIdentifier(replacement);
        } else {
            ret.appendTemplateIdentifier(IndexedTypeIdentifier(QualifiedIdentifier(QStringLiteral("..."))));
            break;
        }
    }

    return ret;
}

IndexedTypeIdentifier removeTemplateParameters(const IndexedTypeIdentifier& identifier, int behindPosition) {
    IndexedTypeIdentifier ret(identifier);

    QualifiedIdentifier oldId(identifier.identifier().identifier());
    QualifiedIdentifier qid;

    for (int a = 0; a < oldId.count(); ++a) {
        qid.push(removeTemplateParameters(oldId.at(a), behindPosition));
    }

    ret.setIdentifier(qid);

    return ret;
}

IndexedType removeConstModifier(const IndexedType& indexedType)
{
    AbstractType::Ptr type = indexedType.abstractType();
    type->setModifiers(type->modifiers() & (~AbstractType::ConstModifier));
    return type->indexed();
}

AbstractType::Ptr shortenTypeForViewing(const AbstractType::Ptr& type)
{
    struct ShortenAliasExchanger
        : public TypeExchanger
    {
        virtual AbstractType::Ptr exchange(const AbstractType::Ptr& type) override {
            if (!type) {
                return type;
            }

            AbstractType::Ptr newType(type->clone());

            TypeAliasType::Ptr alias = type.cast<TypeAliasType>();
            if (alias) {
                //If the aliased type has less involved template arguments, prefer it
                AbstractType::Ptr shortenedTarget = exchange(alias->type());
                if (shortenedTarget && shortenedTarget->toString().count(QLatin1Char('<')) < alias->toString().count(QLatin1Char('<'))
                    && reservedIdentifierCount(shortenedTarget->toString()) <= reservedIdentifierCount(alias->toString()))
                {
                    shortenedTarget->setModifiers(shortenedTarget->modifiers() | alias->modifiers());
                    return shortenedTarget;
                }
            }

            newType->exchangeTypes(this);

            return newType;
        }
    };

    ShortenAliasExchanger exchanger;
    return exchanger.exchange(type);
}

///Returns a type that has all template types replaced with DelayedType's that have their template default parameters stripped away,
///and all scope prefixes removed that are redundant within the given context
///The returned type should not actively be used in the  type-system, but rather only for displaying.
AbstractType::Ptr stripType(const AbstractType::Ptr& type, DUContext* ctx)
{
    if (!type) {
        return AbstractType::Ptr();
    }

    struct ShortenTemplateDefaultParameter
        : public TypeExchanger
    {
        DUContext* ctx;
        ShortenTemplateDefaultParameter(DUContext* _ctx)
            : ctx(_ctx) {
            Q_ASSERT(ctx);
        }

        virtual AbstractType::Ptr exchange(const AbstractType::Ptr& type) override {
            if (!type) {
                return type;
            }

            AbstractType::Ptr newType(type->clone());

            if (const IdentifiedType * idType = dynamic_cast<const IdentifiedType*>(type.data())) {
                Declaration* decl = idType->declaration(ctx->topContext());
                if (!decl) {
                    return type;
                }

                QualifiedIdentifier newTypeName;

#if 0 // from oldcpp
                if (TemplateDeclaration * tempDecl = dynamic_cast<TemplateDeclaration*>(decl)) {
                    if (decl->context()->type() == DUContext::Class && decl->context()->owner()) {
                        //Strip template default-parameters from the parent class
                        AbstractType::Ptr parentType = stripType(decl->context()->owner()->abstractType(), ctx);
                        if (parentType) {
                            newTypeName = QualifiedIdentifier(parentType->toString(), true);
                        }
                    }
                    if (newTypeName.isEmpty()) {
                        newTypeName = decl->context()->scopeIdentifier(true);
                    }

                    Identifier currentId;
                    if (!idType->qualifiedIdentifier().isEmpty()) {
                        currentId = idType->qualifiedIdentifier().last();
                    }
                    currentId.clearTemplateIdentifiers();

                    InstantiationInformation instantiationInfo = tempDecl->instantiatedWith().information();
                    InstantiationInformation newInformation(instantiationInfo);
                    newInformation.templateParametersList().clear();

                    for (uint neededParameters = 0; neededParameters < instantiationInfo.templateParametersSize(); ++neededParameters) {
                        newInformation.templateParametersList().append(instantiationInfo.templateParameters()[neededParameters]);
                        AbstractType::Ptr niceParam = stripType(instantiationInfo.templateParameters()[neededParameters].abstractType(), ctx);
                        if (niceParam) {
                            currentId.appendTemplateIdentifier(IndexedTypeIdentifier(niceParam->toString(), true));
//               debug() << "testing param" << niceParam->toString();
                        }

                        if (tempDecl->instantiate(newInformation, ctx->topContext()) == decl) {
//               debug() << "got full instantiation";
                            break;
                        }
                    }

                    newTypeName.push(currentId);
                } else {
                    newTypeName = decl->qualifiedIdentifier();
                }
#endif

                newTypeName = decl->qualifiedIdentifier();

                //Strip unneded prefixes of the scope
                QualifiedIdentifier candidate = newTypeName;
                while (candidate.count() > 1) {
                    candidate = candidate.mid(1);
                    QList<Declaration*> decls = ctx->findDeclarations(candidate);
                    if (decls.isEmpty()) {
                        continue; // type aliases might be available for nested sub scopes, hence we must not break early
                    }
                    if (decls[0]->kind() != Declaration::Type || removeConstModifier(decls[0]->indexedType()) != removeConstModifier(type->indexed())) {
                        break;
                    }
                    newTypeName = candidate;
                }
                if (newTypeName == decl->qualifiedIdentifier()) {
                    return type;
                }

                DelayedType::Ptr ret(new DelayedType);
                IndexedTypeIdentifier ti(newTypeName);
                ti.setIsConstant(type->modifiers() & AbstractType::ConstModifier);
                ret->setIdentifier(ti);
                return ret.cast<AbstractType>();
            }
            newType->exchangeTypes(this);

            return newType;
        }
    };

    ShortenTemplateDefaultParameter exchanger(ctx);
    return exchanger.exchange(type);
}
}

namespace CodegenHelper {
AbstractType::Ptr typeForShortenedString(Declaration* decl)
{
    AbstractType::Ptr type = decl->abstractType();
    if (decl->isTypeAlias()) {
        if (type.cast<TypeAliasType>()) {
            type = type.cast<TypeAliasType>()->type();
        }
    }

    if (decl->isFunctionDeclaration()) {
        FunctionType::Ptr funType = decl->type<FunctionType>();
        if (!funType) {
            return AbstractType::Ptr();
        }
        type = funType->returnType();
    }
    return type;
}

QString shortenedTypeString(Declaration* decl, DUContext* ctx, int desiredLength, const QualifiedIdentifier& stripPrefix)
{
    return shortenedTypeString(typeForShortenedString(decl), ctx, desiredLength, stripPrefix);
}

QString simplifiedTypeString(const AbstractType::Ptr& type, DUContext* visibilityFrom)
{
    return shortenedTypeString(type, visibilityFrom, 100000);
}

QString shortenedTypeString(const AbstractType::Ptr& type, DUContext* ctx, int desiredLength, const QualifiedIdentifier& stripPrefix)
{
    return shortenedTypeIdentifier(type, ctx, desiredLength, stripPrefix).toString();
}

IndexedTypeIdentifier shortenedTypeIdentifier(const AbstractType::Ptr& type_, DUContext* ctx, int desiredLength,
                                                             const QualifiedIdentifier& stripPrefix)
{
    bool isReference = false;
    bool isRValue = false;

    auto type = type_;
    if (const auto& refType = type.cast<ReferenceType>()) {
        isReference = true;
        type = refType->baseType();
        isRValue = refType->isRValue();
    }

    type = shortenTypeForViewing(type);

    if (ctx) {
        type = stripType(type, ctx);
    }
    if (!type) {
        return IndexedTypeIdentifier();
    }

    IndexedTypeIdentifier identifier = identifierForType(type, ctx ? ctx->topContext() : 0);

    if (type.cast<DelayedType>()) {
        identifier = type.cast<DelayedType>()->identifier();
    }
    identifier = stripPrefixIdentifiers(identifier, stripPrefix);

    if (isReference) {
        identifier.setIsReference(true);
    }
    if (isRValue) {
        identifier.setIsRValue(true);
    }

    int removeTemplateParametersFrom = 10;

    while (identifier.toString().length() > desiredLength * 3 && removeTemplateParametersFrom >= 0) {
        --removeTemplateParametersFrom;
        identifier = removeTemplateParameters(identifier, removeTemplateParametersFrom);
    }
    return identifier;
}

}
