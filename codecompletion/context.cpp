/*
 * This file is part of KDevelop
 * Copyright 2014 Milian Wolff <mail@milianw.de>
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
 */

#include "context.h"

#include <language/duchain/duchainlock.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/types/functiontype.h>
#include <language/interfaces/iastcontainer.h>
#include <language/codecompletion/codecompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>
#include <language/codecompletion/normaldeclarationcompletionitem.h>

#include "../util/clangdebug.h"
#include "../util/clangtypes.h"
#include "../duchain/parsesession.h"
#include "../duchain/navigationwidget.h"

#include <memory>

#include <KTextEditor/Document>
#include <KTextEditor/View>

using namespace KDevelop;

namespace {
/// Completion results with priority below this value will be shown in "Best Matches" group
/// See http://clang.llvm.org/doxygen/CodeCompleteConsumer_8h_source.html
/// We currently treat CCP_SuperCompletion = 20 as the highest priority that still gives a "Best Match"
const unsigned int MAX_PRIORITY_FOR_BEST_MATCHES = 20;
/// Maximum return-type string length in completion items
const int MAX_RETURN_TYPE_STRING_LENGTH = 20;

/**
 * Common base class for Clang code completion items.
 */
template<class Base>
class CompletionItem : public Base
{
public:
    CompletionItem(const QString& display, const QString& prefix)
        : Base()
        , m_display(display)
        , m_prefix(prefix)
    {
    }

    virtual ~CompletionItem() = default;

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* /*model*/) const override
    {
        if (role == Qt::DisplayRole) {
            if (index.column() == CodeCompletionModel::Prefix) {
                return m_prefix;
            } else if (index.column() == CodeCompletionModel::Name) {
                return m_display;
            }
        }
        return {};
    }

protected:
    QString m_display;
    QString m_prefix;
};

class OverrideItem : public CompletionItem<CompletionTreeItem>
{
public:
    OverrideItem(const QString& nameAndParams, const QString& returnType)
        : CompletionItem<KDevelop::CompletionTreeItem>(
              nameAndParams,
              i18n("Override %1", returnType)
          )
        , m_returnType(returnType)
    {
    }

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* model) const override
    {
        if (role == Qt::DecorationRole) {
            if (index.column() == KTextEditor::CodeCompletionModel::Icon) {
                static const QIcon icon = QIcon::fromTheme("CTparents");
                return icon;
            }
        }
        return CompletionItem<CompletionTreeItem>::data(index, role, model);
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        view->document()->replaceText(word, "virtual " + m_returnType + ' ' + m_display);
    }

private:
    QString m_returnType;
};

class ImplementsItem : public CompletionItem<CompletionTreeItem>
{
public:
    ImplementsItem(const FuncImplementInfo& item)
        : CompletionItem<KDevelop::CompletionTreeItem>(
              item.prototype,
              i18n("Implement %1", item.isConstructor ? "<constructor>" :
                                   item.isDestructor ? "<destructor>" : item.returnType)
          )
        , m_item(item)
    {
    }

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* model) const override
    {
        if (role == Qt::DecorationRole) {
            if (index.column() == KTextEditor::CodeCompletionModel::Icon) {
                static const QIcon icon = QIcon::fromTheme("CTsuppliers");
                return icon;
            }
        }
        return CompletionItem<CompletionTreeItem>::data(index, role, model);
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        QString replacement = m_item.templatePrefix;
        if (!m_item.isDestructor && !m_item.isConstructor) {
            replacement += m_item.returnType + ' ';
        }
        replacement += m_item.prototype + "\n{\n}\n";
        view->document()->replaceText(word, replacement);
    }

private:
    FuncImplementInfo m_item;
};

/**
 * Specialized completion item class for items which are represented by a Declaration
 */
