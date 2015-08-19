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

#ifndef SIGNATUREASSISTANT_H
#define SIGNATUREASSISTANT_H

#include "adaptsignatureaction.h"

#include <language/assistant/staticassistant.h>
#include <language/duchain/identifier.h>
#include <language/duchain/topducontext.h>

#include <QPointer>
#include <QUrl>

namespace KTextEditor {
class View;
}

namespace KDevelop {
class ParseJob;
}

class AdaptSignatureAssistant : public KDevelop::StaticAssistant
{
    Q_OBJECT

public:
    AdaptSignatureAssistant(KDevelop::ILanguageSupport* supportedLanguage);

    virtual QString title() const override;
    virtual void textChanged(KTextEditor::View* view, const KTextEditor::Range& invocationRange, const QString& removedText = QString()) override;
    virtual bool isUseful() const override;

private:
    ///Compare @param newSignature to m_oldSignature and put differences in oldPositions
    ///@returns whether or not there are any differences
    bool getSignatureChanges(const Signature &newSignature, QList<int> &oldPositions) const;
    ///Set default params in @param newSignature based on m_oldSignature's defaults and @param oldPositions
    void setDefaultParams(Signature &newSignature, const QList<int> &oldPositions) const;
    ///@returns RenameActions for each parameter in newSignature that has been renamed
    QList<KDevelop::RenameAction*> getRenameActions(const Signature &newSignature, const QList<int> &oldPositions) const;

    // If this is true, the user is editing on the definition side,
    // and the declaration should be updated
    bool m_editingDefinition = false;
    KDevelop::Identifier m_declarationName;
    KDevelop::DeclarationId m_otherSideId;
    KDevelop::ReferencedTopDUContext m_otherSideTopContext;
    KDevelop::DUContextPointer m_otherSideContext;
    //old signature of the _other_side
    Signature m_oldSignature;
    QUrl m_document;
    QPointer<KTextEditor::View> m_view;

private slots:
    void parseJobFinished(KDevelop::ParseJob*);
    void reset();
};

#endif // SIGNATUREASSISTANT_H
