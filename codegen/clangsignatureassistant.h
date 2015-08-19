/*
 * Copyright 2014 David Stevens <dgedstevens@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CLANG_SIGNATUREASSISTANT_H
#define CLANG_SIGNATUREASSISTANT_H

#include <QObject>

#include <KTextEditor/View>
#include <KTextEditor/Range>

#include <language/assistant/staticassistant.h>
#include <language/backgroundparser/parsejob.h>
#include <language/editor/documentcursor.h>

#include "../duchain/parsesession.h"
#include "../util/clangtypes.h"

#include <interfaces/iassistant.h>

namespace KDevelop {
    class IndexedString;
}

namespace KTextEditor {
    class Cursor;
}

struct ParamInfo
{
    QString name;
    QString type;
};

class ClangAdaptSignatureAction : public KDevelop::IAssistantAction
{
public:
    ClangAdaptSignatureAction(bool targetDecl, const QUrl &url, const KTextEditor::Range& range, const QString& newSig, const QString &oldSig);

    virtual QString description() const;

    virtual QString toolTip() const;

    virtual void execute();
private:
    bool m_targetDecl;
    QUrl m_url;
    KTextEditor::Range m_range;
    QString m_newSig;
    QString m_oldSig;
};

class ClangSignatureAssistant : public KDevelop::StaticAssistant
{
    Q_OBJECT
public:
    ClangSignatureAssistant(KDevelop::ILanguageSupport* languageSupport);
    virtual ~ClangSignatureAssistant();

    virtual QString title() const override;

    virtual bool isUseful() const override;

    virtual void textChanged(KTextEditor::View* view, const KTextEditor::Range& invocationRange,
                             const QString& removedText = QString()) override;

private slots:
    void parseJobFinished(KDevelop::ParseJob*);
    void reset();

private:
    QPointer<KTextEditor::View> m_view;

    //True if and only if the assistant is adjusting the declaration
    bool m_targetDecl;
    QUrl m_targetUnit;

    KDevelop::DocumentCursor m_otherLoc;

    QString m_oldName;
    QString m_oldSig;

    QList<ParamInfo> m_oldParamInfo;
};

#endif //CLANG_SIGNATUREASSISTANT_H
