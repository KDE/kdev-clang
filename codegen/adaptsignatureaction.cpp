/*
   Copyright 2009 David Nolden <david.nolden.kdevelop@art-master.de>
   Copyright 2014 Kevin Funk <kfunk@kde.org>

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

#include "adaptsignatureaction.h"
#include "codegenhelper.h"

#include "../duchain/duchainutils.h"
#include "../util/clangdebug.h"

#include <language/assistant/renameaction.h>
#include <language/codegen/documentchangeset.h>
#include <language/duchain/types/arraytype.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/functiondefinition.h>

#include <KLocalizedString>
#include <KMessageBox>

using namespace KDevelop;

QString makeSignatureString(const Declaration* functionDecl, const Signature& signature)
{
    if (!functionDecl || !functionDecl->internalContext()) {
        return {};
    }
    const auto visibilityFrom = functionDecl->internalContext()->parentContext();
    if (!visibilityFrom) {
        return {};
    }

    QString ret = CodegenHelper::simplifiedTypeString(signature.returnType.abstractType(),
                                                      visibilityFrom);

    ret += QLatin1Char(' ');

    QualifiedIdentifier namespaceIdentifier = visibilityFrom->scopeIdentifier(false);
    Identifier id(IndexedString(functionDecl->qualifiedIdentifier().mid(namespaceIdentifier.count()).toString()));
    ret += functionDecl->identifier().toString();

    ret += QLatin1Char('(');
    int pos = 0;

    foreach(const ParameterItem &item, signature.parameters)
    {
        if (pos != 0) {
            ret += QLatin1String(", ");
        }

        ///TODO: merge common code with helpers.cpp::createArgumentList
        AbstractType::Ptr type = item.first.abstractType();

        QString arrayAppendix;
        ArrayType::Ptr arrayType;
        while ((arrayType = type.cast<ArrayType>())) {
            type = arrayType->elementType();
            //note: we have to prepend since we iterate from outside, i.e. from right to left.
            if (arrayType->dimension()) {
                arrayAppendix.prepend(QStringLiteral("[%1]").arg(arrayType->dimension()));
            } else {
                // dimensionless
                arrayAppendix.prepend(QLatin1String("[]"));
            }
        }
        ret += CodegenHelper::simplifiedTypeString(type,
                                                   visibilityFrom);

        if (!item.second.isEmpty()) {
            ret += QLatin1Char(' ') + item.second;
        }
        ret += arrayAppendix;
        if (signature.defaultParams.size() > pos && !signature.defaultParams[pos].isEmpty()) {
            ret += QLatin1String(" = ") + signature.defaultParams[pos];
        }
        ++pos;
    }
    ret += QLatin1Char(')');
    if (signature.isConst) {
        ret += QLatin1String(" const");
    }
    return ret;
}

AdaptSignatureAction::AdaptSignatureAction(const DeclarationId& definitionId,
                                           ReferencedTopDUContext definitionContext,
                                           const Signature& oldSignature,
                                           const Signature& newSignature,
                                           bool editingDefinition, QList<RenameAction*> renameActions
                                           )
    : m_otherSideId(definitionId)
    , m_otherSideTopContext(definitionContext)
    , m_oldSignature(oldSignature)
    , m_newSignature(newSignature)
    , m_editingDefinition(editingDefinition)
    , m_renameActions(renameActions) {
}

AdaptSignatureAction::~AdaptSignatureAction()
{
    qDeleteAll(m_renameActions);
}

QString AdaptSignatureAction::description() const
{
    return m_editingDefinition ? i18n("Update declaration signature") : i18n("Update definition signature");
}

QString AdaptSignatureAction::toolTip() const
{
    DUChainReadLocker lock;
    auto declaration = m_otherSideId.getDeclaration(m_otherSideTopContext.data());
    if (!declaration) {
        return {};
    }
    return i18n("Update %1 signature\nfrom: %2\nto: %3",
                m_editingDefinition ? i18n("declaration") : i18n("definition"),
                makeSignatureString(declaration, m_oldSignature),
                makeSignatureString(declaration, m_newSignature));
}

void AdaptSignatureAction::execute()
{
    DUChainReadLocker lock;
    IndexedString url = m_otherSideTopContext->url();
    lock.unlock();
    m_otherSideTopContext = DUChain::self()->waitForUpdate(url, TopDUContext::AllDeclarationsContextsAndUses);
    if (!m_otherSideTopContext) {
        clangDebug() << "failed to update" << url.str();
        return;
    }

    lock.lock();

    Declaration* otherSide = m_otherSideId.getDeclaration(m_otherSideTopContext.data());
    if (!otherSide) {
        clangDebug() << "could not find definition";
        return;
    }
    DUContext* functionContext = DUChainUtils::getFunctionContext(otherSide);
    if (!functionContext) {
        clangDebug() << "no function context";
        return;
    }
    if (!functionContext || functionContext->type() != DUContext::Function) {
        clangDebug() << "no correct function context";
        return;
    }

    DocumentChangeSet changes;
    KTextEditor::Range parameterRange = ClangIntegration::DUChainUtils::functionSignatureRange(otherSide);
    QString newText = makeSignatureString(otherSide, m_newSignature);
    if (!m_editingDefinition) {
        // append a newline after the method signature in case the method definition follows
        newText += QLatin1Char('\n');
    }

    DocumentChange changeParameters(functionContext->url(), parameterRange, QString(), newText);
    changeParameters.m_ignoreOldText = true;
    changes.addChange(changeParameters);
    changes.setReplacementPolicy(DocumentChangeSet::WarnOnFailedChange);
    DocumentChangeSet::ChangeResult result = changes.applyAllChanges();
    if (!result) {
        KMessageBox::error(0, i18n("Failed to apply changes: %1", result.m_failureReason));
    }
    emit executed(this);

    foreach(RenameAction * renAct, m_renameActions) {
        renAct->execute();
    }
}

#include "moc_adaptsignatureaction.cpp"
