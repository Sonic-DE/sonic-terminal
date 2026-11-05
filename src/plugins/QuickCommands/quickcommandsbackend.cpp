/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quickcommandsbackend.h"

#include "filtermodel.h"
#include "quickcommanddata.h"
#include "quickcommandsmodel.h"

#include "MainWindow.h"
#include "quick/FrameworkDialogScope.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

QuickCommandsBackend::QuickCommandsBackend(QuickCommandsModel* model, Konsole::MainWindow* window, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_filterModel(new FilterModel(this))
    , m_window(window)
    , m_warningsTitle(i18n("Warnings"))
{
    m_filterModel->setSourceModel(model);
    connect(model, &QAbstractItemModel::rowsInserted, this, &QuickCommandsBackend::groupsChanged);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &QuickCommandsBackend::groupsChanged);

    m_hasShellCheck = !QStandardPaths::findExecutable(QStringLiteral("shellcheck")).isEmpty();
    if (!m_hasShellCheck) {
        m_warnings = QStringLiteral("Missing executable 'shellcheck', please install");
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("quickcommands"));
    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_G);
    m_shortcut = QKeySequence(settings.value(QStringLiteral("shortcut"), def.toString()).toString());
}

QuickCommandsBackend::~QuickCommandsBackend() = default;

QAbstractItemModel* QuickCommandsBackend::model() const
{
    return m_filterModel;
}

QStringList QuickCommandsBackend::groups() const
{
    return m_model->groups();
}

QString QuickCommandsBackend::warnings() const
{
    return m_warnings;
}

void QuickCommandsBackend::setWarnings(const QString& warnings)
{
    if (m_warnings != warnings) {
        m_warnings = warnings;
        Q_EMIT warningsChanged();
    }
}

QString QuickCommandsBackend::warningsTitle() const
{
    return m_warningsTitle;
}

QKeySequence QuickCommandsBackend::shortcut() const
{
    return m_shortcut;
}

void QuickCommandsBackend::setShortcut(const QKeySequence& shortcut)
{
    if (m_shortcut != shortcut) {
        m_shortcut = shortcut;
        Q_EMIT quickAccessShortcutChanged(shortcut);
    }
}

void QuickCommandsBackend::setCurrentController(Konsole::SessionController* controller)
{
    m_controller = controller;
}

void QuickCommandsBackend::setFilter(const QString& text)
{
    m_filterModel->setFilterRegularExpression(text);
    m_filterModel->invalidate();
}

void QuickCommandsBackend::setInvertFilter(bool invert)
{
    m_filterModel->setInvertFilter(invert);
}

QVariantMap QuickCommandsBackend::selected(const QModelIndex& index) const
{
    const QModelIndex sourceIdx = m_filterModel->mapToSource(index);
    if (m_model->rowCount(sourceIdx) != 0) {
        return {{QStringLiteral("name"), QString()},
            {QStringLiteral("tooltip"), QString()},
            {QStringLiteral("command"), QString()},
            {QStringLiteral("group"), QString()}};
    }
    const QStandardItem* item = m_model->itemFromIndex(sourceIdx);
    if (item == nullptr || item->parent() == nullptr) {
        return {};
    }
    const auto data = item->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
    return {{QStringLiteral("name"), data.name},
        {QStringLiteral("tooltip"), data.tooltip},
        {QStringLiteral("command"), data.command},
        {QStringLiteral("group"), item->parent()->text()}};
}

bool QuickCommandsBackend::valid(const QString& name, const QString& command)
{
    Konsole::FrameworkDialogScope scope(QStringLiteral("invalid quick command"));
    if (name.trimmed().isEmpty()) {
        KMessageBox::error(m_window, i18n("Title can not be empty or blank"));
        return false;
    }
    if (command.isEmpty()) {
        KMessageBox::error(m_window, i18n("Command can not be empty"));
        return false;
    }
    return true;
}

bool QuickCommandsBackend::saveCommand(const QString& name, const QString& tooltip, const QString& command, const QString& group)
{
    if (!valid(name, command)) {
        return false;
    }
    QuickCommandData data;
    data.name = name.trimmed();
    data.tooltip = tooltip;
    data.command = command;
    if (m_model->addChildItem(data, group)) {
        return true;
    }
    Konsole::FrameworkDialogScope scope(QStringLiteral("duplicate quick command"));
    KMessageBox::error(m_window, i18n("A duplicate item exists"));
    return false;
}

bool QuickCommandsBackend::updateCommand(const QModelIndex& index, const QString& name, const QString& tooltip, const QString& command, const QString& group)
{
    if (!valid(name, command)) {
        return false;
    }
    QuickCommandData data;
    data.name = name.trimmed();
    data.tooltip = tooltip;
    data.command = command;
    if (m_model->editChildItem(data, m_filterModel->mapToSource(index), group)) {
        return true;
    }
    Konsole::FrameworkDialogScope scope(QStringLiteral("duplicate quick command"));
    KMessageBox::error(m_window, i18n("A duplicate item exists"));
    return false;
}

