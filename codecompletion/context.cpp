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
#include <language/duchain/classmemberdeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/persistentsymboltable.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/pointertype.h>
#include <language/duchain/types/typealiastype.h>
#include <language/duchain/types/typeutils.h>
#include <language/interfaces/iastcontainer.h>
#include <language/codecompletion/codecompletionitem.h>
#include <language/codecompletion/codecompletionmodel.h>
#include <language/codecompletion/normaldeclarationcompletionitem.h>

#include "../util/clangdebug.h"
#include "../util/clangtypes.h"
#include "../duchain/parsesession.h"
#include "../duchain/navigationwidget.h"
#include "../clangsettings/clangsettingsmanager.h"

#include <memory>

#include <KTextEditor/Document>
#include <KTextEditor/View>

using namespace KDevelop;

namespace {
/// Maximum return-type string length in completion items
const int MAX_RETURN_TYPE_STRING_LENGTH = 20;

/// Priority of code-completion results. NOTE: Keep in sync with Clang code base.
enum CodeCompletionPriority {
  /// Priority for the next initialization in a constructor initializer list.
  CCP_NextInitializer = 7,
  /// Priority for an enumeration constant inside a switch whose condition is of the enumeration type.
  CCP_EnumInCase = 7,

  CCP_LocalDeclarationMatch = 8,

  CCP_DeclarationMatch = 12,

  CCP_LocalDeclarationSimiliar = 17,
  /// Priority for a send-to-super completion.
  CCP_SuperCompletion = 20,

