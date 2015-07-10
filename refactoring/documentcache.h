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

#ifndef KDEV_CLANG_CACHE_H
#define KDEV_CLANG_CACHE_H

// C++ std
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

// Qt
#include <QObject>

// LLVM
#include <llvm/ADT/StringMap.h>
#include <clang/Tooling/Tooling.h>
#include <language/codegen/documentchangeset.h>
#include <clang/Tooling/Refactoring.h>

namespace KDevelop
{
class IDocumentController;

class IDocument;
};

class RefactoringContext;

/**
 * Implementation of documents cache for use with libTooling
 */
class DocumentCache : public QObject
{
    Q_OBJECT;
    Q_DISABLE_COPY(DocumentCache);
public:
    DocumentCache(RefactoringContext *parent);

    bool fileIsOpened(llvm::StringRef fileName) const;

    llvm::StringRef contentOfOpenedFile(llvm::StringRef fileName);

    clang::tooling::RefactoringTool &refactoringTool();

private:
    /// Some modification occurred and we must mark this document as dirty
    void handleDocumentModified(KDevelop::IDocument *document);

private:
    std::unique_ptr<clang::tooling::RefactoringTool> m_refactoringTool;
    std::unordered_map<std::string, std::string> m_cachedFiles;  // This cache if very volatile
    bool m_dirty = true;

    llvm::StringMap<std::unique_ptr<std::pair<std::string, std::string>>> m_data;
};


#endif //KDEV_CLANG_CACHE_H
