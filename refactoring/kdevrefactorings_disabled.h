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

#ifndef KDEV_CLANG_REFACTORINGSGLUE_DISABLED_H
#define KDEV_CLANG_REFACTORINGSGLUE_DISABLED_H

#ifndef KDEV_CLANG_REFACTORINGSGLUE_H
#error Do not include this file directly, use kdevrefactorings.h instead
#endif

#ifdef BUILD_REFACTORINGS
// This include file should not be used because refactorings are enabled - we detected an error
#error Refactorings are enabled - do not use kdevrefactorings_disabled.h
#endif

/**
 * Glue between KDevelop interfaces and refactorings implementation.
 * It exists to separate refactorings (requiring Clang and quite fixed environment) and KDevelop.
 *
 * This is "disabled" version - provides API of standard @c KDevRefactorings implementation but
 * its functions are effectively no-ops
 */
class KDevRefactorings : public QObject
{
    // no Q_OBJECT here
public:
    using QObject::QObject;

    // only stubs
    void fillContextMenu(KDevelop::ContextMenuExtension &extension, KDevelop::Context *context) { }
};

// NOTE: This stub interface does not have associated implementation file - it doesn't have to be
// included in project by any mean (to link with).

#endif //KDEV_CLANG_REFACTORINGSGLUE_DISABLED_H
