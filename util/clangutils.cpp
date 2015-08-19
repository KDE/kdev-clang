/*
 * Copyright 2014 Kevin Funk <kfunk@kde.org>
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

#include "clangutils.h"

#include "../util/clangdebug.h"
#include "../util/clangtypes.h"
#include "../duchain/cursorkindtraits.h"

#include <serialization/indexedstring.h>
#include <language/editor/documentrange.h>

#include <clang-c/Index.h>

#include <QTextStream>

using namespace KDevelop;

CXCursor ClangUtils::getCXCursor(int line, int column, const CXTranslationUnit& unit, const CXFile& file)
{
    if (!file) {
        clangDebug() << "getCXCursor couldn't find file: " << clang_getFileName(file);
        return clang_getNullCursor();
    }

    CXSourceLocation location = clang_getLocation(unit, file, line + 1, column + 1);

    if (clang_equalLocations(clang_getNullLocation(), location)) {
        clangDebug() << "getCXCursor given invalid position " << line << ", " << column
                << " for file " << clang_getFileName(file);
        return clang_getNullCursor();
    }

    return clang_getCursor(unit, location);
}

namespace {

struct FunctionInfo {
    KTextEditor::Range range;
    QString fileName;
    CXTranslationUnit unit;
    QStringList stringParts;
};

CXChildVisitResult paramVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData data)
{
    //Ignore the type of the parameter
    CXCursorKind kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_TypeRef || kind == CXCursor_TemplateRef || kind == CXCursor_NamespaceRef) {
        return CXChildVisit_Continue;
    }

    FunctionInfo *info = static_cast<FunctionInfo*>(data);
    CXToken *tokens;
    unsigned int numTokens;
    ClangRange range(clang_getCursorExtent(cursor));

    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor),&file,nullptr,nullptr,nullptr);
    if (!file) {
        clangDebug() << "Couldn't find file associated with default parameter cursor!";
        //We keep going, because getting an error because we accidentally duplicated
        //a default parameter is better than deleting a default parameter
    }
    QString fileName = ClangString(clang_getFileName(file)).toString();

    //Clang doesn't make a distinction between the default arguments being in
    //the declaration or definition, and the default arguments don't have lexical
    //parents. So this range check is the only thing that really works.
    if ((info->fileName.isEmpty() || fileName == info->fileName) && info->range.contains(range.toRange())) {
        clang_tokenize(info->unit, range.range(), &tokens, &numTokens);
        for (unsigned int i = 0; i < numTokens; i++) {
            info->stringParts.append(ClangString(clang_getTokenSpelling(info->unit, tokens[i])).toString());
        }

        clang_disposeTokens(info->unit, tokens, numTokens);
    }
    return CXChildVisit_Continue;
}

}

QVector<QString> ClangUtils::getDefaultArguments(CXCursor cursor, DefaultArgumentsMode mode)
{
    if (!CursorKindTraits::isFunction(clang_getCursorKind(cursor))) {
        return QVector<QString>();
    }

    int numArgs = clang_Cursor_getNumArguments(cursor);
    QVector<QString> arguments(mode == FixedSize ? numArgs : 0);
    QString fileName;
    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor),&file,nullptr,nullptr,nullptr);
    if (!file) {
        clangDebug() << "Couldn't find file associated with default parameter cursor!";
        //The empty string serves as a wildcard string, because it's better to
        //duplicate a default parameter than delete one
    } else {
        fileName = ClangString(clang_getFileName(file)).toString();
    }

    FunctionInfo info{ClangRange(clang_getCursorExtent(cursor)).toRange(), fileName,
                      clang_Cursor_getTranslationUnit(cursor), QStringList()};

    for (int i = 0; i < numArgs; i++) {
        CXCursor arg = clang_Cursor_getArgument(cursor, i);
        info.stringParts.clear();
        clang_visitChildren(arg, paramVisitor, &info);

        //Clang includes the equal sign sometimes, but not other times.
        if (!info.stringParts.isEmpty() && info.stringParts.first() == QLatin1String("=")) {
            info.stringParts.removeFirst();
        }
        //Clang seems to include the , or ) at the end of the param, so delete that
        if (!info.stringParts.isEmpty() && (info.stringParts.last() == QLatin1String(",") || info.stringParts.last() == QLatin1String(")"))) {
            info.stringParts.removeLast();
        }

        const QString result = info.stringParts.join(QString());
        if (mode == FixedSize) {
            arguments.replace(i, result);
        } else if (!result.isEmpty()) {
            arguments << result;
        }
    }
    return arguments;
}

bool ClangUtils::isFileEqual(CXFile file1, CXFile file2)
{
#if CINDEX_VERSION_MINOR >= 28
    return clang_File_isEqual(file1, file2);
#else
    // note: according to the implementation of clang_File_isEqual, file1 and file2 can still be equal,
    // regardless of whether file1 == file2 is true or not
    // however, we didn't see any problems with pure pointer comparisions until now, so fall back to that
    return file1 == file2;
#endif
}

constexpr bool isScopeKind(CXCursorKind kind)
{
    return kind == CXCursor_Namespace || kind == CXCursor_StructDecl ||
           kind == CXCursor_UnionDecl || kind == CXCursor_ClassDecl  ||
           kind == CXCursor_ClassTemplate || kind == CXCursor_ClassTemplatePartialSpecialization;
}

QString ClangUtils::getScope(CXCursor cursor)
{
    QStringList scope;
    CXCursor destContext = clang_getCanonicalCursor(clang_getCursorLexicalParent(cursor));
    CXCursor search = clang_getCursorSemanticParent(cursor);
    while (isScopeKind(clang_getCursorKind(search)) && !clang_equalCursors(search, destContext)) {
        scope.prepend(ClangString(clang_getCursorSpelling(search)).toString());
        search = clang_getCursorSemanticParent(search);
    }
    return scope.join(QStringLiteral("::"));
}

QString ClangUtils::getCursorSignature(CXCursor cursor, const QString& scope, const QVector<QString>& defaultArgs)
{
    CXCursorKind kind = clang_getCursorKind(cursor);
    //Get the return type
    QString ret;
    ret.reserve(128);
    QTextStream stream(&ret);
    if (kind != CXCursor_Constructor && kind != CXCursor_Destructor) {
        stream << ClangString(clang_getTypeSpelling(clang_getCursorResultType(cursor))).toString()
               << ' ';
    }

    //Build the function name, with scope and parameters
    if (!scope.isEmpty()) {
        stream << scope << "::";
    }

    QString functionName = ClangString(clang_getCursorSpelling(cursor)).toString();
    if (functionName.contains(QLatin1Char('<'))) {
        stream << functionName.left(functionName.indexOf(QLatin1Char('<')));
    } else {
        stream << functionName;
    }

    //Add the parameters and such
    stream << '(';
    int numArgs = clang_Cursor_getNumArguments(cursor);
    for (int i = 0; i < numArgs; i++) {
        CXCursor arg = clang_Cursor_getArgument(cursor, i);

        //Clang formats pointer types as "t *x" and reference types as "t &x", while
        //KDevelop formats them as "t* x" and "t& x". Make that adjustment.
        const QString type = ClangString(clang_getTypeSpelling(clang_getCursorType(arg))).toString();
        if (type.endsWith(QLatin1String(" *")) || type.endsWith(QLatin1String(" &"))) {
            stream << type.left(type.length() - 2) << type.at(type.length() - 1);
        } else {
            stream << type;
        }

        const QString id = ClangString(clang_getCursorDisplayName(arg)).toString();
        if (!id.isEmpty()) {
            stream << ' ' << id;
        }

        if (i < defaultArgs.count() && !defaultArgs.at(i).isEmpty()) {
            stream << " = " << defaultArgs.at(i);
        }

        if (i < numArgs - 1) {
            stream << ", ";
        }
    }

    if (clang_Cursor_isVariadic(cursor)) {
        if (numArgs > 0) {
            stream << ", ";
        }
        stream << "...";
    }

    stream << ')';

    if (clang_CXXMethod_isConst(cursor)) {
        stream << " const";
    }

    return ret;
}

QByteArray ClangUtils::getRawContents(CXTranslationUnit unit, CXSourceRange range)
{
    const auto rangeStart = clang_getRangeStart(range);
    const auto rangeEnd = clang_getRangeEnd(range);
    unsigned int start, end;
    clang_getFileLocation(rangeStart, nullptr, nullptr, nullptr, &start);
    clang_getFileLocation(rangeEnd, nullptr, nullptr, nullptr, &end);

    QByteArray result;
    CXToken *tokens = 0;
    unsigned int nTokens = 0;
    clang_tokenize(unit, range, &tokens, &nTokens);
    for (unsigned int i = 0; i < nTokens; i++) {
        const auto location = ClangLocation(clang_getTokenLocation(unit, tokens[i]));
        unsigned int offset;
        clang_getFileLocation(location, nullptr, nullptr, nullptr, &offset);
        Q_ASSERT(offset >= start);
        const int fillCharacters = offset - start - result.size();
        Q_ASSERT(fillCharacters >= 0);
        result.append(QByteArray(fillCharacters, ' '));
        const auto spelling = clang_getTokenSpelling(unit, tokens[i]);
        result.append(clang_getCString(spelling));
        clang_disposeString(spelling);
    }
    clang_disposeTokens(unit, tokens, nTokens);

    // Clang always appends the full range of the last token, even if this exceeds the end of the requested range.
    // Fix this.
    result.chop((result.size() - 1) - (end - start));

    return result;
}
