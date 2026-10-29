/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KActionCollection>
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>
#include <kde_terminal_interface.h>

#include <QAction>
#include <QDomDocument>
#include <QFile>
#include <QKeyEvent>
#include <QMetaMethod>
#include <QTest>

class PartCompatTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void isReadOnlyPartWithTerminalInterface();
    void hasMethod_data();
    void hasMethod();
    void hasSignal_data();
    void hasSignal();
    void invokesByName();
    void hasEveryPartUiAction();
    void mergesIntoHostWindow();

private:
    KParts::ReadOnlyPart* _part = nullptr;
};

void PartCompatTest::initTestCase()
{
    const KPluginMetaData metaData(QStringLiteral("kf6/parts/konsolepart"));
    QVERIFY2(metaData.isValid(), "konsolepart plugin not found under kf6/parts");
    QCOMPARE(metaData.pluginId(), QStringLiteral("konsolepart"));
    auto result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(metaData, this);
    QVERIFY2(result, qPrintable(result.errorString));
    _part = result.plugin;
}

void PartCompatTest::cleanupTestCase()
{
    delete _part;
}

void PartCompatTest::isReadOnlyPartWithTerminalInterface()
{
    QVERIFY(qobject_cast<TerminalInterface*>(_part));
    QVERIFY(_part->widget());
    QVERIFY(_part->widget()->inherits("QWidget"));
}

void PartCompatTest::hasMethod_data()
{
    QTest::addColumn<QString>("signature");
    for (const char* signature : {"setContextMenuAdditionalActions(QList<QAction*>)",
             "createSession()",
             "createSession(QString)",
             "createSession(QString,QString)",
             "showManageProfilesDialog(QWidget*)",
             "showEditCurrentProfileDialog(QWidget*)",
             "changeSessionSettings(QString)",
             "openTeletype(int)",
             "openTeletype(int,bool)",
             "setMonitorSilenceEnabled(bool)",
             "setMonitorActivityEnabled(bool)",
             "isBlurEnabled()"}) {
        QTest::newRow(signature) << QString::fromLatin1(signature);
    }
}

void PartCompatTest::hasMethod()
{
    QFETCH(QString, signature);
    const QByteArray normalized = QMetaObject::normalizedSignature(signature.toLatin1().constData());
    QVERIFY2(_part->metaObject()->indexOfMethod(normalized.constData()) >= 0, normalized.constData());
}

void PartCompatTest::hasSignal_data()
{
    QTest::addColumn<QString>("signature");
    for (const char* signature : {"overrideShortcut(QKeyEvent*,bool&)",
             "silenceDetected()",
             "activityDetected()",
             "currentDirectoryChanged(QString)",
             "setWindowCaption(QString)",
             "setStatusBarText(QString)"}) {
        QTest::newRow(signature) << QString::fromLatin1(signature);
    }
}

void PartCompatTest::hasSignal()
{
    QFETCH(QString, signature);
    const QByteArray normalized = QMetaObject::normalizedSignature(signature.toLatin1().constData());
    QVERIFY2(_part->metaObject()->indexOfSignal(normalized.constData()) >= 0, normalized.constData());
}

void PartCompatTest::invokesByName()
{
    bool blur = true;
    QVERIFY(QMetaObject::invokeMethod(_part, "isBlurEnabled", Qt::DirectConnection, Q_RETURN_ARG(bool, blur)));
    QVERIFY(QMetaObject::invokeMethod(_part, "setMonitorActivityEnabled", Qt::DirectConnection, Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(_part, "setMonitorSilenceEnabled", Qt::DirectConnection, Q_ARG(bool, false)));
    QVERIFY(connect(_part, SIGNAL(currentDirectoryChanged(QString)), this, SLOT(deleteLater())));
    QVERIFY(disconnect(_part, SIGNAL(currentDirectoryChanged(QString)), this, SLOT(deleteLater())));
}

void PartCompatTest::hasEveryPartUiAction()
{
    QFile file(QStringLiteral(PARTUI_RC));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QDomDocument document;
    QVERIFY(document.setContent(&file));
    const QDomNodeList actions = document.elementsByTagName(QStringLiteral("Action"));
    QVERIFY(actions.size() > 0);
    // Session actions live in the part's child GUI client (the session controller), as hosts see them.
    QList<KXMLGUIClient*> clients = {_part};
    for (int i = 0; i < clients.size(); ++i) {
        clients += clients.at(i)->childClients();
    }
    QStringList missing;
    for (int i = 0; i < actions.size(); ++i) {
        const QString name = actions.at(i).toElement().attribute(QStringLiteral("name"));
        const bool found = std::any_of(clients.cbegin(), clients.cend(), [&name](KXMLGUIClient* client) {
            return client->actionCollection()->action(name) != nullptr;
        });
        if (!found) {
            missing.append(name);
        }
    }
    // manage-profiles is created only by the application window (MainWindow.cpp); the part
    // never had it, and KXmlGui skips missing actions, so hosts have never seen it.
    QCOMPARE(missing, QStringList{QStringLiteral("manage-profiles")});
}

void PartCompatTest::mergesIntoHostWindow()
{
    KXmlGuiWindow host;
    host.setupGUI(KXmlGuiWindow::Create, QStringLiteral(PARTCOMPAT_HOST_RC));
    host.guiFactory()->addClient(_part);
    QVERIFY(host.guiFactory()->clients().contains(_part));
    host.guiFactory()->removeClient(_part);
}

QTEST_MAIN(PartCompatTest)

#include "PartCompatTest.moc"