class DeclarationItem : public CompletionItem<NormalDeclarationCompletionItem>
{
public:
    DeclarationItem(Declaration* dec, const QString& display, const QString& prefix, const QString& replacement)
        : CompletionItem<NormalDeclarationCompletionItem>(display, prefix)
        , m_replacement(replacement)
    {
        m_declaration = dec;
    }

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* model) const override
    {
        if (role == CodeCompletionModel::MatchQuality && m_matchQuality) {
            return m_matchQuality;
        }

        auto ret = CompletionItem<NormalDeclarationCompletionItem>::data(index, role, model);
        if (ret.isValid()) {
            return ret;
        }
        return NormalDeclarationCompletionItem::data(index, role, model);
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        QString repl = m_replacement;
        DUChainReadLocker lock;

        if(!m_declaration){
            return;
        }

        if(m_declaration->isFunctionDeclaration()) {
            repl += "()";
            view->document()->replaceText(word, repl);
            auto f = m_declaration->type<FunctionType>();
            if (f && f->indexedArgumentsSize()) {
                view->setCursorPosition(word.start() + KTextEditor::Cursor(0, repl.size() - 1));
            }
        } else {
            view->document()->replaceText(word, repl);
        }
    }

    bool createsExpandingWidget() const override
    {
        return true;
    }

    QWidget* createExpandingWidget(const CodeCompletionModel* /*model*/) const override
    {
        return new ClangNavigationWidget(m_declaration);
    }

    int matchQuality() const
    {
        return m_matchQuality;
    }

    ///Sets match quality from 0 to 10. 10 is the best fit.
    void setMatchQuality(int value)
    {
        m_matchQuality = value;
    }

private:
    int m_matchQuality = 0;
    QString m_replacement;
};

/**
 * A minimalistic completion item for macros and such
 */
class SimpleItem : public CompletionItem<CompletionTreeItem>
{
public:
    SimpleItem(const QString& display, const QString& prefix, const QString& replacement, const QIcon& icon = QIcon())
        : CompletionItem<CompletionTreeItem>(display, prefix)
        , m_replacement(replacement)
        , m_icon(icon)
    {
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        view->document()->replaceText(word, m_replacement);
    }

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* model) const override
    {
        if (role == Qt::DecorationRole && index.column() == KTextEditor::CodeCompletionModel::Icon) {
            return m_icon;
        }
        return CompletionItem<CompletionTreeItem>::data(index, role, model);
    }

private:
    QString m_replacement;
    QIcon m_icon;
};

/**
 * Return true in case position @p position represents a cursor inside a comment
 */
bool isInsideComment(CXTranslationUnit unit, CXFile file, const KTextEditor::Cursor& position)
{
    if (!position.isValid()) {
        return false;
    }

    // TODO: This may get very slow for a large TU, investigate if we can improve this function
    auto begin = clang_getLocation(unit, file, 1, 1);
    auto end = clang_getLocation(unit, file, position.line() + 1, position.column() + 1);
    CXSourceRange range = clang_getRange(begin, end);

    // tokenize the whole range from the start until 'position'
    // if we detect a comment token at this position, return true
    CXToken* tokens = nullptr;
    unsigned int nTokens = 0;
    clang_tokenize(unit, range, &tokens, &nTokens);
    for (unsigned int i = 0; i < nTokens; ++i) {
        CXToken token = tokens[i];
        CXTokenKind tokenKind = clang_getTokenKind(token);
        if (tokenKind != CXToken_Comment) {
            continue;
        }

        auto range = ClangRange(clang_getTokenExtent(unit, token));
        if (range.toRange().contains(position)) {
            return true;
        }
    }
    return false;
}

QString& elideStringRight(QString& str, int length)
{
    if (str.size() > length + 3) {
        return str.replace(length, str.size() - length, "...");
    }
    return str;
}

/**
 * @return Value suited for @ref CodeCompletionModel::MatchQuality in the range [0.0, 10.0] (the higher the better)
 *
 * See http://clang.llvm.org/doxygen/CodeCompleteConsumer_8h_source.html for list of priorities
 * They (currently) have a range from [-3, 80] (the lower, the better)
 */
