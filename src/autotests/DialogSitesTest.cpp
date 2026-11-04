/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A scan of src/: every framework widget dialog kept by decision is opened inside a
// FrameworkDialogScope in the same function (or lambda), no replaced widget dialog is built
// outside its own files, and nothing starts ZModem. NoWidgetsTest checks the running
// application; this catches call sites no scenario reaches.

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

namespace {
// The replaced widget dialogs and the Configure dialog's pages: compiled for the ABI check and
// the tests only.
const QStringList replacedFiles = {QStringLiteral("widgets/EditProfileDialog."),
    QStringLiteral("colorscheme/ColorSchemeEditor."),
    QStringLiteral("KeyBindingEditor."),
    QStringLiteral("RenameTabDialog."),
    QStringLiteral("HistorySizeDialog."),
    QStringLiteral("CopyInputDialog."),
    QStringLiteral("ZModemDialog."),
    QStringLiteral("settings/")};

const QRegularExpression& frameworkDialog()
{
    static const QRegularExpression pattern(QStringLiteral(
        R"(\bKMessageBox::(?!enableAllMessages\b)[a-z]\w*\s*\()"
        R"(|\bQMessageBox::[a-z]\w*\s*\()"
        R"(|\b(?:QInputDialog|QFileDialog|QColorDialog|KIconDialog)::get\w*\s*\()"
        R"(|\bnew\s+(?:QInputDialog|QFileDialog|QColorDialog|QPrintDialog|KIconDialog|KShortcutsDialog|FontDialog|KNSWidgets::\w+)\b)"
        R"(|\b(?:QInputDialog|QFileDialog|QColorDialog|QPrintDialog|KIconDialog|KShortcutsDialog|FontDialog)\s+\w+\s*[({;])"
        R"(|\bKNotifyConfigWidget::configure\s*\()"
        R"(|\bshowConfigureShortcutsDialog\s*\()"
        // A KIO job's dialogs, through its UI delegate or window (FrameworkDialogScope::attach()).
        R"(|\b(?:createDefaultJobUiDelegate|KJobWidgets::setWindow)\s*\()"
        // KHelpMenu's and KBookmarkMenu's dialog slots, called directly or through a member pointer.
        R"(|\b(?:aboutApplication|aboutKDE|reportBug|switchApplicationLanguage|slotAddBookmark|slotAddBookmarksList|slotNewFolder)\s*\(\s*\))"
        R"(|\(\s*\w+\s*->\*\s*\w+\s*\)\s*\()"));
    return pattern;
}

const QRegularExpression& replacedDialog()
{
    static const QRegularExpression pattern(QStringLiteral(
        R"(\bnew\s+(?:EditProfileDialog|ConfigurationDialog|ColorSchemeEditor|KeyBindingEditor|RenameTabDialog|HistorySizeDialog|CopyInputDialog|ZModemDialog)\b)"
        R"(|\b(?:EditProfileDialog|ConfigurationDialog|ColorSchemeEditor|KeyBindingEditor|RenameTabDialog|HistorySizeDialog|CopyInputDialog|ZModemDialog)\s+\w+\s*[({;])"
        R"(|(?<!::)(?<!void )\bstartZModem\s*\()"));
    return pattern;
}

// Comments, string and character literals and preprocessor lines blanked, offsets kept.
QString stripped(const QString& source)
{
    QString code = source;
    enum { Code,
        LineComment,
        BlockComment,
        String,
        Char,
        Preprocessor } state = Code;
    bool lineStart = true;
    for (qsizetype i = 0; i < code.size(); ++i) {
        const QChar c = code.at(i);
        const QChar next = i + 1 < code.size() ? code.at(i + 1) : QChar();
        const bool newline = c == u'\n';
        switch (state) {
        case Code:
            if (c == u'/' && next == u'/') {
                state = LineComment;
            } else if (c == u'/' && next == u'*') {
                state = BlockComment;
            } else if (c == u'"') {
                state = String;
                break;
            } else if (c == u'\'') {
                state = Char;
                break;
            } else if (c == u'#' && lineStart) {
                state = Preprocessor;
            }
            if (state != Code) {
                code[i] = u' ';
            }
            break;
        case LineComment:
        case Preprocessor:
            if (newline) {
                state = Code;
            } else {
                code[i] = u' ';
            }
            break;
        case BlockComment:
            if (c == u'*' && next == u'/') {
                code[i] = code[i + 1] = u' ';
                ++i;
                state = Code;
            } else if (!newline) {
                code[i] = u' ';
            }
            break;
        case String:
        case Char:
            if (c == u'\\') {
                code[i] = u' ';
                if (i + 1 < code.size() && code.at(i + 1) != u'\n') {
                    code[++i] = u' ';
                }
            } else if (c == (state == String ? u'"' : u'\'')) {
                state = Code;
            } else if (!newline) {
                code[i] = u' ';
            }
            break;
        }
        if (newline) {
            lineStart = true;
        } else if (!c.isSpace()) {
            lineStart = false;
        }
    }
    return code;
}

struct Block {
    qsizetype open;
    qsizetype close;
    // A function's or a lambda's body, not a namespace, class or statement block.
    bool function;
};

// A brace initializer inside an expression: QVector<QString>({…}), x = {…}, return {…}.
bool isInitializer(const QString& header)
{
    static const QRegularExpression initializer(QStringLiteral(R"([=,(]$|\breturn$)"));
    return initializer.match(header.simplified()).hasMatch();
}

bool isFunctionBody(const QString& header)
{
    static const QRegularExpression container(QStringLiteral(R"(\b(?:namespace|class|struct|union|enum|extern)\b)"));
    static const QRegularExpression statement(QStringLiteral(R"(^(?:if|for|while|switch|catch|else|do|try)\b|^$|:$)"));
    static const QRegularExpression lambda(QStringLiteral(R"(\]\s*(?:\(.*\))?\s*(?:mutable\s*)?(?:->[^{]*)?$)"),
        QRegularExpression::DotMatchesEverythingOption);
    const QString text = header.simplified();
    if (lambda.match(text).hasMatch()) {
        return true;
    }
    if (isInitializer(text) || statement.match(text).hasMatch() || container.match(text).hasMatch()) {
        return false;
    }
    return true;
}

QList<Block> blocks(const QString& code)
{
    QList<Block> result;
    // Each open block, and where the statement holding it started, if it is an initializer
    // (the statement goes on after it), else -1.
    QList<std::pair<qsizetype, qsizetype>> open;
    qsizetype statementStart = 0;
    for (qsizetype i = 0; i < code.size(); ++i) {
        const QChar c = code.at(i);
        if (c == u'{') {
            const QString header = code.mid(statementStart, i - statementStart);
            result.append({i, code.size(), isFunctionBody(header)});
            open.append({result.size() - 1, isInitializer(header) ? statementStart : -1});
            statementStart = i + 1;
        } else if (c == u'}') {
            qsizetype resume = -1;
            if (!open.isEmpty()) {
                const auto [block, start] = open.takeLast();
                result[block].close = i;
                resume = start;
            }
            statementStart = resume >= 0 ? resume : i + 1;
        } else if (c == u';') {
            statementStart = i + 1;
        }
    }
    return result;
}

// The innermost block containing position, of any kind or only function bodies; -1 if none.
qsizetype innermost(const QList<Block>& all, qsizetype position, bool functionOnly)
{
    qsizetype found = -1;
    for (qsizetype i = 0; i < all.size(); ++i) {
        const Block& block = all.at(i);
        if (block.open < position && position < block.close && (!functionOnly || block.function) && (found < 0 || block.open > all.at(found).open)) {
            found = i;
        }
    }
    return found;
}

// The framework dialog sites with no FrameworkDialogScope open before them in their function, as
// "line: text".
QStringList unscopedSites(const QString& source)
{
    static const QRegularExpression scope(QStringLiteral(R"(\bFrameworkDialogScope\s+\w+\s*\(|\bFrameworkDialogScope::attach\s*\()"));
    const QString code = stripped(source);
    const QList<Block> all = blocks(code);
    QStringList result;
    auto it = frameworkDialog().globalMatch(code);
    while (it.hasNext()) {
        const QRegularExpressionMatch site = it.next();
        const qsizetype function = innermost(all, site.capturedStart(), true);
        bool scoped = false;
        if (function >= 0) {
            auto scopes = scope.globalMatch(code);
            while (scopes.hasNext() && !scoped) {
                const qsizetype at = scopes.next().capturedStart();
                const qsizetype block = innermost(all, at, false);
                scoped = at > all.at(function).open && at < site.capturedStart() && block >= 0 && all.at(block).open >= all.at(function).open
                    && site.capturedStart() < all.at(block).close;
            }
        }
        if (!scoped) {
            result.append(QStringLiteral("%1: %2").arg(code.left(site.capturedStart()).count(u'\n') + 1).arg(site.captured().simplified()));
        }
    }
    return result;
}

QStringList matches(const QString& source, const QRegularExpression& pattern)
{
    const QString code = stripped(source);
    QStringList result;
    auto it = pattern.globalMatch(code);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result.append(QStringLiteral("%1: %2").arg(code.left(match.capturedStart()).count(u'\n') + 1).arg(match.captured().simplified()));
    }
    return result;
}

