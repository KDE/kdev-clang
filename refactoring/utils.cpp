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

#include "utils.h"

// Qt
#include <QString>

// Clang
#include <clang/Lex/Lexer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Refactoring.h>

#include "util/clangdebug.h"

using namespace clang;
using namespace clang::tooling;
using namespace KDevelop;

using llvm::StringRef;
using llvm::ErrorOr;

std::unique_ptr<CompilationDatabase> makeCompilationDatabaseFromCMake(const std::string &buildPath,
                                                                      QString &errorMessage)
{
    std::string msg;
    auto result = CompilationDatabase::loadFromDirectory(buildPath, msg);
    errorMessage = QString::fromStdString(msg);
    return result;
}

std::unique_ptr<RefactoringTool> makeRefactoringTool(const CompilationDatabase &database,
                                                     const std::vector<std::string> &sources)
{
    auto result = cpp::make_unique<RefactoringTool>(database, sources);
    return result;
}

enum class EndOfLine
{
    LF,
    CRLF,
    CR,
};

/// Detect end of line marker in @p text
static EndOfLine endOfLine(StringRef text)
{
    bool seenCr = false;
    for (auto c : text) {
        switch (c) {
        case '\r':
            seenCr = true;
            break;
        case '\n':
            if (seenCr) {
                return EndOfLine::CRLF;
            } else {
                return EndOfLine::LF;
            }
        default:
            if (seenCr) {
                return EndOfLine::CR;
            }
        }
    }
    // if file has only one line or ends with CR
    return seenCr ? EndOfLine::CR : EndOfLine::LF;
}

// Cached entries need manual handling because these are outside of FileManager
static KTextEditor::Range toRange(StringRef text, unsigned offset, unsigned length)
{
    Q_ASSERT(offset + length < text.size());
    unsigned lastLine = 0;
    unsigned lastColumn = 0;
    // last* is our cursor
    const EndOfLine eolMarker = endOfLine(text);
    // LF always ends line
    // Shift cursor in text assuming cursor is in @p offset and eating @p length chars
    auto shift = [&text, &lastLine, &lastColumn, eolMarker]
            (unsigned start, unsigned length)
    {
        for (unsigned i = start; i < start + length && i < text.size(); ++i) {
            switch (text[i]) {
            case '\r':
                if (eolMarker == EndOfLine::CR) {
                    lastLine++;
                    lastColumn = 0;
                }
                else {
                    lastColumn++;
                }
                break;
            case '\n':
                lastLine++;
                lastColumn = 0;
                break;
            default:
                lastColumn++;
            }
        }
    };
    shift(0, offset);
    const KTextEditor::Cursor start(lastLine, lastColumn);
    shift(offset, length);
    const KTextEditor::Cursor end(lastLine, lastColumn);
    return KTextEditor::Range(std::move(start), std::move(end));
}

/// Decides if file should be taken from cache or file system and takes it
static ErrorOr<StringRef> readFileContent(StringRef name, DocumentCache *cache,
                                          FileManager &fileManager)
{
    if (cache->fileIsOpened(name)) {
        // It would be great if we could get rid of this use of cache
        return cache->contentOfOpenedFile(name);
    } else {
        auto r = fileManager.getBufferForFile(name);
        if (!r) {
            return r.getError();
        }
        return r.get()->getBuffer();
    }
}

static ErrorOr<DocumentChange> toDocumentChange(const Replacement &replacement,
                                                DocumentCache *cache,
                                                FileManager &fileManager)
{
    // (Clang) FileManager is unaware of cache (from ClangTool) (cache is applied just before run)
    // SourceManager enumerates columns counting from 1 (probably also lines)
    // Is ClangTool doing refactoring on mapped files? Certainly Replacements cannot be applied
    // on cache (not a problem - Refactorings are translated to DocumentChangeSet HERE)

    // IndexedString constructor has this limitation
    Q_ASSERT(replacement.getFilePath().size() <=
             std::numeric_limits<unsigned short>::max());

    ErrorOr<StringRef> fileContent = readFileContent(replacement.getFilePath(), cache, fileManager);
    if (!fileContent) {
        return fileContent.getError();
    }
    auto result = DocumentChange(
            IndexedString(
                    replacement.getFilePath().data(),
                    static_cast<unsigned short>(
                            replacement.getFilePath().size()
                    )
            ),
            toRange(
                    fileContent.get(),
                    replacement.getOffset(),
                    replacement.getLength()
            ),
            QString(), // we don't have this data
            QString::fromStdString(replacement.getReplacementText())
            // NOTE: above conversion assumes UTF-8 encoding
    );
    result.m_ignoreOldText = true;
    return result;
}

