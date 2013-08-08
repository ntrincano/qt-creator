/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "qmlitemnode.h"
#include <metainfo.h>
#include "qmlchangeset.h"
#include "nodelistproperty.h"
#include "variantproperty.h"
#include "qmlanchors.h"
#include "invalidmodelnodeexception.h"
#include "itemlibraryinfo.h"

#include "plaintexteditmodifier.h"
#include "rewriterview.h"
#include "modelmerger.h"
#include "rewritingexception.h"

#include <QMessageBox>
#include <QUrl>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <QDir>

namespace QmlDesigner {

bool QmlItemNode::isItemOrWindow(const ModelNode &modelNode)
{
    if (modelNode.metaInfo().isSubclassOf("QtQuick.Item", -1, -1))
        return true;

    if (modelNode.metaInfo().isSubclassOf("QtQuick.Window.Window", -1, -1) && modelNode.isRootNode())
        return true;

    return false;
}

static QmlItemNode createQmlItemNodeFromSource(AbstractView *view, const QString &source, const QPointF &position)
{
    QScopedPointer<Model> inputModel(Model::create("QtQuick.Item", 1, 0, view->model()));
    inputModel->setFileUrl(view->model()->fileUrl());
    QPlainTextEdit textEdit;

    textEdit.setPlainText(source);
    NotIndentingTextEditModifier modifier(&textEdit);

    QScopedPointer<RewriterView> rewriterView(new RewriterView(RewriterView::Amend, 0));
    rewriterView->setCheckSemanticErrors(false);
    rewriterView->setTextModifier(&modifier);
    inputModel->setRewriterView(rewriterView.data());

    if (rewriterView->errors().isEmpty() && rewriterView->rootModelNode().isValid()) {
        ModelNode rootModelNode = rewriterView->rootModelNode();
        inputModel->detachView(rewriterView.data());

        rootModelNode.variantProperty("x").setValue(qRound(position.x()));
        rootModelNode.variantProperty("y").setValue(qRound(position.y()));

        ModelMerger merger(view);
        return merger.insertModel(rootModelNode);
    }

    return QmlItemNode();
}


QmlItemNode QmlItemNode::createQmlItemNode(AbstractView *view, const ItemLibraryEntry &itemLibraryEntry, const QPointF &position, QmlItemNode parentQmlItemNode)
{
    if (!parentQmlItemNode.isValid())
        parentQmlItemNode = QmlItemNode(view->rootModelNode());

    Q_ASSERT(parentQmlItemNode.isValid());

    QmlItemNode newQmlItemNode;

    try {
        RewriterTransaction transaction = view->beginRewriterTransaction();

        NodeMetaInfo metaInfo = view->model()->metaInfo(itemLibraryEntry.typeName());

        int minorVersion = metaInfo.minorVersion();
        int majorVersion = metaInfo.majorVersion();

        if (itemLibraryEntry.typeName().contains('.')) {

            const QString newImportUrl = itemLibraryEntry.requiredImport();

            if (!itemLibraryEntry.requiredImport().isEmpty()) {
                const QString newImportVersion = QString("%1.%2").arg(QString::number(itemLibraryEntry.majorVersion()), QString::number(itemLibraryEntry.minorVersion()));

                Import newImport = Import::createLibraryImport(newImportUrl, newImportVersion);
                if (itemLibraryEntry.majorVersion() == -1 && itemLibraryEntry.minorVersion() == -1)
                    newImport = Import::createFileImport(newImportUrl, QString());
                else
                    newImport = Import::createLibraryImport(newImportUrl, newImportVersion);

                foreach (const Import &import, view->model()->imports()) {
                    if (import.isLibraryImport()
                            && import.url() == newImport.url()
                            && import.version() == newImport.version()) {
                        // reuse this import
                        newImport = import;
                        break;
                    }
                }

                if (!view->model()->hasImport(newImport, true, true))
                    view->model()->changeImports(QList<Import>() << newImport, QList<Import>());
            }
        }

        if (itemLibraryEntry.qmlSource().isEmpty()) {
            QList<QPair<PropertyName, QVariant> > propertyPairList;
            propertyPairList.append(qMakePair(PropertyName("x"), QVariant(qRound(position.x()))));
            propertyPairList.append(qMakePair(PropertyName("y"), QVariant(qRound(position.y()))));

            foreach (const PropertyContainer &property, itemLibraryEntry.properties())
                propertyPairList.append(qMakePair(property.name(), property.value()));

            newQmlItemNode = QmlItemNode(view->createModelNode(itemLibraryEntry.typeName(), majorVersion, minorVersion, propertyPairList));
        } else {
            newQmlItemNode = createQmlItemNodeFromSource(view, itemLibraryEntry.qmlSource(), position);
        }

        if (parentQmlItemNode.hasDefaultPropertyName())
            parentQmlItemNode.nodeAbstractProperty(parentQmlItemNode.defaultPropertyName()).reparentHere(newQmlItemNode);

        if (!newQmlItemNode.isValid())
            return newQmlItemNode;

        newQmlItemNode.setId(view->generateNewId("image"));

        if (!view->currentState().isBaseState()) {
            newQmlItemNode.modelNode().variantProperty("opacity").setValue(0);
            newQmlItemNode.setVariantProperty("opacity", 1);
        }

        Q_ASSERT(newQmlItemNode.isValid());
    }
    catch (RewritingException &e) {
        QMessageBox::warning(0, "Error", e.description());
    }

    Q_ASSERT(newQmlItemNode.isValid());

    return newQmlItemNode;
}

static void checkImageImport(AbstractView *view)
{
    const QString newImportUrl = QLatin1String("QtQuick");
    const QString newImportVersion = QLatin1String("1.1");
    Import newImport = Import::createLibraryImport(newImportUrl, newImportVersion);

    foreach (const Import &import, view->model()->imports()) {
        if (import.isLibraryImport()
            && import.url() == newImport.url()) {
            // reuse this import
            newImport = import;
            break;
        }
    }

    if (!view->model()->hasImport(newImport, true, true))
        view->model()->changeImports(QList<Import>() << newImport, QList<Import>());
}

QmlItemNode QmlItemNode::createQmlItemNodeFromImage(AbstractView *view, const QString &imageName, const QPointF &position, QmlItemNode parentQmlItemNode)
{
    QmlItemNode newQmlItemNode;
    if (!parentQmlItemNode.isValid())
        parentQmlItemNode = QmlItemNode(view->rootModelNode());

    if (parentQmlItemNode.isValid()) {
        RewriterTransaction transaction = view->beginRewriterTransaction();

        checkImageImport(view);

        if (view->model()->hasNodeMetaInfo("QtQuick.Image")) {
            NodeMetaInfo metaInfo = view->model()->metaInfo("QtQuick.Image");
            QList<QPair<PropertyName, QVariant> > propertyPairList;
            propertyPairList.append(qMakePair(PropertyName("x"), QVariant(qRound(position.x()))));
            propertyPairList.append(qMakePair(PropertyName("y"), QVariant(qRound(position.y()))));

            QString relativeImageName = imageName;

            //use relative path
            if (QFileInfo(view->model()->fileUrl().toLocalFile()).exists()) {
                QDir fileDir(QFileInfo(view->model()->fileUrl().toLocalFile()).absolutePath());
                relativeImageName = fileDir.relativeFilePath(imageName);
                propertyPairList.append(qMakePair(PropertyName("source"), QVariant(relativeImageName)));
            }

            newQmlItemNode = QmlItemNode(view->createModelNode("QtQuick.Image", metaInfo.majorVersion(), metaInfo.minorVersion(), propertyPairList));
            parentQmlItemNode.defaultNodeAbstractProperty().reparentHere(newQmlItemNode);

            newQmlItemNode.setId(view->generateNewId("image"));

            if (!view->currentState().isBaseState()) {
                newQmlItemNode.modelNode().variantProperty("opacity").setValue(0);
                newQmlItemNode.setVariantProperty("opacity", 1);
            }

            Q_ASSERT(newQmlItemNode.isValid());
        }
        Q_ASSERT(newQmlItemNode.isValid());
    }


    return newQmlItemNode;
}

bool QmlItemNode::isValid() const
{
    return isValidQmlItemNode(modelNode());
}

bool QmlItemNode::isValidQmlItemNode(const ModelNode &modelNode)
{
    return isValidQmlObjectNode(modelNode) && modelNode.metaInfo().isValid() && isItemOrWindow(modelNode);
}

bool QmlItemNode::isRootNode() const
{
    return modelNode().isValid() && modelNode().isRootNode();
}

QStringList QmlModelStateGroup::names() const
{
    QStringList returnList;

    if (!modelNode().isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (modelNode().property("states").isNodeListProperty()) {
        foreach (const ModelNode &node, modelNode().nodeListProperty("states").toModelNodeList()) {
            if (QmlModelState::isValidQmlModelState(node))
                returnList.append(QmlModelState(node).name());
        }
    }
    return returnList;
}

/**
  \brief Returns list of states (without 'base state').
  The list contains all states defined by this item.
  */
QmlModelStateGroup QmlItemNode::states() const
{
    if (isValid())
        return QmlModelStateGroup(modelNode());
    else
        return QmlModelStateGroup();
}

QList<QmlItemNode> QmlItemNode::children() const
{
    QList<ModelNode> childrenList;

    if (isValid()) {

        if (modelNode().hasNodeListProperty("children"))
                childrenList.append(modelNode().nodeListProperty("children").toModelNodeList());

        if (modelNode().hasNodeListProperty("data")) {
            foreach (const ModelNode &node, modelNode().nodeListProperty("data").toModelNodeList()) {
                if (QmlItemNode::isValidQmlItemNode(node))
                    childrenList.append(node);
            }
        }
    }

    return toQmlItemNodeList(childrenList);
}

QList<QmlObjectNode> QmlItemNode::resources() const
{
    QList<ModelNode> resourcesList;

    if (isValid()) {

        if (modelNode().hasNodeListProperty("resources"))
                resourcesList.append(modelNode().nodeListProperty("resources").toModelNodeList());

        if (modelNode().hasNodeListProperty("data")) {
            foreach (const ModelNode &node, modelNode().nodeListProperty("data").toModelNodeList()) {
                if (!QmlItemNode::isValidQmlItemNode(node))
                    resourcesList.append(node);
            }
        }
    }

    return toQmlObjectNodeList(resourcesList);
}

QList<QmlObjectNode> QmlItemNode::allDirectSubNodes() const
{
    return toQmlObjectNodeList(modelNode().allDirectSubModelNodes());
}

QmlAnchors QmlItemNode::anchors() const
{
    return QmlAnchors(*this);
}

bool QmlItemNode::hasChildren() const
{
    if (modelNode().hasNodeListProperty("children"))
        return true;

    return !children().isEmpty();
}

bool QmlItemNode::hasResources() const
{
    if (modelNode().hasNodeListProperty("resources"))
        return true;

    return !resources().isEmpty();
}

bool QmlItemNode::instanceHasAnchors() const
{
    return anchors().instanceHasAnchors();
}

bool QmlItemNode::instanceHasShowContent() const
{
    return nodeInstance().hasContent();
}

bool QmlItemNode::instanceCanReparent() const
{
    return QmlObjectNode::instanceCanReparent() && !anchors().instanceHasAnchors() && !instanceIsAnchoredBySibling();
}

bool QmlItemNode::instanceIsAnchoredBySibling() const
{
    return nodeInstance().isAnchoredBySibling();
}

bool QmlItemNode::instanceIsAnchoredByChildren() const
{
    return nodeInstance().isAnchoredByChildren();
}

bool QmlItemNode::instanceIsMovable() const
{
    return nodeInstance().isMovable();
}

bool QmlItemNode::instanceIsResizable() const
{
    return nodeInstance().isResizable();
}

bool QmlItemNode::instanceIsInLayoutable() const
{
     return nodeInstance().isInLayoutable();
}

bool QmlItemNode::instanceHasRotationTransform() const
{
    return nodeInstance().transform().type() > QTransform::TxScale;
}

QRectF  QmlItemNode::instanceBoundingRect() const
{
    return QRectF(QPointF(0, 0), nodeInstance().size());
}

QRectF QmlItemNode::instancePaintedBoundingRect() const
{
    return nodeInstance().boundingRect();
}

QRectF QmlItemNode::instanceContentItemBoundingRect() const
{
    return nodeInstance().contentItemBoundingRect();
}

QTransform  QmlItemNode::instanceTransform() const
{
    return nodeInstance().transform();
}

QTransform QmlItemNode::instanceTransformWithContentTransform() const
{
    return nodeInstance().transform() * nodeInstance().contentTransform();
}

QTransform QmlItemNode::instanceTransformWithContentItemTransform() const
{
    return nodeInstance().transform() * nodeInstance().contentItemTransform();
}

QTransform QmlItemNode::instanceSceneTransform() const
{
    return nodeInstance().sceneTransform();
}

QTransform QmlItemNode::instanceSceneContentItemTransform() const
{
    return nodeInstance().sceneTransform() * nodeInstance().contentItemTransform();
}

QPointF QmlItemNode::instanceScenePosition() const
{
    if (hasInstanceParentItem())
        return instanceParentItem().instanceSceneTransform().map(nodeInstance().position());
     else if (modelNode().hasParentProperty() && QmlItemNode::isValidQmlItemNode(modelNode().parentProperty().parentModelNode()))
        return QmlItemNode(modelNode().parentProperty().parentModelNode()).instanceSceneTransform().map(nodeInstance().position());

    return QPointF();
}

QPointF QmlItemNode::instancePosition() const
{
    return nodeInstance().position();
}

QSizeF QmlItemNode::instanceSize() const
{
    return nodeInstance().size();
}

int QmlItemNode::instancePenWidth() const
{
    return nodeInstance().penWidth();
}

bool QmlItemNode::instanceIsRenderPixmapNull() const
{
    return nodeInstance().renderPixmap().isNull();
}

void QmlItemNode::paintInstance(QPainter *painter)
{
    if (nodeInstance().isValid())
        nodeInstance().paint(painter);
}

QList<QmlModelState> QmlModelStateGroup::allStates() const
{
    QList<QmlModelState> returnList;

    if (!modelNode().isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (modelNode().property("states").isNodeListProperty()) {
        foreach (const ModelNode &node, modelNode().nodeListProperty("states").toModelNodeList()) {
            if (QmlModelState::isValidQmlModelState(node))
                returnList.append(node);
        }
    }
    return returnList;
}

TypeName QmlItemNode::simplifiedTypeName() const
{
    return modelNode().simplifiedTypeName();
}

uint qHash(const QmlItemNode &node)
{
    return qHash(node.modelNode());
}

QmlModelState QmlModelStateGroup::addState(const QString &name)
{
    if (!modelNode().isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);


    PropertyListType propertyList;
    propertyList.append(qMakePair(PropertyName("name"), QVariant(name)));

    ModelNode newState = QmlModelState::createQmlState(modelNode().view(), propertyList);
    modelNode().nodeListProperty("states").reparentHere(newState);

    return newState;
}

void QmlModelStateGroup::removeState(const QString &name)
{
    if (!modelNode().isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (state(name).isValid())
        state(name).modelNode().destroy();
}

QmlModelState QmlModelStateGroup::state(const QString &name) const
{
    if (!modelNode().isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (modelNode().property("states").isNodeListProperty()) {
        foreach (const ModelNode &node, modelNode().nodeListProperty("states").toModelNodeList()) {
            if (QmlModelState(node).name() == name)
                return node;
        }
    }
    return QmlModelState();
}

QList<ModelNode> toModelNodeList(const QList<QmlItemNode> &qmlItemNodeList)
{
    QList<ModelNode> modelNodeList;

    foreach (const QmlItemNode &qmlItemNode, qmlItemNodeList)
        modelNodeList.append(qmlItemNode.modelNode());

    return modelNodeList;
}

QList<QmlItemNode> toQmlItemNodeList(const QList<ModelNode> &modelNodeList)
{
    QList<QmlItemNode> qmlItemNodeList;

    foreach (const ModelNode &modelNode, modelNodeList) {
        if (QmlItemNode::isValidQmlItemNode(modelNode))
            qmlItemNodeList.append(modelNode);
    }

    return qmlItemNodeList;
}

const QList<QmlItemNode> QmlItemNode::allDirectSubModelNodes() const
{
    return toQmlItemNodeList(modelNode().allDirectSubModelNodes());
}

const QList<QmlItemNode> QmlItemNode::allSubModelNodes() const
{
    return toQmlItemNodeList(modelNode().allSubModelNodes());
}

bool QmlItemNode::hasAnySubModelNodes() const
{
    return modelNode().hasAnySubModelNodes();
}

void QmlItemNode::setPosition(const QPointF &position)
{
    if (!hasBindingProperty("x"))
        setVariantProperty("x", qRound(position.x()));

    if (!hasBindingProperty("y"))
        setVariantProperty("y", qRound(position.y()));
}

void QmlItemNode::setPostionInBaseState(const QPointF &position)
{
    modelNode().variantProperty("x").setValue(qRound(position.x()));
    modelNode().variantProperty("y").setValue(qRound(position.y()));
}

void QmlItemNode::setSize(const QSizeF &size)
{
    if (!hasBindingProperty("width"))
        setVariantProperty("width", qRound(size.width()));

    if (!hasBindingProperty("height"))
        setVariantProperty("height", qRound(size.height()));
}

} //QmlDesigner
