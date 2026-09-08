// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTemporaryDir>
#include <QTest>

class DBusCodecTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        const QDBusReply<QStringList> before = QDBusConnection::sessionBus().interface()->registeredServiceNames();
        QVERIFY(before.isValid());
        m_existingServices = before.value();

        QVERIFY(m_configDir.isValid());
        m_xvfb.setProcessChannelMode(QProcess::SeparateChannels);
        m_xvfb.start(QStringLiteral(XVFB_EXECUTABLE),
            {QStringLiteral("-displayfd"), QStringLiteral("1"), QStringLiteral("-screen"), QStringLiteral("0"), QStringLiteral("1024x768x24"), QStringLiteral("-nolisten"), QStringLiteral("tcp")});
        QVERIFY2(m_xvfb.waitForStarted(), qPrintable(m_xvfb.errorString()));
        QVERIFY2(m_xvfb.waitForReadyRead(5000), qPrintable(QString::fromLocal8Bit(m_xvfb.readAllStandardError())));
        const QString display = QStringLiteral(":%1").arg(QString::fromLatin1(m_xvfb.readLine()).trimmed());

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
        environment.insert(QStringLiteral("QT_NO_XDG_DESKTOP_PORTAL"), QStringLiteral("1"));
        environment.insert(QStringLiteral("GTK_USE_PORTAL"), QStringLiteral("0"));
        environment.insert(QStringLiteral("DISPLAY"), display);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), m_configDir.path());
        environment.insert(QStringLiteral("XDG_DATA_HOME"), m_configDir.path());
        m_process.setProcessEnvironment(environment);
        m_process.setProcessChannelMode(QProcess::SeparateChannels);
        m_process.start(QStringLiteral(KONSOLE_EXECUTABLE), {QStringLiteral("--separate")});
        QVERIFY2(m_process.waitForStarted(), qPrintable(m_process.errorString()));

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 10000 && m_process.state() != QProcess::NotRunning && !findNewService()) {
            QTest::qWait(50);
        }
        const QString diagnostic = QStringLiteral("state=%1 exit=%2 stderr=%3")
                                       .arg(m_process.state())
                                       .arg(m_process.exitCode())
                                       .arg(QString::fromLocal8Bit(m_process.readAllStandardError()));
        QVERIFY2(!m_service.isEmpty(), qPrintable(diagnostic));
        QTRY_VERIFY_WITH_TIMEOUT(findSessionPath(), 10000);
    }

    void cleanupTestCase()
    {
        if (!m_service.isEmpty()) {
            QDBusInterface application(m_service,
                QStringLiteral("/MainApplication"),
                QStringLiteral("org.qtproject.Qt.QCoreApplication"));
            application.asyncCall(QStringLiteral("quit"));
        }
        if (!m_process.waitForFinished(5000)) {
            m_process.kill();
            m_process.waitForFinished();
        }
        m_xvfb.terminate();
        if (!m_xvfb.waitForFinished(3000)) {
            m_xvfb.kill();
            m_xvfb.waitForFinished();
        }
    }

    void codecRoundTrip()
    {
        QDBusInterface session(m_service, m_sessionPath, QStringLiteral("org.kde.konsole.Session"));
        QVERIFY(session.isValid());

        QStringList codecs{
            QStringLiteral("UTF-8"),
            QStringLiteral("UTF-16"),
            QStringLiteral("UTF-16LE"),
            QStringLiteral("UTF-16BE"),
            QStringLiteral("UTF-32"),
            QStringLiteral("UTF-32LE"),
            QStringLiteral("UTF-32BE"),
            QStringLiteral("ISO-8859-1"),
        };
        const QStringList available = QStringConverter::availableCodecs();
        for (const QString& codec : available.mid(0, 16)) {
            if (!codecs.contains(codec, Qt::CaseInsensitive)) {
                codecs.append(codec);
            }
        }

        for (const QString& availableCodec : codecs) {
            const QByteArray requested = availableCodec.toUtf8();
            const QDBusReply<bool> setReply = session.call(QStringLiteral("setCodec"), requested);
            QVERIFY2(setReply.isValid(), qPrintable(setReply.error().message()));
            QVERIFY2(setReply.value(), qPrintable(QStringLiteral("codec rejected: %1").arg(availableCodec)));

            const QDBusReply<QByteArray> getReply = session.call(QStringLiteral("codec"));
            QVERIFY2(getReply.isValid(), qPrintable(getReply.error().message()));
            const QStringDecoder expected(requested);
            const QStringDecoder actual(getReply.value());
            QVERIFY(expected.isValid());
            QVERIFY2(actual.isValid(),
                qPrintable(QStringLiteral("requested=%1 returned=%2")
                        .arg(QString::fromUtf8(requested), QString::fromUtf8(getReply.value()))));
            QCOMPARE(QByteArray(actual.name()), QByteArray(expected.name()));
        }
    }

    void invalidCodecPreservesCurrentCodec()
    {
        QDBusInterface session(m_service, m_sessionPath, QStringLiteral("org.kde.konsole.Session"));
        const QDBusReply<QByteArray> before = session.call(QStringLiteral("codec"));
        QVERIFY(before.isValid());

        const QDBusReply<bool> invalid = session.call(QStringLiteral("setCodec"), QByteArrayLiteral("not-a-real-codec"));
        QVERIFY(invalid.isValid());
        QVERIFY(!invalid.value());

        const QDBusReply<QByteArray> after = session.call(QStringLiteral("codec"));
        QVERIFY(after.isValid());
        QCOMPARE(after.value(), before.value());
    }

private:
    bool findNewService()
    {
        const QDBusReply<QStringList> services = QDBusConnection::sessionBus().interface()->registeredServiceNames();
        if (!services.isValid()) {
            return false;
        }
        for (const QString& service : services.value()) {
            if (service.startsWith(QLatin1String("org.kde.konsole")) && !m_existingServices.contains(service)) {
                m_service = service;
                return true;
            }
        }
        return false;
    }

    bool findSessionPath()
    {
        QDBusInterface window(m_service, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Window"));
        if (!window.isValid()) {
            return false;
        }
        const QDBusReply<QStringList> sessions = window.call(QStringLiteral("sessionList"));
        if (!sessions.isValid() || sessions.value().isEmpty()) {
            return false;
        }
        m_sessionPath = QStringLiteral("/Sessions/%1").arg(sessions.value().constFirst());
        return QDBusInterface(m_service, m_sessionPath, QStringLiteral("org.kde.konsole.Session")).isValid();
    }

    QStringList m_existingServices;
    QString m_service;
    QString m_sessionPath;
    QProcess m_process;
    QProcess m_xvfb;
    QTemporaryDir m_configDir;
};

QTEST_GUILESS_MAIN(DBusCodecTest)

#include "DBusCodecTest.moc"