int codeCompletionPriorityToMatchQuality(unsigned int completionPriority)
{
    return 10u - qBound(0u, completionPriority, 80u) / 8;
}

/**
 * @return Whether the declaration represented by identifier @p identifier qualifies as completion result
 *
 * For example, we don't want to offer SomeClass::SomeClass as completion item to the user
 * (otherwise we'd end up generating code such as 's.SomeClass();')
 */
bool isValidCompletionIdentifier(const QualifiedIdentifier& identifier)
{
    const int count = identifier.count();
    if (identifier.count() < 2) {
        return true;
    }

    const Identifier scope = identifier.at(count-2);
    const Identifier id = identifier.last();
    if (scope == id) {
        return false; // is constructor
    }
    const QString idString = id.toString();
    if (idString.startsWith("~") && scope.toString() == idString.midRef(1)) {
        return false; // is destructor
    }
    return true;
}

/**
 * @return Whether the declaration represented by identifier @p identifier qualifies as "special" completion result
 *
 * "Special" completion results are items that are likely not regularly used.
 *
 * Examples:
 * - 'SomeClass::operator=(const SomeClass&)'
 */
bool isValidSpecialCompletionIdentifier(const QualifiedIdentifier& identifier)
{
    if (identifier.count() < 2) {
        return false;
    }

    const Identifier id = identifier.last();
    const QString idString = id.toString();
    if (idString.startsWith("operator=")) {
        return true; // is assignment operator
    }
    return false;
}

void addEnumItems(Declaration* declaration, QHash<QualifiedIdentifier, Declaration*>& declarationsCache);

/// Add declarations from namespace into @p declarationsCache
void addNamespaceItems(Declaration* declaration, QHash<QualifiedIdentifier, Declaration*>& declarationsCache)
{
    if (declaration->kind() != Declaration::Namespace || !declaration->internalContext()) {
        return;
    }

    const auto namespaceDeclarations = declaration->internalContext()->localDeclarations();
    for (const auto& nd : namespaceDeclarations) {
        declarationsCache.insert(nd->qualifiedIdentifier(), nd);
        addEnumItems(nd, declarationsCache);
    }
}

/// Add enumerators into @p declarationsCache
void addEnumItems(Declaration* declaration, QHash<QualifiedIdentifier, Declaration*>& declarationsCache)
{
    if (declaration->kind() != Declaration::Type || !declaration->internalContext()) {
        return;
    }

    const auto ictx = declaration->internalContext();
    if (ictx->type() == DUContext::Enum) {
        for (const auto enumerator : ictx->localDeclarations()) {
            declarationsCache.insert(enumerator->qualifiedIdentifier(), enumerator);
        }
    }
}

QMultiHash<QualifiedIdentifier, Declaration*> generateCache(const DUContextPointer& ctx, const CursorInRevision& position)
{
    const auto allDeclarationsList = ctx->allDeclarations(position, ctx->topContext());
    QMultiHash<QualifiedIdentifier, Declaration*> declarationsHash;

    for (const auto& declaration : allDeclarationsList) {
        // We store function-local declarations with qid like: "function::declaration", but completion items provided by Clang have qid like: "declaration", so we use id here instead.
        declarationsHash.insert(declaration.second ? declaration.first->qualifiedIdentifier() : QualifiedIdentifier(declaration.first->identifier()), declaration.first);

        // Intentionally not recurse into nested namespaces and inner class contexts as the findDeclarations call is much cheaper.
        addNamespaceItems(declaration.first, declarationsHash);
        addEnumItems(declaration.first, declarationsHash);
    }

    return declarationsHash;
}

