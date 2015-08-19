/*************************************************************************************
 *  Copyright (C) Kevin Funk <kfunk@kde.org>                                         *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/

#include "test_problems.h"

#include "../duchain/clangindex.h"
#include "../duchain/clangproblem.h"
#include "../duchain/parsesession.h"
#include "../duchain/unknowndeclarationproblem.h"
#include "../util/clangtypes.h"

#include <language/duchain/duchain.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchainlock.h>
#include <language/codegen/coderepresentation.h>
#include <language/backgroundparser/backgroundparser.h>

#include <tests/autotestshell.h>
#include <tests/testcore.h>
#include <tests/testfile.h>
#include <tests/testhelpers.h>
#include <tests/testproject.h>
#include <interfaces/ilanguagecontroller.h>

#include <QtTest/QTest>
#include <QLoggingCategory>


using namespace KDevelop;

namespace {

const QString FileName = QStringLiteral("/tmp/stdin.cpp");

}

QTEST_GUILESS_MAIN(TestProblems)

void TestProblems::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\ndefault.debug=true\nkdevelop.plugins.clang.debug=true\n"));
    QVERIFY(qputenv("KDEV_DISABLE_PLUGINS", "kdevcppsupport"));
    AutoTestShell::init();
    TestCore::initialize(Core::NoUi);
    DUChain::self()->disablePersistentStorage();
    Core::self()->languageController()->backgroundParser()->setDelay(0);
    CodeRepresentation::setDiskChangesForbidden(true);
}

void TestProblems::cleanupTestCase()
{
    TestCore::shutdown();
}

QList<ProblemPointer> TestProblems::parse(const QByteArray& code)
{
    ClangIndex index;
    ClangParsingEnvironment environment;
    environment.setTranslationUnitUrl(IndexedString(FileName));
    ParseSession session(ParseSessionData::Ptr(new ParseSessionData({UnsavedFile(FileName, {code})},
                                                                    &index, environment)));
    return session.problemsForFile(session.mainFile());
}

void TestProblems::testNoProblems()
{
    const QByteArray code = "int main() {}";
    auto problems = parse(code);
    QCOMPARE(problems.size(), 0);
}

void TestProblems::testBasicProblems()
{
    // expected:
    // <stdin>:1:13: error: expected ';' after class
    // class Foo {}
    //             ^
    //             ;
    const QByteArray code = "class Foo {}";
    auto problems = parse(code);
    QCOMPARE(problems.size(), 1);
    QCOMPARE(problems[0]->diagnostics().size(), 0);
    auto range = problems[0]->rangeInCurrentRevision();
    QCOMPARE(range.start(), KTextEditor::Cursor(0, 12));
    QCOMPARE(range.end(), KTextEditor::Cursor(0, 12));
}

void TestProblems::testBasicRangeSupport()
{
    // expected:
    // <stdin>:1:17: warning: expression result unused [-Wunused-value]
    // int main() { (1 + 1); }
    //               ~ ^ ~
    const QByteArray code = "int main() { (1 + 1); }";
    auto problems = parse(code);
    QCOMPARE(problems.size(), 1);
    QCOMPARE(problems[0]->diagnostics().size(), 0);
    auto range = problems[0]->rangeInCurrentRevision();
    QCOMPARE(range.start(), KTextEditor::Cursor(0, 14));
    QCOMPARE(range.end(), KTextEditor::Cursor(0, 19));
}

void TestProblems::testChildDiagnostics()
{
    // expected:
    // test.cpp:3:14: error: call to 'foo' is ambiguous
    // int main() { foo(0); }
    //              ^~~
    // test.cpp:1:6: note: candidate function
    // void foo(unsigned int);
    //      ^
    // test.cpp:2:6: note: candidate function
    // void foo(const char*);
    //      ^
    const QByteArray code = "void foo(unsigned int);\n"
                            "void foo(const char*);\n"
                            "int main() { foo(0); }";
    auto problems = parse(code);
    QCOMPARE(problems.size(), 1);
    auto range = problems[0]->rangeInCurrentRevision();
    QCOMPARE(range.start(), KTextEditor::Cursor(2, 13));
    QCOMPARE(range.end(), KTextEditor::Cursor(2, 16));
    QCOMPARE(problems[0]->diagnostics().size(), 2);
    IProblem::Ptr p1 = problems[0]->diagnostics()[0];
    const ProblemPointer d1 = ProblemPointer(dynamic_cast<Problem*>(p1.data()));
    QCOMPARE(d1->url().str(), FileName);
    QCOMPARE(d1->rangeInCurrentRevision(), KTextEditor::Range(0, 5, 0, 8));
    IProblem::Ptr p2 = problems[0]->diagnostics()[1];
    const ProblemPointer d2 = ProblemPointer(dynamic_cast<Problem*>(p2.data()));
    QCOMPARE(d2->url().str(), FileName);
    QCOMPARE(d2->rangeInCurrentRevision(), KTextEditor::Range(1, 5, 1, 8));
}

Q_DECLARE_METATYPE(QVector<ClangFixit>);

/**
 * Provides a list of possible fixits: http://blog.llvm.org/2010/04/amazing-feats-of-clang-error-recovery.html
 */
