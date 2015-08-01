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

#ifndef TEST_PROBLEMS_H
#define TEST_PROBLEMS_H

#include <QObject>

#include <language/duchain/problem.h>

class TestProblems : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testNoProblems();
    void testBasicProblems();
    void testBasicRangeSupport();
    void testChildDiagnostics();
    void testRanges();

    void testFixits();
    void testFixits_data();
    void testTodoProblems();
    void testTodoProblems_data();

    void testMissingInclude();

private:
    QList<KDevelop::ProblemPointer> parse(const QByteArray& code);
};

#endif // TEST_PROBLEMS_H