Declaration* findDeclaration(const QualifiedIdentifier& qid, const DUContextPointer& ctx, const CursorInRevision& position, const QMultiHash<QualifiedIdentifier, Declaration*>& declarationsCache, QSet<Declaration*>& handled)
{
    auto i = declarationsCache.find(qid);
    while (i != declarationsCache.end() && i.key() == qid) {
        auto declaration = i.value();
        if (!handled.contains(declaration)) {
            handled.insert(declaration);
            return declaration;
        }
        ++i;
    }

    const auto foundDeclarations = ctx->findDeclarations(qid, position);
    for (auto dec : foundDeclarations) {
        if (!handled.contains(dec)) {
            handled.insert(dec);
            return dec;
        }
    }

    return nullptr;
}

}

ClangCodeCompletionContext::ClangCodeCompletionContext(const DUContextPointer& context,
                                                       const ParseSessionData::Ptr& sessionData,
                                                       const QUrl& url,
                                                       const KTextEditor::Cursor& position,
                                                       const QString& text
                                                      )
    : CodeCompletionContext(context, text, CursorInRevision::castFromSimpleCursor(position), 0)
    , m_results(nullptr, clang_disposeCodeCompleteResults)
    , m_parseSessionData(sessionData)
{
    const QByteArray file = url.toLocalFile().toUtf8();
    ParseSession session(m_parseSessionData);
    {
        const unsigned int completeOptions = clang_defaultCodeCompleteOptions();

        CXUnsavedFile unsaved;
        unsaved.Filename = file.constData();
        const QByteArray content = m_text.toUtf8();
        unsaved.Contents = content.constData();
        unsaved.Length = content.size() + 1; // + \0-byte

        m_results.reset(clang_codeCompleteAt(session.unit(), file.constData(),
                        position.line() + 1, position.column() + 1,
                        content.isEmpty() ? nullptr : &unsaved, content.isEmpty() ? 0 : 1,
                        completeOptions));

        if (!m_results) {
            qCWarning(KDEV_CLANG) << "Something went wrong during 'clang_codeCompleteAt' for file" << file;
        }
    }

    // check 'isValidPosition' after parsing the new content
    auto clangFile = session.file(file);
    if (!isValidPosition(session.unit(), clangFile)) {
        m_valid = false;
        return;
    }

    m_completionHelper.computeCompletions(session, clangFile, position);
}

ClangCodeCompletionContext::~ClangCodeCompletionContext()
{
}

bool ClangCodeCompletionContext::isValidPosition(CXTranslationUnit unit, CXFile file) const
{
    if (isInsideComment(unit, file, m_position.castToSimpleCursor())) {
        clangDebug() << "Invalid completion context: Inside comment";
        return false;
    }
    return true;
}

