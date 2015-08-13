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
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>

// KF5
#include <KLocalizedString>

// Clang
#include <clang/Tooling/Core/Replacement.h>

#include "refactoring.h"

using namespace clang;
using namespace clang::tooling;

Refactoring::Refactoring(QObject *parent)
    : QObject(parent)
{
}

llvm::ErrorOr<Replacements> Refactoring::cancelledResult()
{
    return Replacements{};
}

QDialog *Refactoring::newBusyDialog()
{
    // Very simple placeholder
    // TODO: progress bar and cancel button (this is not trivial task!)
    QDialog *result = new QDialog();
    QVBoxLayout *layout = new QVBoxLayout();
    QLabel *label = new QLabel(i18n("Refactoring..."));
    layout->addWidget(label);
    result->setLayout(layout);
    return result;
}

std::function<void(Replacements)> Refactoring::uiLockerCallback(QDialog *uiLocker,
                                                                Replacements &result)
{
    return [uiLocker, &result](Replacements repl)
    {
        // User may close dialog before end of operation
        if(uiLocker->isVisible()) {
            result = std::move(repl);
            uiLocker->accept();
        }
        uiLocker->deleteLater();
    };
}