// src/'s .cpp and .h files but the tests' and the parity hook's, relative to src/.
QStringList sourceFiles()
{
    QStringList files;
    const QString root = QStringLiteral(KONSOLE_SOURCE_DIR);
    QDirIterator it(root, {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = QDir(root).relativeFilePath(it.next());
        if (!path.startsWith(QLatin1String("autotests/")) && !path.startsWith(QLatin1String("parity/"))) {
            files.append(path);
        }
    }
    files.sort();
    return files;
}

QString read(const QString& path)
{
    QFile file(QDir(QStringLiteral(KONSOLE_SOURCE_DIR)).filePath(path));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

bool replaced(const QString& path)
{
    return std::any_of(replacedFiles.cbegin(), replacedFiles.cend(), [&path](const QString& prefix) {
        return path.startsWith(prefix);
    });
}
}

class DialogSitesTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The scan itself, on fixtures: each unscoped form is found, each scoped form isn't.
    void testScanner_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<int>("unscoped");

        QTest::newRow("unscoped") << QStringLiteral("void f()\n{\n    KMessageBox::error(this, text);\n}\n") << 1;
        QTest::newRow("scoped") << QStringLiteral("void f()\n{\n    FrameworkDialogScope scope(name);\n    KMessageBox::error(this, text);\n}\n") << 0;
        QTest::newRow("scope after the site") << QStringLiteral("void f()\n{\n    KMessageBox::error(this, text);\n    FrameworkDialogScope scope(name);\n}\n") << 1;
        QTest::newRow("scope in another function")
            << QStringLiteral("void g()\n{\n    FrameworkDialogScope scope(name);\n}\nvoid f()\n{\n    QFileDialog::getOpenFileName(this);\n}\n") << 1;
        QTest::newRow("scope in a closed block")
            << QStringLiteral("void f()\n{\n    if (a) {\n        FrameworkDialogScope scope(name);\n    }\n    QColorDialog::getColor(c);\n}\n") << 1;
        QTest::newRow("scope outside a lambda")
            << QStringLiteral("void f()\n{\n    FrameworkDialogScope scope(name);\n    connect(a, &A::b, this, [this] {\n        new FontDialog(this);\n    });\n}\n") << 1;
        QTest::newRow("scope inside a lambda")
            << QStringLiteral("void f()\n{\n    connect(a, &A::b, this, [this](bool x) {\n        FrameworkDialogScope scope(name);\n        new FontDialog(this);\n    });\n}\n")
            << 0;
        QTest::newRow("attached to a job") << QStringLiteral(
            "void f()\n{\n    auto *job = new KIO::OpenUrlJob(url);\n    FrameworkDialogScope::attach(job, name);\n    job->setUiDelegate(createDefaultJobUiDelegate(a, b));\n}\n")
                                           << 0;
        QTest::newRow("job without a scope")
            << QStringLiteral("void f()\n{\n    auto *job = KIO::drop(event, target);\n    KJobWidgets::setWindow(job, display);\n}\n") << 1;
        QTest::newRow("in a comment or string") << QStringLiteral("void f()\n{\n    // KMessageBox::error(this, text);\n    s = \"QFileDialog::getOpenFileName(\";\n}\n") << 0;
        QTest::newRow("enum values") << QStringLiteral("void f()\n{\n    return r == KMessageBox::PrimaryAction;\n}\n") << 0;
        QTest::newRow("brace initializer in a condition")
            << QStringLiteral("void f()\n{\n    FrameworkDialogScope s(n);\n    if (!QList<int>({1, 2}).contains(x)) {\n        KMessageBox::error(this, t);\n    }\n}\n") << 0;
        QTest::newRow("member pointer call") << QStringLiteral("void f()\n{\n    (helpMenu->*open)();\n}\n") << 1;
        QTest::newRow("in a namespace and a class")
            << QStringLiteral("namespace N {\nclass C {\n    void f()\n    {\n        FrameworkDialogScope s(n);\n        KIconDialog d;\n    }\n};\n}\n") << 0;
    }

    void testScanner()
    {
        QFETCH(QString, source);
        QFETCH(int, unscoped);
        QCOMPARE(unscopedSites(source).size(), unscoped);
    }

    void testFrameworkDialogsAreScoped()
    {
        QStringList failures;
        const QStringList files = sourceFiles();
        QVERIFY(files.contains(QStringLiteral("MainWindow.cpp")));
        for (const QString& path : files) {
            if (replaced(path)) {
                continue;
            }
            const QStringList sites = unscopedSites(read(path));
            for (const QString& site : sites) {
                failures.append(path + QLatin1Char(':') + site);
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(QStringLiteral("framework dialogs without a FrameworkDialogScope:\n") + failures.join(u'\n')));
    }

    void testReplacedDialogsAreUnused()
    {
        QStringList failures;
        const QStringList files = sourceFiles();
        for (const QString& path : files) {
            if (replaced(path)) {
                continue;
            }
            const QStringList found = matches(read(path), replacedDialog());
            for (const QString& match : found) {
                failures.append(path + QLatin1Char(':') + match);
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(QStringLiteral("replaced widget dialogs or ZModem used:\n") + failures.join(u'\n')));
        // The rule finds what it is for.
        QCOMPARE(matches(QStringLiteral("auto *dialog = new EditProfileDialog(this);\nsession->startZModem(a, b, c);\n"), replacedDialog()).size(), 2);
        QCOMPARE(matches(QStringLiteral("void Session::startZModem(const QString &zmodem)\n"), replacedDialog()).size(), 0);
    }
};

QTEST_GUILESS_MAIN(DialogSitesTest)

#include "DialogSitesTest.moc"
