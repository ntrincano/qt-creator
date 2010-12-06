/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "progressdata.h"
#include "rule.h"

#include <QtCore/QtGlobal>

using namespace TextEditor;
using namespace Internal;

ProgressData::ProgressData() :
    m_offset(0),
    m_savedOffset(-1),
    m_onlySpacesSoFar(true),
    m_openingBraceMatchAtFirstNonSpace(false),
    m_closingBraceMatchAtNonEnd(false),
    m_willContinueLine(false)
{}

ProgressData::~ProgressData()
{
    foreach (Rule *rule, m_trackedRules)
        rule->progressFinished();
}

void ProgressData::setOffset(const int offset)
{ m_offset = offset; }

int ProgressData::offset() const
{ return m_offset; }

void ProgressData::incrementOffset()
{ ++m_offset; }

void ProgressData::incrementOffset(const int increment)
{ m_offset += increment; }

void ProgressData::saveOffset()
{ m_savedOffset = m_offset; }

void ProgressData::restoreOffset()
{
    Q_ASSERT(m_savedOffset != -1);
    m_offset = m_savedOffset;
    m_savedOffset = -1;
}

void ProgressData::setOnlySpacesSoFar(const bool onlySpaces)
{ m_onlySpacesSoFar = onlySpaces; }

bool ProgressData::isOnlySpacesSoFar() const
{ return m_onlySpacesSoFar; }

void ProgressData::setOpeningBraceMatchAtFirstNonSpace(const bool match)
{ m_openingBraceMatchAtFirstNonSpace = match; }

bool ProgressData::isOpeningBraceMatchAtFirstNonSpace() const
{ return m_openingBraceMatchAtFirstNonSpace; }

void ProgressData::setClosingBraceMatchAtNonEnd(const bool match)
{ m_closingBraceMatchAtNonEnd = match; }

bool ProgressData::isClosingBraceMatchAtNonEnd() const
{ return m_closingBraceMatchAtNonEnd; }

void ProgressData::clearBracesMatches()
{
    if (m_openingBraceMatchAtFirstNonSpace)
        m_openingBraceMatchAtFirstNonSpace = false;
    if (m_closingBraceMatchAtNonEnd)
        m_closingBraceMatchAtNonEnd = false;
}

void ProgressData::setWillContinueLine(const bool willContinue)
{ m_willContinueLine = willContinue; }

bool ProgressData::isWillContinueLine() const
{ return m_willContinueLine; }

void ProgressData::setCaptures(const QStringList &captures)
{ m_captures = captures; }

const QStringList &ProgressData::captures() const
{ return m_captures; }

void ProgressData::trackRule(Rule *rule)
{
    m_trackedRules.append(rule);
}
