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

#include "documentcache.h"

// KDevelop
#include <kdevplatform/interfaces/idocumentcontroller.h>
#include <kdevplatform/interfaces/idocument.h>
#include <kdevplatform/interfaces/icore.h>

#include "refactoringcontext.h"
#include "utils.h"
#include "../util/clangdebug.h"

using namespace KDevelop;

DocumentCache::DocumentCache(RefactoringContext *parent)
    : QObject(parent)
{
    connect(ICore::self()->documentController(), &IDocumentController::documentContentChanged, this,
            &DocumentCache::handleDocumentModified);
}

clang::tooling::RefactoringTool &DocumentCache::refactoringTool()
{
    if (m_dirty) {
        m_dirty = false;
        const auto ctx = static_cast<RefactoringContext *>(parent());
        m_refactoringTool = makeRefactoringTool(*ctx->database, ctx->database->getAllFiles());
        for (auto document : ICore::self()->documentController()->openDocuments()) {
            KTextEditor::Document *textDocument = document->textDocument();
            if (textDocument == nullptr) {
                continue;
            }
            auto obj = cpp::make_unique<std::pair<std::string, std::string>>(
                document->url().toLocalFile().toStdString(),
                textDocument->text().toStdString());
            llvm::StringRef key = obj->first;
            m_data[key] = std::move(obj);
            m_refactoringTool->mapVirtualFile(m_data[key]->first, m_data[key]->second);
        }
    }
    return *m_refactoringTool.get();
}

clang::tooling::RefactoringTool DocumentCache::refactoringToolForFile(
    const std::string &fileName)
{
    // try to prepare RefactoringTool just for @p fileName
    // if we don't have compile command for this file (e.g. it is a header file) then return
    // general version
    const auto ctx = static_cast<RefactoringContext *>(parent());
    const auto &files = ctx->database->getAllFiles();
    if (std::find_if(files.begin(), files.end(),
                     [&fileName](const std::string &file)
                     {
                         return llvm::sys::fs::equivalent(file, fileName);
                     }) != files.end()) {
        auto result = clang::tooling::RefactoringTool(*ctx->database, {fileName});
        result.mapVirtualFile(fileName, contentOfOpenedFile(fileName));
        return result;
    } else {
        return refactoringTool();   // a copy of
    }
}

void DocumentCache::handleDocumentModified(KDevelop::IDocument *document)
{
    m_dirty = true;
    m_refactoringTool.reset();
    m_data.clear();
    m_cachedFiles.erase(document->url().toLocalFile().toStdString());
}

bool DocumentCache::fileIsOpened(llvm::StringRef fileName) const
{
    return ICore::self()->documentController()->documentForUrl(
        QUrl::fromLocalFile(QString::fromStdString(fileName.str()))) != nullptr;
}

llvm::StringRef DocumentCache::contentOfOpenedFile(llvm::StringRef fileName)
{
    auto i = m_cachedFiles.find(fileName);
    if (i != m_cachedFiles.end()) {
        return i->second;
    }
    auto documentController = ICore::self()->documentController();
    IDocument *document = documentController->documentForUrl(
        QUrl::fromLocalFile(QString::fromStdString(fileName.str())));
    Q_ASSERT(document);
    KTextEditor::Document *textDocument = document->textDocument();
    Q_ASSERT(textDocument);
    std::string key = fileName;
    m_cachedFiles[key] = textDocument->text().toStdString();
    connect(documentController, &IDocumentController::documentClosed, this, [this, key]
    {
        m_cachedFiles.erase(key);
    });
    return m_cachedFiles[key];
}