void QuickCommandsBackend::send(const QString& command)
{
    m_controller->session()->sendTextToTerminal(command, QLatin1Char('\r'));
    if (m_controller->session()->views().count()) {
        m_controller->session()->views().at(0)->setFocus();
    }
}

void QuickCommandsBackend::invokeCommand(const QModelIndex& index)
{
    Konsole::FrameworkDialogScope scope(QStringLiteral("quick command errors"));
    if (!m_warnings.isEmpty()) {
        QMessageBox::warning(m_window, QStringLiteral("Shell Errors"), i18n("Please fix all the warnings before trying to run this script"));
        return;
    }
    if (!m_controller) {
        return;
    }
    const auto sourceIdx = m_filterModel->mapToSource(index);
    if (sourceIdx.parent() == m_model->invisibleRootItem()->index()) {
        return;
    }
    const auto data = m_model->itemFromIndex(sourceIdx)->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
    send(data.command);
}

void QuickCommandsBackend::runCommand(const QString& command)
{
    if (!m_hasShellCheck) {
        // check again
        m_hasShellCheck = !QStandardPaths::findExecutable(QStringLiteral("shellcheck")).isEmpty();
        if (m_hasShellCheck) {
            setWarnings(QString());
        }
    }
    if (!m_warnings.isEmpty()) {
        Konsole::FrameworkDialogScope scope(QStringLiteral("run quick command"));
        const auto choice = KMessageBox::questionTwoActions(m_window,
            i18n("There are some errors on the script, do you really want to run it?"),
            i18n("Shell Errors"),
            KGuiItem(i18nc("@action:button", "Run"), QStringLiteral("system-run")),
            KStandardGuiItem::cancel(),
            QStringLiteral("quick-commands-question"));
        if (choice == KMessageBox::SecondaryAction) {
            return;
        }
    }
    if (m_controller) {
        send(command);
    }
}

void QuickCommandsBackend::runShellCheck(const QString& command)
{
    if (!m_hasShellCheck) {
        return;
    }
    QTemporaryFile file;
    if (!file.open()) {
        return;
    }
    QTextStream ts(&file);
    ts << "#!/bin/bash\n";
    ts << command;
    ts.flush();
    file.close();

    QProcess process;
    process.start(QStringLiteral("shellcheck"), {file.fileName()});
    process.waitForFinished();

    const QString errorString = QString::fromLocal8Bit(process.readAllStandardOutput());
    m_warningsTitle = errorString.isEmpty() ? i18n("Warnings") : i18n("Warnings (*)");
    m_warnings = errorString;
    Q_EMIT warningsChanged();
}

void QuickCommandsBackend::triggerDelete(const QModelIndex& index)
{
    const QString text = index.data(Qt::DisplayRole).toString();
    const QString dialogMessage = m_filterModel->rowCount(index) ? i18n("You are about to delete the group %1,\n with multiple configurations, are you sure?", text)
                                                                 : i18n("You are about to delete %1, are you sure?", text);
    Konsole::FrameworkDialogScope scope(QStringLiteral("delete quick commands"));
    const int result = KMessageBox::warningTwoActions(m_window, dialogMessage, i18n("Delete Quick Commands Configurations"), KStandardGuiItem::del(), KStandardGuiItem::cancel());
    if (result != KMessageBox::PrimaryAction) {
        return;
    }
    const auto sourceIdx = m_filterModel->mapToSource(index);
    m_model->removeRow(sourceIdx.row(), sourceIdx.parent());
}

void QuickCommandsBackend::createMenu(const QModelIndex& index, const QPoint& globalPosition)
{
    if (!index.isValid()) {
        return;
    }
    const auto sourceIdx = m_filterModel->mapToSource(index);
    const bool isParent = sourceIdx.parent() == m_model->invisibleRootItem()->index();
    const QPersistentModelIndex persistent(index);
    auto* menu = new QMenu(m_window);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (!isParent) {
        connect(menu->addAction(i18n("Edit")), &QAction::triggered, this, &QuickCommandsBackend::editRequested);
    } else {
        connect(menu->addAction(i18n("Rename")), &QAction::triggered, this, [this, persistent] {
            Q_EMIT renameRequested(persistent);
        });
    }
    connect(menu->addAction(i18n("Delete")), &QAction::triggered, this, [this, persistent] {
        triggerDelete(persistent);
    });
    menu->popup(globalPosition);
}

#include "moc_quickcommandsbackend.cpp"
