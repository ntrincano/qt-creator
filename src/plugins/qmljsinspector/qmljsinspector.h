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

#ifndef QMLJSINSPECTOR_H
#define QMLJSINSPECTOR_H

#include "qmljsprivateapi.h"

#include <coreplugin/basemode.h>
#include <debugger/debuggerconstants.h>
#include <qmlprojectmanager/qmlprojectrunconfiguration.h>

#include <qmljs/qmljsdocument.h>
#include <qmljs/parser/qmljsastfwd_p.h>

#include <QtGui/QAction>
#include <QtCore/QObject>

namespace ProjectExplorer {
    class Project;
    class Environment;
}

namespace TextEditor {
    class ITextEditor;
}

namespace Core {
    class IContext;
}

namespace QmlJS {
    class ModelManagerInterface;
}

namespace Debugger {
    class QmlEngine;
}

QT_FORWARD_DECLARE_CLASS(QDockWidget)

namespace QmlJSInspector {
namespace Internal {

class QmlInspectorToolbar;
class QmlJSObjectTree;
class ClientProxy;
class InspectorSettings;
class ContextCrumblePath;
class QmlJSLiveTextPreview;

class InspectorUi : public QObject
{
    Q_OBJECT

public:
    enum DebugMode {
        StandaloneMode,
        CppProjectWithQmlEngines,
        QmlProjectWithCppPlugins
    };
public:
    InspectorUi(QObject *parent = 0);
    virtual ~InspectorUi();

    void saveSettings() const;
    void restoreSettings();

    bool showExperimentalWarning();
    void setShowExperimentalWarning(bool value);

    static InspectorUi *instance();

    // returns the project being currently debugged, or 0 if not debugging anything
    ProjectExplorer::Project *debugProject() const;
    QString debugProjectBuildDirectory() const;
    bool isShadowBuildProject() const;

    void setupUi();
    bool isConnected() const;
    void connected(ClientProxy *clientProxy);
    void disconnected();
    void setDebuggerEngine(Debugger::QmlEngine *qmlEngine);
    Debugger::QmlEngine *debuggerEngine() const;

signals:
    void statusMessage(const QString &text);
    void livePreviewActivated(bool isActivated);

public slots:
    void setSimpleDockWidgetArrangement(const Debugger::DebuggerLanguages &activeLanguages);
    void reloadQmlViewer();
    void serverReloaded();
    void setApplyChangesToQmlObserver(bool applyChanges);

private slots:
    void enable();
    void disable();
    void gotoObjectReferenceDefinition(const QDeclarativeDebugObjectReference &obj);
    void gotoObjectReferenceDefinition(QList<QDeclarativeDebugObjectReference> objectReferences);
    void changeSelectedItems(const QList<QDeclarativeDebugObjectReference> &objects);

    void updateEngineList();


    void removePreviewForEditor(Core::IEditor *newEditor);
    QmlJSLiveTextPreview *createPreviewForEditor(Core::IEditor *newEditor);

    void disableLivePreview();
    void crumblePathElementClicked(int);

    void currentDebugProjectRemoved();
    void updatePendingPreviewDocuments(QmlJS::Document::Ptr doc);
    void showDebuggerTooltip(const QPoint &mousePos, TextEditor::ITextEditor *editor, int cursorPos);
    void debugQueryUpdated(QDeclarativeDebugQuery::State);

private:
    bool addQuotesForData(const QVariant &value) const;
    void resetViews();

    void initializeDocuments();
    void applyChangesToQmlObserverHelper(bool applyChanges);
    void setupToolbar(bool doConnect);
    void setupDockWidgets();
    QString filenameForShadowBuildFile(const QString &filename) const;

private:
    bool m_listeningToEditorManager;

    QmlInspectorToolbar *m_toolbar;
    ContextCrumblePath *m_crumblePath;
    QmlJSObjectTree *m_objectTreeWidget;
    QDockWidget *m_inspectorDockWidget;

    InspectorSettings *m_settings;
    ClientProxy *m_clientProxy;
    Debugger::QmlEngine *m_qmlEngine;
    QDeclarativeDebugExpressionQuery *m_debugQuery;

    // Qml/JS integration
    QHash<QString, QmlJSLiveTextPreview *> m_textPreviews;
    QmlJS::Snapshot m_loadedSnapshot; //the snapshot loaded by the viewer

    // project is needed for matching filenames, esp. with shadow builds.
    ProjectExplorer::Project *m_debugProject;
    QString m_debugProjectBuildDir;

    QStringList m_pendingPreviewDocumentNames;

    static InspectorUi *m_instance;
};

} // Internal
} // QmlJSInspector

#endif