ErrorOr<DocumentChangeSet> toDocumentChangeSet(const Replacements &replacements,
                                               DocumentCache *cache,
                                               FileManager &fileManager)
{
    // NOTE: DocumentChangeSet can handle file renaming, libTooling will not do that, but it may be
    // reasonable in some cases (renaming of a class, ...). This feature may be used outside to
    // further polish result.
    DocumentChangeSet result;
    for (const auto &r : replacements) {
        ErrorOr<DocumentChange> documentChange = toDocumentChange(r, cache, fileManager);
        if (!documentChange) {
            return documentChange.getError();
        }
        result.addChange(documentChange.get());
    }
    return result;
}

/**
 * Translate to definite eol (last character in line)
 */
static char toChar(EndOfLine eol)
{
    switch (eol) {
    case EndOfLine::LF:
    case EndOfLine::CRLF:
        return '\n';
    case EndOfLine::CR:
        return '\r';
    }
    Q_ASSERT(false);
    return '\0';
}

ErrorOr<unsigned> toOffset(const std::string &fileName, const KTextEditor::Cursor &position,
                           ClangTool &clangTool,
                           DocumentCache *documentCache)
{
    auto fileContent = readFileContent(fileName, documentCache, clangTool.getFiles());
    if (!fileContent) {
        return fileContent.getError();
    }
    int currentLine = 0;
    unsigned currentOffset = 0;
    const char eol = toChar(endOfLine(fileContent.get()));
    auto i = fileContent.get().begin();
    while (currentLine < position.line()) {
        Q_ASSERT(i != fileContent.get().end());
        auto c = *i++;
        currentOffset++;
        if (c == eol) {
            currentLine++;
        }
    }
    int currentColumn = 0;
    while (currentColumn < position.column()) {
        // FIXME: cursor after last character crashes
        Q_ASSERT(i != fileContent.get().end());
        i++;
        currentColumn++;
        currentOffset++;
    }
    return currentOffset;
}

bool isInRange(const std::string &fileName, unsigned offset, SourceLocation start,
               SourceLocation end, const SourceManager &sourceManager)
{
    auto startD = sourceManager.getDecomposedLoc(start);
    auto endD = sourceManager.getDecomposedLoc(end);
    auto fileEntry = sourceManager.getFileEntryForID(startD.first);
    if (fileEntry == nullptr) {
        return false;
    }
    if (!llvm::sys::fs::equivalent(fileEntry->getName(), fileName)) {
        return false;
    }
    return startD.second <= offset && offset <= endD.second;
}

bool isInRange(const std::string &fileName, unsigned offset, SourceRange range,
               const SourceManager &sourceManager)
{
    return isInRange(fileName, offset, range.getBegin(), range.getEnd(), sourceManager);
}

SourceRange tokenRangeToCharRange(SourceRange range, const SourceManager &sourceManager,
                                  const LangOptions &langOptions)
{
    SourceRange result(range.getBegin(), range.getEnd().getLocWithOffset(
            Lexer::MeasureTokenLength(range.getEnd(), sourceManager, langOptions)));
    return result;
}

SourceRange tokenRangeToCharRange(SourceRange range,
                                  const CompilerInstance &CI)
{
    return tokenRangeToCharRange(range, CI.getSourceManager(), CI.getLangOpts());
}

bool isLocationEqual(const std::string &fileName, unsigned offset, clang::SourceLocation location,
                     const clang::SourceManager &sourceManager)
{
    auto locationD = sourceManager.getDecomposedLoc(location);
    auto fileEntry = sourceManager.getFileEntryForID(locationD.first);
    if (fileEntry == nullptr) {
        return false;
    }
    if (!llvm::sys::fs::equivalent(fileEntry->getName(), fileName)) {
        return false;
    }
    return locationD.second == offset;
}

