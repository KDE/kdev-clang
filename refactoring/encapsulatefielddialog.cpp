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

// Qt
#include <QPushButton>

#include "encapsulatefielddialog.h"
#include "encapsulatefieldrefactoring_changepack.h"

using namespace clang;

Q_DECLARE_METATYPE(clang::AccessSpecifier);

EncapsulateFieldDialog::EncapsulateFieldDialog(ChangePack *changePack)
    : QDialog(nullptr)
    , m_changePack(changePack)
{
    setupUi(this);
    fieldLineEdit->setText(QString::fromStdString(m_changePack->fieldDescription()));
    setterGroupBox->setEnabled(m_changePack->createSetter());

    getterDefinitionPlainTextEdit->setPlainText(
        QString::fromStdString(m_changePack->accessorCode()));
    setterDefinitionPlainTextEdit->setPlainText(
        QString::fromStdString(m_changePack->mutatorCode()));

    Q_ASSERT(m_changePack->getterAccess() != AS_none);
    Q_ASSERT(m_changePack->setterAccess() != AS_none);

    getterVisibilityComboBox->addItem(QStringLiteral("public"), AccessSpecifier::AS_public);
    if (m_changePack->getterAccess() != AccessSpecifier::AS_public) {
        getterVisibilityComboBox->addItem(QStringLiteral("protected"),
                                          AccessSpecifier::AS_protected);
    }
    if (m_changePack->getterAccess() == AccessSpecifier::AS_private) {
        getterVisibilityComboBox->addItem(QStringLiteral("private"), AccessSpecifier::AS_private);
    }
    getterVisibilityComboBox->setCurrentIndex(getterVisibilityComboBox->count() - 1);
    setterVisibilityComboBox->addItem(QStringLiteral("public"), AccessSpecifier::AS_public);
    if (m_changePack->setterAccess() != AccessSpecifier::AS_public) {
        setterVisibilityComboBox->addItem(QStringLiteral("protected"),
                                          AccessSpecifier::AS_protected);
    }
    if (m_changePack->setterAccess() == AccessSpecifier::AS_private) {
        setterVisibilityComboBox->addItem(QStringLiteral("private"), AccessSpecifier::AS_private);
    }
    setterVisibilityComboBox->setCurrentIndex(setterVisibilityComboBox->count() - 1);

    // set above default

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]()
    {
        m_changePack->setAccessorCode(getterDefinitionPlainTextEdit->toPlainText().toStdString());
        m_changePack->setGetterName(
            functionName(m_changePack->accessorCode(), m_changePack->getterName()));
        m_changePack->setGetterAccess(
            getterVisibilityComboBox->currentData().value<AccessSpecifier>());
        m_changePack->setCreateSetter(setterGroupBox->isChecked());
        if (m_changePack->createSetter()) {
            m_changePack->setMutatorCode(
                setterDefinitionPlainTextEdit->toPlainText().toStdString());
            m_changePack->setSetterName(
                functionName(m_changePack->mutatorCode(), m_changePack->setterName()));
            m_changePack->setSetterAccess(
                setterVisibilityComboBox->currentData().value<AccessSpecifier>());
        }
    });
}

