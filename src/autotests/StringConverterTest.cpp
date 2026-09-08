/*
    SPDX-FileCopyrightText: 2025 SonicDE

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QObject>
#include <QSet>
#include <QTest>

#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>

class StringConverterTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testAvailableCodecsDecode();
    void testAvailableCodecsEncode();
    void testUtf8RoundTrip();
    void testCanonicalNames();
};

void StringConverterTest::testAvailableCodecsDecode()
{
    const QStringList codecs = QStringConverter::availableCodecs();
    QVERIFY(!codecs.isEmpty());
    for (const QString& name : codecs) {
        QStringDecoder decoder(name.toUtf8());
        QVERIFY(decoder.isValid());
        QVERIFY(!decoder.hasError());
    }
}

void StringConverterTest::testAvailableCodecsEncode()
{
    const QStringList codecs = QStringConverter::availableCodecs();
    for (const QString& name : codecs) {
        QStringEncoder encoder(name.toUtf8());
        QVERIFY(encoder.isValid());
        QVERIFY(!encoder.hasError());
    }
}

void StringConverterTest::testUtf8RoundTrip()
{
    QStringDecoder decoder(QStringConverter::Utf8);
    QStringEncoder encoder(QStringConverter::Utf8);

    const QString input = QStringLiteral("héllo wörld — ünïcode ✓");
    const QByteArray encoded = encoder.encode(input);
    const QString decoded = decoder.decode(encoded);

    QCOMPARE(decoded, input);
    QVERIFY(!decoder.hasError());
}

void StringConverterTest::testCanonicalNames()
{
    // Canonical converter names must be comparable as a set without
    // relying on the order of availableCodecs().
    const QStringList codecs = QStringConverter::availableCodecs();
    const QSet<QString> unique(codecs.cbegin(), codecs.cend());

    // Every reported name must be unique after canonicalization.
    QCOMPARE(unique.size(), codecs.size());

    // Well-known codecs must be present by canonical name.
    QVERIFY(unique.contains(QStringLiteral("UTF-8")));
}

QTEST_MAIN(StringConverterTest)

#include "StringConverterTest.moc"