QList<CompletionTreeItemPointer> ClangCodeCompletionContext::completionItems(bool& abort, bool /*fullCompletion*/)
{
    if (!m_valid || !m_duContext || !m_results) {
        return {};
    }

    const auto ctx = DUContextPointer(m_duContext->findContextAt(m_position));
    const auto declarationsCache = generateCache(ctx, m_position);

    /// Normal completion items, such as 'void Foo::foo()'
    QList<CompletionTreeItemPointer> items;
    /// Stuff like 'Foo& Foo::operator=(const Foo&)', etc. Not regularly used by our users.
    QList<CompletionTreeItemPointer> specialItems;
    /// Macros from the current context
    QList<CompletionTreeItemPointer> macros;
    /// Builtins reported by Clang
    QList<CompletionTreeItemPointer> builtin;

    QSet<Declaration*> handled;

    clangDebug() << "Clang found" << m_results->NumResults << "completion results";

    for (uint i = 0; i < m_results->NumResults; ++i) {
        if (abort) {
            return {};
        }

        auto result = m_results->Results[i];

        const auto availability = clang_getCompletionAvailability(result.CompletionString);
        if (availability == CXAvailability_NotAvailable || availability == CXAvailability_NotAccessible) {
            continue;
        }

        const bool isMacroDefinition = result.CursorKind == CXCursor_MacroDefinition;
        if (isMacroDefinition && m_filters & NoMacros) {
            continue;
        }

        const bool isBuiltin = (result.CursorKind == CXCursor_NotImplemented);
        if (isBuiltin && m_filters & NoBuiltins) {
            continue;
        }

        const bool isDeclaration = !isMacroDefinition && !isBuiltin;
        if (isDeclaration && m_filters & NoDeclarations) {
            continue;
        }

        const uint chunks = clang_getNumCompletionChunks(result.CompletionString);

        // the string that would be needed to type, usually the identifier of something. Also we use it as name for code completion declaration items.
        QString typed;
        // the display string we use in the simple code completion items, including the function signature.
        QString display;
        // the return type of a function e.g.
        QString resultType;
        // the replacement text when an item gets executed
        QString replacement;
        //BEGIN function signature parsing
        // nesting depth of parentheses
        int parenDepth = 0;
        enum FunctionSignatureState {
            // not yet inside the function signature
            Before,
            // any token is part of the function signature now
            Inside,
            // finished parsing the function signature
            After
        };
        // current state
        FunctionSignatureState signatureState = Before;
        //END function signature parsing
        for (uint j = 0; j < chunks; ++j) {
            const auto kind = clang_getCompletionChunkKind(result.CompletionString, j);
            if (kind == CXCompletionChunk_CurrentParameter || kind == CXCompletionChunk_Optional) {
                continue;
            }

            // We don't need function signature for declaration items, we can get it directly from the declaration. Also adding the function signature to the "display" would break the "Detailed completion" option.
            if(isDeclaration && !typed.isEmpty()){
                break;
            }

            const QString string = ClangString(clang_getCompletionChunkText(result.CompletionString, j)).toString();
            switch (kind) {
                case CXCompletionChunk_TypedText:
                    display += string;
                    typed = string;
                    replacement = string;
                    break;
                case CXCompletionChunk_ResultType:
                    resultType = string;
                    continue;
                case CXCompletionChunk_Placeholder:
                    //TODO:consider KTextEditor::TemplateInterface possibility
                    //replacement += "/*" + string + "*/";
                    if (signatureState == Inside) {
                        display += string;
                    }
                    continue;
                case CXCompletionChunk_LeftParen:
                    if (signatureState == Before && !parenDepth) {
                        signatureState = Inside;
                    }
                    parenDepth++;
                    break;
                case CXCompletionChunk_RightParen:
                    --parenDepth;
                    if (signatureState == Inside && !parenDepth) {
                        display += ')';
                        signatureState = After;
                    }
                    break;
                default:
                    break;
            }
            //replacement += string;
            if (signatureState == Inside) {
                display += string;
            }
        }

        if(typed.isEmpty()){
            continue;
        }

        // ellide text to the right for overly long result types (templates especially)
        elideStringRight(resultType, MAX_RETURN_TYPE_STRING_LENGTH);

        if (isDeclaration) {
            const Identifier id(typed);
            QualifiedIdentifier qid;
            ClangString parent(clang_getCompletionParent(result.CompletionString, nullptr));
            if (parent.c_str() != nullptr) {
                qid = QualifiedIdentifier(parent.toString());
            }
            qid.push(id);

            if (!isValidCompletionIdentifier(qid)) {
                continue;
            }

            auto found = findDeclaration(qid, ctx, m_position, declarationsCache, handled);

            CompletionTreeItemPointer item;
            if (found) {
                auto declarationItem = new DeclarationItem(found, display, resultType, replacement);

                const unsigned int completionPriority = clang_getCompletionPriority(result.CompletionString);
                const bool bestMatch = completionPriority <= MAX_PRIORITY_FOR_BEST_MATCHES;

                //don't set best match property for internal identifiers, also prefer declarations from current file
                if (bestMatch && !found->indexedIdentifier().identifier().toString().startsWith("__") ) {
                    const int matchQuality = codeCompletionPriorityToMatchQuality(completionPriority);
                    declarationItem->setMatchQuality(matchQuality);
                }

                item = declarationItem;
            } else {
                // still, let's trust that Clang found something useful and put it into the completion result list
                clangDebug() << "Could not find declaration for" << qid;
                item = CompletionTreeItemPointer(new SimpleItem(display, resultType, replacement));
            }

            if (isValidSpecialCompletionIdentifier(qid)) {
                // If it's a special completion identifier e.g. "operator=(const&)" and we don't have a declaration for it, don't add it into completion list, as this item is completely useless and pollutes the test case.
                // This happens e.g. for "class A{}; a.|".  At | we have "operator=(const A&)" as a special completion identifier without a declaration.
                if(item->declaration()){
                    specialItems.append(item);
                }
            } else {
                items.append(item);
            }
            continue;
        }

        if (result.CursorKind == CXCursor_MacroDefinition) {
            // TODO: grouping of macros and built-in stuff
            static const QIcon icon = QIcon::fromTheme("code-macro");
            auto item = CompletionTreeItemPointer(new SimpleItem(display, resultType, replacement, icon));
            macros.append(item);
        } else if (result.CursorKind == CXCursor_NotImplemented) {
            auto item = CompletionTreeItemPointer(new SimpleItem(display, resultType, replacement));
            builtin.append(item);
        }
    }

    if (abort) {
        return {};
    }

    addOverwritableItems();
    addImplementationHelperItems();
    eventuallyAddGroup(i18n("Special"), 700, specialItems);
    eventuallyAddGroup(i18n("Macros"), 900, macros);
    eventuallyAddGroup(i18n("Builtin"), 800, builtin);
    return items;
}