void TestProblems::testFixits()
{
    QFETCH(QString, code);
    QFETCH(int, problemsCount);
    QFETCH(QVector<ClangFixit>, fixits);

    auto problems = parse(code.toLatin1());

    qDebug() << problems.last()->description();
    QCOMPARE(problems.size(), problemsCount);

    const ClangProblem* p1 = dynamic_cast<ClangProblem*>(problems[0].data());
    QVERIFY(p1);
    ClangFixitAssistant* a1 = qobject_cast<ClangFixitAssistant*>(p1->solutionAssistant().data());
    QVERIFY(a1);

    QCOMPARE(p1->allFixits(), fixits);
}

void TestProblems::testFixits_data()
{
    QTest::addColumn<QString>("code"); // input
    QTest::addColumn<int>("problemsCount");
    QTest::addColumn<QVector<ClangFixit>>("fixits");

    // expected:
    // test -Wextra-tokens
    // /home/krf/test.cpp:2:8: warning: extra tokens at end of #endif directive [-Wextra-tokens]
    // #endif FOO
    //        ^
    //        //
    QTest::newRow("extra-tokens test")
        << "#ifdef FOO\n#endif FOO\n"
        << 1
        << QVector<ClangFixit>{ ClangFixit{"//", DocumentRange(IndexedString(FileName), KTextEditor::Range(1, 7, 1, 7)), QString()} };

    // expected:
    // test.cpp:1:19: warning: empty parentheses interpreted as a function declaration [-Wvexing-parse]
    //     int a();
    //          ^~
    // test.cpp:1:19: note: replace parentheses with an initializer to declare a variable
    //     int a();
    //          ^~
    //           = 0
    QTest::newRow("vexing-parse test")
        << "int main() { int a(); }\n"
        << 1
        << QVector<ClangFixit>{ ClangFixit{" = 0", DocumentRange(IndexedString(FileName), KTextEditor::Range(0, 18, 0, 20)), QString()} };

    // expected:
    // test.cpp:2:21: error: no member named 'someVariablf' in 'C'; did you mean 'someVariable'?
    // int main() { C c; c.someVariablf = 1; }
    //                     ^~~~~~~~~~~~
    //                     someVariable
    QTest::newRow("spell-check test")
        << "class C{ int someVariable; };\n"
           "int main() { C c; c.someVariablf = 1; }\n"
        << 1
        << QVector<ClangFixit>{ ClangFixit{"someVariable", DocumentRange(IndexedString(FileName), KTextEditor::Range(1, 20, 1, 32)), QString()} };
}

void TestProblems::testMissingInclude()
{
    TestFile include("class A {};\n", "h");
    include.parse(TopDUContext::AllDeclarationsAndContexts);

    TestFile workingFile("int main() { A a; }", "cpp");
    workingFile.parse(TopDUContext::AllDeclarationsAndContexts);

    QCOMPARE(include.url().toUrl().adjusted(QUrl::RemoveFilename), workingFile.url().toUrl().adjusted(QUrl::RemoveFilename));
    QVERIFY(include.waitForParsed());
    QVERIFY(workingFile.waitForParsed());

    DUChainReadLocker lock;

    QVERIFY(include.topContext());
    TopDUContext* includeTop = DUChainUtils::contentContextFromProxyContext(include.topContext().data());
    QVERIFY(includeTop);

    QVERIFY(workingFile.topContext());
    TopDUContext* top = DUChainUtils::contentContextFromProxyContext(workingFile.topContext());
    QVERIFY(top);
    QCOMPARE(top->problems().size(), 1);

    auto problem = dynamic_cast<UnknownDeclarationProblem*>(top->problems().first().data());
    auto assistant = problem->solutionAssistant();
    auto clangFixitAssistant = qobject_cast<ClangFixitAssistant*>(assistant.data());
    QVERIFY(clangFixitAssistant);

    auto fixits = clangFixitAssistant->fixits();
    QCOMPARE(fixits.size(), 3);
    QCOMPARE(fixits[0].replacementText, QString("class A;\n"));
    QCOMPARE(fixits[1].replacementText, QString("struct A;\n")); // TODO: We shouldn't show this
    QCOMPARE(fixits[2].replacementText, QString("#include \"%1\"\n").arg(include.url().toUrl().fileName()));
}

struct ExpectedTodo
{
    QString description;
    KTextEditor::Cursor start;
    KTextEditor::Cursor end;
};
typedef QVector<ExpectedTodo> ExpectedTodos;
Q_DECLARE_METATYPE(ExpectedTodos)

void TestProblems::testTodoProblems()
{
    QFETCH(QString, code);
    QFETCH(ExpectedTodos, expectedTodos);

    TestFile file(code, "cpp");
    QVERIFY(file.parseAndWait());

    DUChainReadLocker lock;
    auto top = file.topContext();
    QVERIFY(top);
    auto problems = top->problems();
    QCOMPARE(problems.size(), expectedTodos.size());

    for (int i = 0; i < problems.size(); ++i) {
        auto problem = problems[i];
        auto expectedTodo = expectedTodos[i];
        QCOMPARE(problem->description(), expectedTodo.description);
        QCOMPARE(problem->finalLocation().start(), expectedTodo.start);
        QCOMPARE(problem->finalLocation().end(), expectedTodo.end);
    }
}