  CCP_DeclarationSimiliar = 25,
  /// Priority for a declaration that is in the local scope.
  CCP_LocalDeclaration = 34,
  /// Priority for a member declaration found from the current method or member function.
  CCP_MemberDeclaration = 35,
  /// Priority for a language keyword (that isn't any of the other categories).
  CCP_Keyword = 40,
  /// Priority for a code pattern.
  CCP_CodePattern = 40,
  /// Priority for a non-type declaration.
  CCP_Declaration = 50,
  /// Priority for a type.
  CCP_Type = CCP_Declaration,
  /// Priority for a constant value (e.g., enumerator).
  CCP_Constant = 65,
  /// Priority for a preprocessor macro.
  CCP_Macro = 70,
  /// Priority for a nested-name-specifier.
  CCP_NestedNameSpecifier = 75,
  /// Priority for a result that isn't likely to be what the user wants, but is included for completeness.
  CCP_Unlikely = 80
};

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
                static const QIcon icon = QIcon::fromTheme(QStringLiteral("CTparents"));
                return icon;
            }
        }
        return CompletionItem<CompletionTreeItem>::data(index, role, model);
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        view->document()->replaceText(word, QLatin1String("virtual ") + m_returnType + QLatin1Char(' ') + m_display);
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
              i18n("Implement %1", item.isConstructor ? QStringLiteral("<constructor>") :
                                   item.isDestructor ? QStringLiteral("<destructor>") : item.returnType)
          )
        , m_item(item)
    {
    }

    QVariant data(const QModelIndex& index, int role, const CodeCompletionModel* model) const override
    {
        if (role == Qt::DecorationRole) {
            if (index.column() == KTextEditor::CodeCompletionModel::Icon) {
                static const QIcon icon = QIcon::fromTheme(QStringLiteral("CTsuppliers"));
                return icon;
            }
        }
        return CompletionItem<CompletionTreeItem>::data(index, role, model);
    }

    void execute(KTextEditor::View* view, const KTextEditor::Range& word) override
    {
        QString replacement = m_item.templatePrefix;
        if (!m_item.isDestructor && !m_item.isConstructor) {
            replacement += m_item.returnType + QLatin1Char(' ');
        }
        replacement += m_item.prototype + QLatin1String("\n{\n}\n");
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
            repl += QLatin1String("()");
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

    void setInheritanceDepth(int depth)
    {
        m_inheritanceDepth = depth;
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
        return str.replace(length, str.size() - length, QLatin1String("..."));
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

int adjustPriorityForType(const AbstractType::Ptr& type, int completionPriority)
{
    const auto modifier = 4;
    if (type) {
        const auto whichType = type->whichType();
        if (whichType == AbstractType::TypePointer || whichType == AbstractType::TypeReference) {
            // Clang considers all pointers as similar, this is not what we want.
            completionPriority += modifier;
        } else if (whichType == AbstractType::TypeStructure) {
            // Clang considers all classes as similar too...
            completionPriority += modifier;
        } else if (whichType == AbstractType::TypeDelayed) {
            completionPriority += modifier;
        } else if (whichType == AbstractType::TypeAlias) {
            auto aliasedType = type.cast<TypeAliasType>();
            return adjustPriorityForType(aliasedType ? aliasedType->type() : AbstractType::Ptr(), completionPriority);
        } else if (whichType == AbstractType::TypeFunction) {
            auto functionType = type.cast<FunctionType>();
            return adjustPriorityForType(functionType ? functionType->returnType() : AbstractType::Ptr(), completionPriority);
        }
    } else {
        completionPriority += modifier;
    }

    return completionPriority;
}

/// Adjusts priority for the @p decl
int adjustPriorityForDeclaration(Declaration* decl, unsigned int completionPriority)
{
    if(completionPriority < CCP_LocalDeclarationSimiliar || completionPriority > CCP_SuperCompletion){
        return completionPriority;
    }

    return adjustPriorityForType(decl->abstractType(), completionPriority);
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
    if (idString.startsWith(QLatin1Char('~')) && scope.toString() == idString.midRef(1)) {
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
    if (idString.startsWith(QLatin1String("operator="))) {
        return true; // is assignment operator
    }
    return false;
}

Declaration* findDeclaration(const QualifiedIdentifier& qid, const DUContextPointer& ctx, const CursorInRevision& position, QSet<Declaration*>& handled)
{
    PersistentSymbolTable::Declarations decl = PersistentSymbolTable::self().getDeclarations(qid);

    for (auto it = decl.iterator(); it; ++it) {
        auto declaration = it->declaration();
        if (declaration->kind() == Declaration::Instance) {
            break;
        }
        if (!handled.contains(declaration)) {
            handled.insert(declaration);
            return declaration;
        }
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

/// If any parent of this context is a class, the closest class declaration is returned, nullptr otherwise
Declaration* classDeclarationForContext(const DUContextPointer& context, const CursorInRevision& position)
{
    auto parent = context;
    while (parent) {
        if (parent->type() == DUContext::Class) {
            break;
        }

        if (auto owner = parent->owner()) {
            // Work-around for out-of-line methods. They have Helper context instead of Class context
            if (owner->context() && owner->context()->type() == DUContext::Helper) {
                auto qid = owner->qualifiedIdentifier();
                qid.pop();

                QSet<Declaration*> tmp;
                auto decl = findDeclaration(qid, context, position, tmp);

                if (decl && decl->internalContext() && decl->internalContext()->type() == DUContext::Class) {
                    parent = decl->internalContext();
                    break;
                }
            }
        }
        parent = parent->parentContext();
    }

    return parent ? parent->owner() : nullptr;
}

class LookAheadItemMatcher
{
public:
    LookAheadItemMatcher(const TopDUContextPointer& ctx)
        : m_topContext(ctx)
        , m_enabled(ClangSettingsManager::self()->codeCompletionSettings().lookAhead)
    {}

    /// Adds all local declarations for @p declaration into possible look-ahead items.
    void addDeclarations(Declaration* declaration)
    {
        if (!m_enabled) {
            return;
        }

        if (declaration->kind() != Declaration::Instance) {
            return;
        }

        auto type = typeForDeclaration(declaration);
        auto identifiedType = dynamic_cast<const IdentifiedType*>(type.data());
        if (!identifiedType) {
            return;
        }

        addDeclarationsForType(identifiedType, declaration);
    }

    /// Add type for matching. This type'll be used for filtering look-ahead items
    /// Only items with @p type will be returned through @sa matchedItems
    void addMatchedType(const IndexedType& type)
    {
        matchedTypes.insert(type);
    }

    /// @return look-ahead items that math given types. @sa addMatchedType
    QList<CompletionTreeItemPointer> matchedItems()
    {
        QList<CompletionTreeItemPointer> lookAheadItems;
        for (const auto& pair: possibleLookAheadDeclarations) {
            auto decl = pair.first;
            if (matchedTypes.contains(decl->indexedType())) {
                auto parent = pair.second;
                const QString access = parent->abstractType()->whichType() == AbstractType::TypePointer
                                 ? QStringLiteral("->") : QStringLiteral(".");
                const QString text = parent->identifier().toString() + access + decl->identifier().toString();
                auto item = new DeclarationItem(decl, text, {}, text);
                item->setMatchQuality(8);
                lookAheadItems.append(CompletionTreeItemPointer(item));
            }
        }

        return lookAheadItems;
    }

private:
    AbstractType::Ptr typeForDeclaration(const Declaration* decl)
    {
        return TypeUtils::targetType(decl->abstractType(), m_topContext.data());
    }

    void addDeclarationsForType(const IdentifiedType* identifiedType, Declaration* declaration)
    {
        if (auto typeDecl = identifiedType->declaration(m_topContext.data())) {
            if (dynamic_cast<ClassDeclaration*>(typeDecl->logicalDeclaration(m_topContext.data()))) {
                if (!typeDecl->internalContext()) {
                    return;
                }

                for (auto localDecl : typeDecl->internalContext()->localDeclarations()) {
                    if(localDecl->identifier().isEmpty()){
                        continue;
                    }

                    if(auto classMember = dynamic_cast<ClassMemberDeclaration*>(localDecl)){
                        // TODO: Also add protected/private members if completion is inside this class context.
                        if(classMember->accessPolicy() != Declaration::Public){
                            continue;
                        }
                    }

                    if(!declaration->abstractType()){
                        continue;
                    }

                    if (declaration->abstractType()->whichType() == AbstractType::TypeIntegral) {
                        if (auto integralType = declaration->abstractType().cast<IntegralType>()) {
                            if (integralType->dataType() == IntegralType::TypeVoid) {
                                continue;
                            }
                        }
                    }

                    possibleLookAheadDeclarations.insert({localDecl, declaration});
                }
            }
        }
    }

    // Declaration and it's context
    typedef QPair<Declaration*, Declaration*> DeclarationContext;

    /// Types of declarations that look-ahead completion items can have
    QSet<IndexedType> matchedTypes;

    // List of declarations that can be added to the Look Ahead group
    // Second declaration represents context
    QSet<DeclarationContext> possibleLookAheadDeclarations;

    TopDUContextPointer m_topContext;

    bool m_enabled;
};

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
            return;
        }

        auto addMacros = ClangSettingsManager::self()->codeCompletionSettings().macros;
        if (!addMacros) {
            m_filters |= NoMacros;
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

    /// Normal completion items, such as 'void Foo::foo()'
    QList<CompletionTreeItemPointer> items;
    /// Stuff like 'Foo& Foo::operator=(const Foo&)', etc. Not regularly used by our users.
    QList<CompletionTreeItemPointer> specialItems;
    /// Macros from the current context
    QList<CompletionTreeItemPointer> macros;
    /// Builtins reported by Clang
    QList<CompletionTreeItemPointer> builtin;

    QSet<Declaration*> handled;

    LookAheadItemMatcher lookAheadMatcher(TopDUContextPointer(ctx->topContext()));

    clangDebug() << "Clang found" << m_results->NumResults << "completion results";

    for (uint i = 0; i < m_results->NumResults; ++i) {
        if (abort) {
            return {};
        }

        auto result = m_results->Results[i];

        const auto availability = clang_getCompletionAvailability(result.CompletionString);
        if (availability == CXAvailability_NotAvailable) {
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

        // If ctx is/inside the Class context, this represents that context.
        auto currentClassContext = classDeclarationForContext(ctx, m_position);
        if (availability == CXAvailability_NotAccessible && (!isDeclaration || !currentClassContext)) {
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
                        display += QLatin1Char(')');
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

            auto found = findDeclaration(qid, ctx, m_position, handled);

            CompletionTreeItemPointer item;
            if (found) {
                // TODO: Bug in Clang: protected members from base classes not accessible in derived classes.
                if (availability == CXAvailability_NotAccessible) {
                    if (auto cl = dynamic_cast<ClassMemberDeclaration*>(found)) {
                        if (cl->accessPolicy() != Declaration::Protected) {
                            continue;
                        }

                        auto declarationClassContext = classDeclarationForContext(DUContextPointer(found->context()), m_position);

                        uint steps = 10;
                        auto inheriters = DUChainUtils::getInheriters(declarationClassContext, steps);
                        if(!inheriters.contains(currentClassContext)){
                            continue;
                        }
                    } else {
                        continue;
                    }
                }

                auto declarationItem = new DeclarationItem(found, display, resultType, replacement);

                const unsigned int completionPriority = adjustPriorityForDeclaration(found, clang_getCompletionPriority(result.CompletionString));
                const bool bestMatch = completionPriority <= CCP_SuperCompletion;

                //don't set best match property for internal identifiers, also prefer declarations from current file
                if (bestMatch && !found->indexedIdentifier().identifier().toString().startsWith(QLatin1String("__")) ) {
                    const int matchQuality = codeCompletionPriorityToMatchQuality(completionPriority);
                    declarationItem->setMatchQuality(matchQuality);

                    // TODO: LibClang missing API to determine expected code completion type.
                    lookAheadMatcher.addMatchedType(found->indexedType());
                } else {
                    declarationItem->setInheritanceDepth(completionPriority);

                    lookAheadMatcher.addDeclarations(found);
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
            static const QIcon icon = QIcon::fromTheme(QStringLiteral("code-macro"));
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

    addImplementationHelperItems();
    addOverwritableItems();

    eventuallyAddGroup(i18n("Special"), 700, specialItems);
    eventuallyAddGroup(i18n("Look-ahead Matches"), 800, lookAheadMatcher.matchedItems());
    eventuallyAddGroup(i18n("Builtin"), 900, builtin);
    eventuallyAddGroup(i18n("Macros"), 1000, macros);
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
        QString nameAndParams = info.name + QLatin1Char('(') + info.params.join(QLatin1String(", ")) + QLatin1Char(')');
        if(info.isConst)
            nameAndParams = nameAndParams + QLatin1String(" const");
        if(info.isVirtual)
            nameAndParams = nameAndParams + QLatin1String(" = 0");
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
    foreach(const auto& info, implementsList) {
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
