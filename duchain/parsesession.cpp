/*
    This file is part of KDevelop

    Copyright 2013 Olivier de Gaalon <olivier.jg@gmail.com>
    Copyright 2013 Milian Wolff <mail@milianw.de>
    Copyright 2013 Kevin Funk <kfunk@kde.org>

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

#include "parsesession.h"
#include "clangproblem.h"
#include "clangdiagnosticevaluator.h"
#include "todoextractor.h"
#include "clanghelpers.h"
#include "clangindex.h"
#include "clangparsingenvironment.h"
#include "util/clangdebug.h"
#include "util/clangtypes.h"
#include "util/clangutils.h"

#include <language/duchain/duchainlock.h>
#include <language/duchain/duchain.h>

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>

using namespace KDevelop;

namespace {

QVector<QByteArray> argsForSession(const QString& path, ParseSessionData::Options options, const ParserSettings& parserSettings)
{
    QMimeDatabase db;
    if(db.mimeTypeForFile(path).name() == QStringLiteral("text/x-objcsrc")) {
        return {QByteArrayLiteral("-xobjective-c++")};
    }

    // this can happen for unit tests that use the ParseSession directly
    if (parserSettings.parserOptions.isEmpty()) {
        return {QByteArrayLiteral("-fspell-checking"), QByteArrayLiteral("-Wdocumentation"), QByteArrayLiteral("-std=c++11"), QByteArrayLiteral("-xc++"), QByteArrayLiteral("-Wall"), QByteArrayLiteral("-nostdinc"), QByteArrayLiteral("-nostdinc++")};
    }

    auto result = parserSettings.toClangAPI();
    result.append(QByteArrayLiteral("-nostdinc"));
    result.append(QByteArrayLiteral("-nostdinc++"));

    if (options & ParseSessionData::PrecompiledHeader) {
        result.append(parserSettings.isCpp() ? QByteArrayLiteral("-xc++-header") : QByteArrayLiteral("-xc-header"));
        return result;
    }

    result.append(parserSettings.isCpp() ? QByteArrayLiteral("-xc++") : QByteArrayLiteral("-xc"));
    return result;
}

void addIncludes(QVector<const char*>* args, QVector<QByteArray>* otherArgs,
                 const Path::List& includes, const char* cliSwitch)
{
    foreach (const Path& url, includes) {
        QFileInfo info(url.toLocalFile());
        QByteArray path = url.toLocalFile().toUtf8();

        if (info.isFile()) {
            path.prepend("-include");
        } else {
            path.prepend(cliSwitch);
        }
        otherArgs->append(path);
        args->append(path.constData());
    }
}

QVector<CXUnsavedFile> toClangApi(const QVector<UnsavedFile>& unsavedFiles)
{
    QVector<CXUnsavedFile> unsaved;
    unsaved.reserve(unsavedFiles.size());
    std::transform(unsavedFiles.begin(), unsavedFiles.end(),
                std::back_inserter(unsaved),
                [] (const UnsavedFile& file) { return file.toClangApi(); });
    return unsaved;
}

}

ParseSessionData::ParseSessionData(const QVector<UnsavedFile>& unsavedFiles, ClangIndex* index,
                                   const ClangParsingEnvironment& environment, Options options)
    : m_file(nullptr)
    , m_unit(nullptr)
{
    unsigned int flags = CXTranslationUnit_CXXChainedPCH
        | CXTranslationUnit_DetailedPreprocessingRecord;
    if (options.testFlag(SkipFunctionBodies)) {
        flags |= CXTranslationUnit_SkipFunctionBodies;
    }
    if (options.testFlag(PrecompiledHeader)) {
        flags |= CXTranslationUnit_ForSerialization;
    } else {
        flags |= CXTranslationUnit_CacheCompletionResults
              |  CXTranslationUnit_PrecompiledPreamble
              |  CXTranslationUnit_Incomplete;
    }

    const auto tuUrl = environment.translationUnitUrl();
    Q_ASSERT(!tuUrl.isEmpty());

    const auto arguments = argsForSession(tuUrl.str(), options, environment.parserSettings());
    QVector<const char*> clangArguments;

    const auto& includes = environment.includes();
    const auto& pchInclude = environment.pchInclude();

    // uses QByteArray as smart-pointer for const char* ownership
    QVector<QByteArray> smartArgs;
    smartArgs.reserve(includes.system.size() + includes.project.size()
                      + pchInclude.isValid() + arguments.size() + 1);
    clangArguments.reserve(smartArgs.size());

    std::transform(arguments.constBegin(), arguments.constEnd(),
                   std::back_inserter(clangArguments),
                   [] (const QByteArray &argument) { return argument.constData(); });

    // NOTE: the PCH include must come before all other includes!
    if (pchInclude.isValid()) {
        clangArguments << "-include";
        QByteArray pchFile = pchInclude.toLocalFile().toUtf8();
        smartArgs << pchFile;
        clangArguments << pchFile.constData();
    }
    addIncludes(&clangArguments, &smartArgs, includes.system, "-isystem");
    addIncludes(&clangArguments, &smartArgs, includes.project, "-I");

    m_definesFile.open();
    QTextStream definesStream(&m_definesFile);
    Q_ASSERT(m_definesFile.isWritable());
    const auto& defines = environment.defines();
    for (auto it = defines.begin(); it != defines.end(); ++it) {
        definesStream << QStringLiteral("#define ") << it.key() << ' ' << it.value() << '\n';
    }
    definesStream.flush();
    smartArgs << m_definesFile.fileName().toUtf8();
    clangArguments << "-imacros" << smartArgs.last().constData();

    QVector<CXUnsavedFile> unsaved;
    //For PrecompiledHeader, we don't want unsaved contents (and contents.isEmpty())
    if (!options.testFlag(PrecompiledHeader)) {
        unsaved = toClangApi(unsavedFiles);
    }

    const CXErrorCode code = clang_parseTranslationUnit2(
        index->index(), tuUrl.byteArray().constData(),
        clangArguments.constData(), clangArguments.size(),
        unsaved.data(), unsaved.size(),
        flags,
        &m_unit
    );
    if (code != CXError_Success) {
        qWarning() << "clang_parseTranslationUnit2 return with error code" << code;
    }

    if (m_unit) {
        setUnit(m_unit);
        m_environment = environment;

        if (options.testFlag(PrecompiledHeader)) {
            clang_saveTranslationUnit(m_unit, (tuUrl.byteArray() + ".pch").constData(), CXSaveTranslationUnit_None);
        }
    } else {
        qWarning() << "Failed to parse translation unit:" << tuUrl;
    }
}

ParseSessionData::~ParseSessionData()
{
    clang_disposeTranslationUnit(m_unit);
}

void ParseSessionData::setUnit(CXTranslationUnit unit)
{
    Q_ASSERT(!m_unit || unit == m_unit);

    m_unit = unit;
    const ClangString unitFile(clang_getTranslationUnitSpelling(unit));
    m_file = clang_getFile(m_unit, unitFile.c_str());
}

ClangParsingEnvironment ParseSessionData::environment() const
{
    return m_environment;
}

ParseSession::ParseSession(const ParseSessionData::Ptr& data)
    : d(data)
{
    if (d) {
        ENSURE_CHAIN_NOT_LOCKED
        d->m_mutex.lock();
    }
}

ParseSession::~ParseSession()
{
    if (d) {
        d->m_mutex.unlock();
    }
}

void ParseSession::setData(const ParseSessionData::Ptr& data)
{
    if (data == d) {
        return;
    }

    if (d) {
        d->m_mutex.unlock();
    }

    d = data;

    if (d) {
        ENSURE_CHAIN_NOT_LOCKED
        d->m_mutex.lock();
    }
}

ParseSessionData::Ptr ParseSession::data() const
{
    return d;
}

IndexedString ParseSession::languageString()
{
    static const IndexedString lang("Clang");
    return lang;
}

QList<ProblemPointer> ParseSession::problemsForFile(CXFile file) const
{
    if (!d) {
        return {};
    }

    QList<ProblemPointer> problems;

    // extra clang diagnostics
    const uint numDiagnostics = clang_getNumDiagnostics(d->m_unit);
    problems.reserve(numDiagnostics);
    for (uint i = 0; i < numDiagnostics; ++i) {
        auto diagnostic = clang_getDiagnostic(d->m_unit, i);

        CXSourceLocation location = clang_getDiagnosticLocation(diagnostic);
        CXFile diagnosticFile;
        clang_getFileLocation(location, &diagnosticFile, nullptr, nullptr, nullptr);
        if (diagnosticFile != file) {
            continue;
        }

        ProblemPointer problem(ClangDiagnosticEvaluator::createProblem(diagnostic, d->m_unit));
        problems << problem;

        clang_disposeDiagnostic(diagnostic);
    }

    const QString path = QDir::cleanPath(ClangString(clang_getFileName(file)).toString());
    const IndexedString indexedPath(path);

    TodoExtractor extractor(unit(), file);
    problems << extractor.problems();

    // other problem sources
    if (ClangHelpers::isHeader(path) && !clang_isFileMultipleIncludeGuarded(unit(), file)
        && !clang_Location_isInSystemHeader(clang_getLocationForOffset(d->m_unit, file, 0)))
    {
        ProblemPointer problem(new Problem);
        problem->setSeverity(IProblem::Warning);
        problem->setDescription(i18n("Header is not guarded against multiple inclusions"));
        problem->setExplanation(i18n("The given header is not guarded against multiple inclusions, "
            "either with the conventional #ifndef/#define/#endif macro guards or with #pragma once."));
        problem->setFinalLocation({indexedPath, KTextEditor::Range()});
        problem->setSource(IProblem::Preprocessor);
        problems << problem;
        // TODO: Easy to add an assistant here that adds the guards -- any takers?
    }

    return problems;
}

CXTranslationUnit ParseSession::unit() const
{
    return d ? d->m_unit : nullptr;
}

CXFile ParseSession::file(const QByteArray& path) const
{
    return clang_getFile(unit(), path.constData());
}

CXFile ParseSession::mainFile() const
{
    return d ? d->m_file : nullptr;
}

bool ParseSession::reparse(const QVector<UnsavedFile>& unsavedFiles, const ClangParsingEnvironment& environment)
{
    if (!d || environment != d->m_environment) {
        return false;
    }

    auto unsaved = toClangApi(unsavedFiles);

    if (clang_reparseTranslationUnit(d->m_unit, unsaved.size(), unsaved.data(), clang_defaultReparseOptions(d->m_unit)) == 0) {
        d->setUnit(d->m_unit);
        return true;
    } else {
        return false;
    }
}

ClangParsingEnvironment ParseSession::environment() const
{
    return d->m_environment;
}
