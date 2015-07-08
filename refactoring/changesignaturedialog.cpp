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
#include <QAbstractTableModel>

// KF5
#include <KLocalizedString>

#include "changesignaturedialog.h"
#include "changesignaturerefactoringinfopack.h"
#include "changesignaturerefactoringchangepack.h"

using InfoPack = ChangeSignatureRefactoring::InfoPack;
using ChangePack = ChangeSignatureRefactoring::ChangePack;

class ChangeSignatureDialog::Model : public QAbstractTableModel
{
    Q_OBJECT;
    Q_DISABLE_COPY(Model);

    friend class ChangeSignatureDialog;

    // Need implicit cast from int to abuse type system...
    // (https://git.reviewboard.kde.org/r/124300/#comment57268)
    struct Column
    {
        enum : int
        {
            Type = 0,
            Name = 1,
        };
    };

public:
    Model(const InfoPack *infoPack, QObject *parent);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent) const override;

    virtual QVariant data(const QModelIndex &index, int role) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    const InfoPack *infoPack() const
    {
        return m_infoPack;
    }

    const ChangePack *changePack() const
    {
        return m_changePack;
    }

    void resetChanges();

    void removeRow(int index);

    // Insert before @p index
    void insertRow(int index);

    void moveRowUp(int index);

    void moveRowDown(int index);

private:
    const InfoPack *const m_infoPack;
    ChangePack *m_changePack;
};

ChangeSignatureDialog::ChangeSignatureDialog(const InfoPack *infoPack, QWidget *parent)
    : QDialog(parent)
    , m_model(new Model(infoPack, this))
{
    setupUi(this);
    reinitializeDialogData();

    connect(dialogButtonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
            &ChangeSignatureDialog::reinitializeDialogData);

    // FIXME: add validation
    // TODO: improve tracking of changes: eliminate dummy changes, track params with changed type
    connect(dialogButtonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]()
    {
        std::string text = returnTypeLineEdit->text().toStdString();
        if (text != m_model->m_infoPack->returnType()) {
            m_model->m_changePack->m_newResult = text;
        }
        text = functionNameLineEdit->text().toStdString();
        if (text != m_model->m_infoPack->functionName()) {
            m_model->m_changePack->m_newName = text;
        }
    });

    parametersTableView->setModel(m_model);
    parametersTableView->verticalHeader()->hide();
    parametersTableView->horizontalHeader()->setStretchLastSection(true);
    parametersTableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(removeToolButton, &QToolButton::clicked, this, [this]()
    {
        const auto &selection = parametersTableView->selectionModel()->selectedIndexes();
        if (selection.size() > 0) {
            m_model->removeRow(selection[0].row());
        }
    });

    connect(addToolButton, &QToolButton::clicked, this, [this]()
    {
        const auto &selection = parametersTableView->selectionModel()->selectedIndexes();
        if (selection.size() > 0) {
            m_model->insertRow(selection[0].row());
        } else {
            m_model->insertRow(m_model->rowCount());
        }
    });

    connect(upToolButton, &QToolButton::clicked, this, [this]()
    {
        const auto &selection = parametersTableView->selectionModel()->selectedIndexes();
        if (selection.size() > 0) {
            m_model->moveRowUp(selection[0].row());
        }
    });

    connect(downToolButton, &QToolButton::clicked, this, [this]()
    {
        const auto &selection = parametersTableView->selectionModel()->selectedIndexes();
        if (selection.size() > 0) {
            m_model->moveRowDown(selection[0].row());
        }
    });
}

const InfoPack *ChangeSignatureDialog::infoPack() const
{
    return m_model->m_infoPack;
}

const ChangePack *ChangeSignatureDialog::changePack() const
{
    return m_model->m_changePack;
}

void ChangeSignatureDialog::reinitializeDialogData()
{
    returnTypeLineEdit->setText(QString::fromStdString(m_model->infoPack()->returnType()));
    functionNameLineEdit->setText(QString::fromStdString(m_model->infoPack()->functionName()));
    if (m_model->infoPack()->isRestricted()) {
        functionNameLineEdit->setDisabled(true);
        returnTypeLineEdit->setDisabled(true);
    }
    m_model->resetChanges();
}