void ClangCodeCompletionContext::eventuallyAddGroup(const QString& name, int priority,
                                                    const QList<CompletionTreeItemPointer>& items)
{
    if (items.isEmpty()) {
        return;
    }

    KDevelop::CompletionCustomGroupNode* node = new KDevelop::CompletionCustomGroupNode(name, priority);
    node->appendChildren(items);
    m_ungrouped << CompletionTreeElementPointer(node);
}

void ClangCodeCompletionContext::addOverwritableItems()
{
    auto overrideList = m_completionHelper.overrides();
    if (overrideList.isEmpty()) {
        return;
    }

    QList<CompletionTreeItemPointer> overrides;
    for (int i = 0; i < overrideList.count(); i++) {
        FuncOverrideInfo info = overrideList.at(i);
        QString nameAndParams = info.name + '(' + info.params.join(", ") + ')';
        if(info.isConst)
            nameAndParams = nameAndParams + " const";
        if(info.isVirtual)
            nameAndParams = nameAndParams + " = 0";
        overrides << CompletionTreeItemPointer(new OverrideItem(nameAndParams, info.returnType));
    }
    eventuallyAddGroup(i18n("Virtual Override"), 0, overrides);
}

void ClangCodeCompletionContext::addImplementationHelperItems()
{
    auto implementsList = m_completionHelper.implements();
    if (implementsList.isEmpty()) {
        return;
    }

    QList<CompletionTreeItemPointer> implements;
    foreach(FuncImplementInfo info, implementsList) {
        implements << CompletionTreeItemPointer(new ImplementsItem(info));
    }
    eventuallyAddGroup(i18n("Implement Function"), 0, implements);
}


QList<CompletionTreeElementPointer> ClangCodeCompletionContext::ungroupedElements()
{
    return m_ungrouped;
}

ClangCodeCompletionContext::ContextFilters ClangCodeCompletionContext::filters() const
{
    return m_filters;
}

void ClangCodeCompletionContext::setFilters(const ClangCodeCompletionContext::ContextFilters& filters)
{
    m_filters = filters;
}