void TestProblems::testTodoProblems_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<ExpectedTodos>("expectedTodos");

    // we have two problems here:
    // - we cannot search for comments without declarations,
    //   that means: we can only search inside doxygen-style comments
    //   possible fix: -fparse-all-comments -- however: libclang API is lacking here again.
    //   Can only search through comments attached to a valid entity in the AST
    // - we cannot detect the correct location of the comment yet
    // see more comments inside TodoExtractor
    QTest::newRow("simple1")
        << "/** TODO: Something */\n/** notodo */\n"
        << ExpectedTodos{{"TODO: Something", {0, 4}, {0, 19}}};
    QTest::newRow("simple2")
        << "/// FIXME: Something\n"
        << ExpectedTodos{{"FIXME: Something", {0, 4}, {0, 20}}};
    QTest::newRow("mixed-content")
        << "/// FIXME: Something\n///Uninteresting content\n"
        << ExpectedTodos{{"FIXME: Something", {0, 4}, {0, 20}}};
    QTest::newRow("multi-line1")
        << "/**\n* foo\n*\n* FIXME: Something\n*/\n"
        << ExpectedTodos{{"FIXME: Something", {3, 2}, {3, 18}}};
    QTest::newRow("multi-line2")
        << "/// FIXME: Something\n///Uninteresting content\n"
        << ExpectedTodos{{"FIXME: Something", {0, 4}, {0, 20}}};
    QTest::newRow("multiple-todos-line2")
        << "/**\n* FIXME: one\n*foo bar\n* FIXME: two */\n"
        << ExpectedTodos{
            {"FIXME: one", {1, 2}, {1, 12}},
            {"FIXME: two", {3, 2}, {3, 12}}
        };
    QTest::newRow("todo-later-in-the-document")
        << "///foo\n\n///FIXME: bar\n"
        << ExpectedTodos{{"FIXME: bar", {2, 3}, {2, 13}}};
    QTest::newRow("non-ascii-todo")
        << "/* TODO: 例えば */"
        << ExpectedTodos{{"TODO: 例えば", {0, 3}, {0, 12}}};
}

void TestProblems::testProblemsForIncludedFiles()
{
    TestFile header("#pragma once\n//TODO: header\n", "h");
    TestFile file("#include \"" + header.url().byteArray() + "\"\n//TODO: source\n", "cpp");

    file.parse(TopDUContext::Features(TopDUContext::AllDeclarationsContextsAndUses|TopDUContext::AST | TopDUContext::ForceUpdate));
    QVERIFY(file.waitForParsed(5000));

    {
        DUChainReadLocker lock;
        QVERIFY(file.topContext());

        auto context = DUChain::self()->chainForDocument(file.url());
        QVERIFY(context);
        QCOMPARE(context->problems().size(), 1);
        QCOMPARE(context->problems()[0]->description(), QStringLiteral("TODO: source"));
        QCOMPARE(context->problems()[0]->finalLocation().document, file.url());

        context = DUChain::self()->chainForDocument(header.url());
        QVERIFY(context);
        QCOMPARE(context->problems().size(), 1);
        QCOMPARE(context->problems()[0]->description(), QStringLiteral("TODO: header"));
        QCOMPARE(context->problems()[0]->finalLocation().document, header.url());
    }
}

using RangeList = QVector<KTextEditor::Range>;

void TestProblems::testRanges_data()
{
    QTest::addColumn<QByteArray>("code");
    QTest::addColumn<RangeList>("ranges");

    {
        // expected:
        // test.cpp:4:1: error: C++ requires a type specifier for all declarations
        // operator[](int){return string;}
        // ^
        //
        // test.cpp:4:24: error: 'string' does not refer to a value
        // operator[](int){return string;}
        //                        ^
        const QByteArray code = "struct string{};\nclass Test{\npublic:\noperator[](int){return string;}\n};";
        QTest::newRow("operator") << code << RangeList{{3, 0, 3, 8}, {3, 23, 3, 29}};
    }
    {
        const QByteArray code = "#include \"/some/file/that/does/not/exist.h\"\nint main() { return 0; }";
        QTest::newRow("badInclude") << code << RangeList{{0, 9, 0, 43}};
    }
    {
        const QByteArray code = "int main() const\n{ return 0; }";
        QTest::newRow("badConst") << code << RangeList{{0, 11, 0, 16}};
    }
}

void TestProblems::testRanges()
{
    QFETCH(QByteArray, code);
    QFETCH(RangeList, ranges);

    const auto problems = parse(code);
    RangeList actualRanges;
    foreach (auto problem, parse(code)) {
        actualRanges << problem->rangeInCurrentRevision();
    }
    qDebug() << actualRanges << ranges;
    QCOMPARE(actualRanges, ranges);
}
