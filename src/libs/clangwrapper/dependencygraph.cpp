/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "dependencygraph.h"

#include <QtCore/QtConcurrentRun>

using namespace Clang;
using namespace Internal;

DependencyGraph::DependencyGraph()
{
    m_includeTracker.setResolutionMode(IncludeTracker::EveryMatchResolution);
}

DependencyGraph::~DependencyGraph()
{
    discard();
}

void DependencyGraph::cancel()
{
    if (m_computeWatcher.isRunning()) {
        m_computeWatcher.cancel();
        m_computeWatcher.waitForFinished();
    }
}

void DependencyGraph::addFile(const QString &fileName, const QStringList &compilationOptions)
{
    cancel();

    m_files.append(qMakePair(fileName, compilationOptions));
}

QFuture<void> DependencyGraph::compute()
{
    QFuture<void> future = QtConcurrent::run(this, &DependencyGraph::computeCore);
    m_computeWatcher.setFuture(future);
    return future;
}

void DependencyGraph::computeCore()
{
    for (int i = 0; i < m_files.size(); ++i) {
        if (m_computeWatcher.isCanceled())
            break;

        const QPair<QString, QStringList> &p = m_files.at(i);
        const QString &currentFile = p.first;
        const QStringList &options = p.second;
        const QPair<bool, NodeRefSetIt> &v = findVertex(currentFile);
        if (!v.first)
            processIncludes(insertVertex(currentFile), options);
    }

    emit dependencyGraphAvailable();
}

void DependencyGraph::processIncludes(NodeRefSetIt currentIt,
                                      const QStringList &compilationOptions)
{
    const QString &currentFile = currentIt.key();
    const QStringList &includes = m_includeTracker.directIncludes(currentFile, compilationOptions);
    foreach (const QString &include, includes) {
        if (m_computeWatcher.isCanceled())
            return;

        QPair<bool, NodeRefSetIt> v = findVertex(include);
        if (!v.first) {
            v.second = insertVertex(include);
            processIncludes(v.second, compilationOptions);
        }
        insertEdge(currentIt, v.second);
    }
}

namespace {

struct SimpleVisitor
{
    bool acceptFile(const QString &fileName)
    {
        m_allFiles.append(fileName);
        return false;
    }

    QStringList m_allFiles;
};

}

QStringList DependencyGraph::collectDependencies(const QString &referenceFile,
                                                 DependencyRole role) const
{
    SimpleVisitor visitor;
    collectDependencies(referenceFile, role, &visitor);

    return visitor.m_allFiles;
}

bool DependencyGraph::hasDependency(const QString &referenceFile, DependencyRole role) const
{
    QPair<bool, NodeRefSetIt> v = findVertex(referenceFile);
    if (!v.first)
        return false;

    NodeListIt nodeIt = v.second.value();

    if (role == FilesDirectlyIncludedBy || role == FilesIncludedBy)
        return nodeIt->m_out != 0;

    return nodeIt->m_in != 0;
}

void DependencyGraph::discard()
{
    cancel();

    for (NodeListIt it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        deleteAdjacencies(it->m_out);
        deleteAdjacencies(it->m_in);
    }
    m_nodes.clear();
    m_nodesRefs.clear();
    m_files.clear();
}


DependencyGraph::Node::Node(const QString &fileName)
    : m_fileName(fileName)
    , m_out(0)
    , m_in(0)
{}

DependencyGraph::AdjacencyNode::AdjacencyNode(NodeListIt it)
    : m_next(0)
    , m_nodeIt(it)
{}

QPair<bool, DependencyGraph::NodeRefSetIt> DependencyGraph::findVertex(const QString &s) const
{
    bool found = false;
    NodeRefSetIt it = const_cast<NodeRefSet &>(m_nodesRefs).find(s);
    if (it != m_nodesRefs.end())
        found = true;
    return qMakePair(found, it);
}

DependencyGraph::NodeRefSetIt DependencyGraph::insertVertex(const QString &s)
{
    Q_ASSERT(m_nodesRefs.find(s) == m_nodesRefs.end());

    m_nodes.append(Node(s));
    return m_nodesRefs.insert(s, m_nodes.end() - 1);
}

void DependencyGraph::insertEdge(DependencyGraph::NodeRefSetIt fromIt,
                                 DependencyGraph::NodeRefSetIt toIt)
{
    NodeListIt nodeFromIt = fromIt.value();
    NodeListIt nodeToIt = toIt.value();

    createAdjacency(&nodeFromIt->m_out, new AdjacencyNode(nodeToIt));
    createAdjacency(&nodeToIt->m_in, new AdjacencyNode(nodeFromIt));
}

void DependencyGraph::deleteAdjacencies(AdjacencyNode *node)
{
    while (node) {
        AdjacencyNode *next = node->m_next;
        delete node;
        node = next;
    }
}

void DependencyGraph::createAdjacency(AdjacencyNode **node, AdjacencyNode *newNode)
{
    if (*node)
        newNode->m_next = *node;
    *node = newNode;
}
