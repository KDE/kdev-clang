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

#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <QtTest>
#include <clang/AST/Stmt.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include "refactoringenvironment.h"
#include "declarationcomparator.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

RefactoringEnvironment::RefactoringEnvironment() = default;

RefactoringEnvironment::RefactoringEnvironment(const string &defaultFlags)
    : m_flags(defaultFlags)
{
}

void RefactoringEnvironment::addFile(const string &name, const string &code)
{
    addFile(name, code, m_flags);
}

void RefactoringEnvironment::addFile(const string &name, const string &code, const string &flags)
{
    m_files.emplace_back("/" + name, code, flags);
}

void RefactoringEnvironment::addHeader(const std::string &name, const std::string &code)
{
    m_headers.emplace_back("/" + name, code);
}

void RefactoringEnvironment::setDefaultFlags(const string &flags)
{
    m_flags = flags;
}

static string stripSpaces(const string &in)
{
    istringstream input(in);
    string result;
    while (!input.eof()) {
        string token;
        input >> token;
        result += " " + token;
    }
    return result;
}

void RefactoringEnvironment::findStmt(
    std::function<bool(const Stmt *)> comparator,
    std::function<void(const Stmt *, ASTContext *)> callback) const
{
    class Callback : public MatchFinder::MatchCallback
    {
    public:
        Callback(std::function<bool(const Stmt *)> comparator,
                 std::function<void(const Stmt *, ASTContext *)> callback)
            : m_comparator(comparator)
              , m_callback(callback)
        {
        }

        virtual void run(const MatchFinder::MatchResult &result) override
        {
            auto stmt = result.Nodes.getNodeAs<Stmt>("Stmt");
            if (m_comparator(stmt)) {
                astContext = result.Context;
                matches.push_back(stmt);
            }
        }

        virtual void onEndOfTranslationUnit() override
        {
            for (auto stmt : matches) {
                m_callback(stmt, astContext);
            }
            matches.clear();
        }

        vector<const Stmt *> matches;
        ASTContext *astContext;

    private:
        std::function<bool(const Stmt *)> m_comparator;
        std::function<void(const Stmt *, ASTContext *)> m_callback;
    } findCallback(comparator, callback);
    auto stmtMatcher = stmt().bind("Stmt");
    MatchFinder finder;
    finder.addMatcher(stmtMatcher, &findCallback);
    runTool(
        [&finder](RefactoringTool &tool)
        {
            tool.run(newFrontendActionFactory(&finder).get());
        });
}

void RefactoringEnvironment::setVerboseVerify(bool verbose)
{
    m_verboseVerify = verbose;
}

void RefactoringEnvironment::verifyResult(const Replacements &replacements, const string &fileName,
                                          const string &newCode)
{
    Replacements filteredReplacements;
    for (auto &r : replacements) {
        if (r.getFilePath() == "/" + fileName) {
            filteredReplacements.insert(r);
        }
    }
    string code;
    auto i = find_if(m_files.rbegin(), m_files.rend(),
                     [fileName](const tuple<string, string, string> &t)
                     {
                         return get<0>(t) == "/" + fileName;
                     });
    if (i != m_files.rend()) {
        code = get<1>(*i);
    } else {
        auto i = find_if(m_headers.rbegin(), m_headers.rend(),
                         [fileName](const tuple<string, string> &t)
                         {
                             return get<0>(t) == "/" + fileName;
                         });
        QVERIFY2(i != m_headers.rend(), (fileName + " was not registered in environment").c_str());
        code = get<1>(*i);
    }

    string result = applyAllReplacements(code, filteredReplacements);
    if (m_verboseVerify) {
        llvm::outs() << "Have:     " << stripSpaces(result) << "\n";
        llvm::outs() << "Expected: " << stripSpaces(newCode) << "\n";
    }
    QCOMPARE(stripSpaces(result), stripSpaces(newCode));
}

namespace
{
class FakeCompilationDatabase : public CompilationDatabase
{
public:
    FakeCompilationDatabase(const vector<tuple<string, string, string>> &files);
    virtual vector<CompileCommand> getAllCompileCommands() const override;
    virtual vector<string> getAllFiles() const override;
    virtual vector<CompileCommand> getCompileCommands(llvm::StringRef FilePath) const override;
private:
    vector<tuple<string, string, string>> m_files;
};
}

Replacements RefactoringEnvironment::runTool(function<void(RefactoringTool &)> runner) const
{
    FakeCompilationDatabase db(m_files);
    RefactoringTool tool(db, db.getAllFiles());
    for (const auto &t : m_files) {
        tool.mapVirtualFile(get<0>(t), get<1>(t));
    }
    for (const auto &t : m_headers) {
        tool.mapVirtualFile(get<0>(t), get<1>(t));
    }
    runner(tool);
    return tool.getReplacements();
}

FakeCompilationDatabase::FakeCompilationDatabase(const vector<tuple<string, string, string>> &files)
    : m_files(files)
{
}

static CompileCommand compileCommandForFile(const tuple<string, string, string> &file)
{
    CompileCommand cc;
    cc.Directory = ".";
    cc.CommandLine = {"/usr/bin/c++"};
    istringstream flags(get<2>(file));
    copy(istream_iterator<string>(flags), istream_iterator<string>(),
         back_inserter(cc.CommandLine));
    cc.CommandLine.push_back("-I/");
    cc.CommandLine.push_back(get<0>(file));
    return cc;
}

vector<CompileCommand> FakeCompilationDatabase::getCompileCommands(llvm::StringRef FilePath) const
{
    vector<CompileCommand> result;
    for (const auto &t : m_files) {
        if (get<0>(t) == FilePath) {
            result.push_back(compileCommandForFile(t));
        }
    }
    return result;
}

vector<string> FakeCompilationDatabase::getAllFiles() const
{
    vector<string> result;
    for (const auto &t : m_files) {
        result.emplace_back(get<0>(t));
    }
    return result;
}

vector<CompileCommand> FakeCompilationDatabase::getAllCompileCommands() const
{
    vector<CompileCommand> result;
    transform(m_files.begin(), m_files.end(), back_inserter(result), compileCommandForFile);
    return result;
}

namespace
{
class FakeDeclarationComparator : public DeclarationComparator
{
public:
    FakeDeclarationComparator(function<bool(const Decl *decl)> impl);

    virtual bool equivalentTo(const clang::Decl *decl) const override;

private:
    function<bool(const Decl *decl)> m_equivalentToImpl;
};
}

unique_ptr<DeclarationComparator> declarationComparator(
    function<bool(const Decl *decl)> equivalentTo)
{
    return unique_ptr<DeclarationComparator>(new FakeDeclarationComparator(equivalentTo));
}

FakeDeclarationComparator::FakeDeclarationComparator(function<bool(const Decl *decl)> impl)
    : m_equivalentToImpl(impl)
{
}

bool FakeDeclarationComparator::equivalentTo(const clang::Decl *decl) const
{
    return m_equivalentToImpl(decl);
}

string serialize(const Replacements &replacements)
{
    ostringstream result;
    for (auto &r : replacements) {
        result << r.getFilePath().str() << ' ' << r.getOffset() << ' ' << r.getLength() << ' ' <<
        r.getReplacementText().str() << '\n';
    }
    return result.str();
}