//////////////////// CHANGE MODEL

ChangeSignatureDialog::Model::Model(const InfoPack *infoPack, QObject *parent)
    : QAbstractTableModel(parent)
    , m_infoPack(infoPack)
    , m_changePack(new ChangePack(infoPack))
{
}

int ChangeSignatureDialog::Model::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_changePack->m_paramRefs.size());
}

int ChangeSignatureDialog::Model::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant ChangeSignatureDialog::Model::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        Q_ASSERT(static_cast<int>(m_changePack->m_paramRefs.size()) > index.row());
        int row = m_changePack->m_paramRefs[index.row()];
        const auto &params = row >= 0 ? m_infoPack->parameters() : m_changePack->m_newParam;
        if (row < 0) {
            row = -row - 1;
        }
        auto &t = params[row];
        switch (index.column()) {
        case Column::Type:
            return QString::fromStdString(std::get<0>(t));
        case Column::Name:
            return QString::fromStdString(std::get<1>(t));
        }
    }
    return QVariant();
}

QVariant ChangeSignatureDialog::Model::headerData(int section, Qt::Orientation orientation,
                                                  int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case Column::Type:
            return i18n("Type");
        case Column::Name:
            return i18n("Name");
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags ChangeSignatureDialog::Model::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.column() == Column::Type || m_changePack->m_paramRefs[index.row()] < 0) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool ChangeSignatureDialog::Model::setData(const QModelIndex &index, const QVariant &value,
                                           int role)
{
    if (value == data(index, role)) {
        return false;
    }
    if (role == Qt::EditRole) {
        int row = m_changePack->m_paramRefs[index.row()];
        if (row >= 0) {
            m_changePack->m_newParam.push_back(m_infoPack->parameters()[row]);
            row = -static_cast<int>(m_changePack->m_newParam.size());
            m_changePack->m_paramRefs[index.row()] = row;
        }
        Q_ASSERT(row < 0);
        auto &t = m_changePack->m_newParam[-row - 1];
        std::string newValue = value.toString().toStdString();
        switch (index.column()) {
        case Column::Type:
            std::get<0>(t) = newValue;
            break;
        case Column::Name:
            std::get<1>(t) = newValue;
            break;
        default:
            return false;
        }
        dataChanged(index, index, {role});
        return true;
    }
    return false;
}

void ChangeSignatureDialog::Model::resetChanges()
{
    beginResetModel();
    m_changePack = new ChangePack(m_infoPack);
    endResetModel();
}

void ChangeSignatureDialog::Model::removeRow(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
    // NOTE: this leaks new parameters
    m_changePack->m_paramRefs.erase(m_changePack->m_paramRefs.begin() + index);
    endRemoveRows();
}

void ChangeSignatureDialog::Model::insertRow(int index)
{
    Q_ASSERT(index >= 0);
    // One-after-end is allowed
    Q_ASSERT(static_cast<std::size_t>(index) <= m_changePack->m_paramRefs.size());
    beginInsertRows(QModelIndex(), index, index);
    m_changePack->m_newParam.emplace_back("", "");
    m_changePack->m_paramRefs.insert(m_changePack->m_paramRefs.begin() + index,
                                     -static_cast<int>(m_changePack->m_newParam.size()));
    endInsertRows();
}

void ChangeSignatureDialog::Model::moveRowUp(int index)
{
    if (index == 0) {
        return;
    }
    if (!beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1)) {
        return;
    }
    using std::swap;
    swap(m_changePack->m_paramRefs[index], m_changePack->m_paramRefs[index - 1]);
    endMoveRows();
}

void ChangeSignatureDialog::Model::moveRowDown(int index)
{
    if (index + 1 == rowCount()) {
        return;
    }
    if (!beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2)) {
        return;
    }
    using std::swap;
    swap(m_changePack->m_paramRefs[index], m_changePack->m_paramRefs[index + 1]);
    endMoveRows();
}

#include "changesignaturedialog.moc"
