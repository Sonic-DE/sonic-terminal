/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerbackend.h"

#include "sshconfigurationdata.h"
#include "sshmanagerfiltermodel.h"
#include "sshmanagermodel.h"
#include "sshmanagerplugin.h"

#include "MainWindow.h"
#include "profile/ProfileModel.h"
#include "quick/FrameworkDialogScope.h"
#include "session/SessionController.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>

namespace {
SSHConfigurationData configuration(const QVariantMap& fields)
{
    SSHConfigurationData data;
    data.host = fields.value(QStringLiteral("host")).toString().trimmed();
    data.name = fields.value(QStringLiteral("name")).toString().trimmed();
    data.port = fields.value(QStringLiteral("port")).toString().trimmed();
    data.sshKey = fields.value(QStringLiteral("sshKey")).toString().trimmed();
    data.profileName = fields.value(QStringLiteral("profileName")).toString().trimmed();
    data.username = fields.value(QStringLiteral("username")).toString().trimmed();
    data.useSshConfig = fields.value(QStringLiteral("useSshConfig")).toBool();
    data.importedFromSshConfig = fields.value(QStringLiteral("imported")).toBool();
    return data;
}
}

SSHManagerBackend::SSHManagerBackend(SSHManagerModel* model, Konsole::MainWindow* window, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_filterModel(new SSHManagerFilterModel(this))
    , m_window(window)
{
    m_filterModel->setSourceModel(model);
    connect(model, &QAbstractItemModel::rowsInserted, this, &SSHManagerBackend::foldersChanged);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &SSHManagerBackend::foldersChanged);
    connect(model, &QAbstractItemModel::dataChanged, this, &SSHManagerBackend::foldersChanged);
    connect(Konsole::ProfileModel::instance(), &Konsole::ProfileModel::rowsRemoved, this, &SSHManagerBackend::profilesChanged);
    connect(Konsole::ProfileModel::instance(), &Konsole::ProfileModel::rowsInserted, this, &SSHManagerBackend::profilesChanged);

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("sshplugin"));
    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_H);
    m_shortcut = QKeySequence(settings.value(QStringLiteral("ssh_shortcut"), def.toString()).toString());
}

SSHManagerBackend::~SSHManagerBackend() = default;

QAbstractItemModel* SSHManagerBackend::model() const
{
    return m_filterModel;
}

QStringList SSHManagerBackend::folders() const
{
    return m_model->folders();
}

QStringList SSHManagerBackend::profiles() const
{
    QStringList names{i18n("Don't Change")};
    auto* model = Konsole::ProfileModel::instance();
    for (int i = 0, end = model->rowCount(QModelIndex()); i < end; i++) {
        names.append(model->data(model->index(i, Konsole::ProfileModel::Column::PROFILE), Qt::DisplayRole).toString());
    }
    return names;
}

bool SSHManagerBackend::manageProfile() const
{
    return m_model->getManageProfile();
}

void SSHManagerBackend::setManageProfile(bool manage)
{
    m_model->setManageProfile(manage);
    Q_EMIT manageProfileChanged();
}

QKeySequence SSHManagerBackend::shortcut() const
{
    return m_shortcut;
}

void SSHManagerBackend::setShortcut(const QKeySequence& shortcut)
{
    if (m_shortcut != shortcut) {
        m_shortcut = shortcut;
        Q_EMIT quickAccessShortcutChanged(shortcut);
    }
}

void SSHManagerBackend::setCurrentController(Konsole::SessionController* controller)
{
    m_controller = controller;
    m_model->setSessionController(controller);
}

void SSHManagerBackend::setFilter(const QString& text)
{
    m_filterModel->setFilterRegularExpression(text);
    m_filterModel->invalidate();
}

void SSHManagerBackend::setInvertFilter(bool invert)
{
    m_filterModel->setInvertFilter(invert);
}

bool SSHManagerBackend::canEdit(const QModelIndex& index) const
{
    return m_filterModel->flags(index) & Qt::ItemIsEditable;
}

QVariantMap SSHManagerBackend::entry(const QModelIndex& index) const
{
    const auto sourceIdx = m_filterModel->mapToSource(index);
    if (sourceIdx.parent() == m_model->invisibleRootItem()->index()) {
        return {};
    }
    const auto data = m_model->itemFromIndex(sourceIdx)->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
    return {{QStringLiteral("name"), data.name},
        {QStringLiteral("host"), data.host},
        {QStringLiteral("port"), data.port},
        {QStringLiteral("sshKey"), data.sshKey},
        {QStringLiteral("username"), data.username},
        {QStringLiteral("profileName"), data.profileName},
        {QStringLiteral("useSshConfig"), data.useSshConfig},
        {QStringLiteral("imported"), data.importedFromSshConfig}};
}

QVariantMap SSHManagerBackend::rowActions(const QModelIndex& index) const
{
    const auto sourceIdx = m_filterModel->mapToSource(index);
    if (sourceIdx.parent() == m_model->invisibleRootItem()->index()) {
        const bool sshConfig = sourceIdx.data(Qt::DisplayRole).toString() == i18n("SSH Config");
        return {{QStringLiteral("canEdit"), false},
            {QStringLiteral("canDelete"), !sshConfig},
            {QStringLiteral("deleteToolTip"), sshConfig ? i18n("Cannot delete this folder") : i18n("Delete folder and all of its contents")}};
    }
    const auto data = m_model->itemFromIndex(sourceIdx)->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
    return {{QStringLiteral("canEdit"), true},
        {QStringLiteral("canDelete"), !data.importedFromSshConfig},
        {QStringLiteral("deleteToolTip"), data.importedFromSshConfig ? i18n("You can't delete an automatically added entry.") : i18n("Delete selected entry")}};
}

QString SSHManagerBackend::checkFields(const QVariantMap& fields) const
{
    bool error = false;
    QString errorString = QStringLiteral("<ul>");
    const QString li = QStringLiteral("<li>");
    const QString il = QStringLiteral("</li>");
    const auto missing = [&error, &errorString, &li, &il](const QString& text) {
        error = true;
        errorString += li + text + il;
    };
    const bool hasKeyOrUser = !fields.value(QStringLiteral("sshKey")).toString().isEmpty() || !fields.value(QStringLiteral("username")).toString().isEmpty();

    if (fields.value(QStringLiteral("host")).toString().isEmpty()) {
        missing(i18n("Missing Hostname"));
    }
    if (fields.value(QStringLiteral("name")).toString().isEmpty()) {
        missing(i18n("Missing Name"));
    }
    if (fields.value(QStringLiteral("useSshConfig")).toBool()) {
        // An imported entry's username is read only: nothing to complain about.
        if (!fields.value(QStringLiteral("imported")).toBool() && hasKeyOrUser) {
            missing(i18n("If Use Ssh Config is set, do not specify sshkey or username."));
        }
    } else if (!hasKeyOrUser) {
        missing(i18n("At least Username or SSHKey must be set"));
    }
    if (fields.value(QStringLiteral("folder")).toString().isEmpty()) {
        missing(i18n("Missing Folder"));
    }
    if (fields.value(QStringLiteral("profileName")).toString().isEmpty()) {
        missing(i18n("An SSH session must have a profile"));
    }
    return error ? errorString + QStringLiteral("</ul>") : QString();
}

void SSHManagerBackend::addEntry(const QVariantMap& fields)
{
    m_model->addChildItem(configuration(fields), fields.value(QStringLiteral("folder")).toString());
}

void SSHManagerBackend::saveEdit(const QModelIndex& index, const QVariantMap& fields)
{
    m_model->editChildItem(configuration(fields), m_filterModel->mapToSource(index));
}

void SSHManagerBackend::triggerDelete(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    const bool folder = m_filterModel->rowCount(index) != 0;
    const QString text = index.data(Qt::DisplayRole).toString();
    const QString dialogMessage = folder ? i18n("You are about to delete the folder %1,\n with multiple SSH Configurations, are you sure?", text)
                                         : i18n("You are about to delete %1, are you sure?", text);
    const QString dontAskAgainKey = folder ? QStringLiteral("remove_ssh_folder") : QStringLiteral("remove_ssh_config");

    Konsole::FrameworkDialogScope scope(QStringLiteral("delete ssh configurations"));
    const int result = KMessageBox::warningTwoActions(m_window,
        dialogMessage,
        i18nc("@title:window", "Delete SSH Configurations"),
        KStandardGuiItem::del(),
        KStandardGuiItem::cancel(),
        dontAskAgainKey);
    if (result == KMessageBox::ButtonCode::SecondaryAction) {
        return;
    }
    m_model->removeIndex(m_filterModel->mapToSource(index));
}

void SSHManagerBackend::createMenu(const QModelIndex& index, const QPoint& globalPosition)
{
    if (!index.isValid() || index.data(Qt::DisplayRole) == i18n("SSH Config")) {
        return;
    }
    const auto sourceIdx = m_filterModel->mapToSource(index);
    if (sourceIdx.parent() != m_model->invisibleRootItem()->index()) {
        const auto data = m_model->itemFromIndex(sourceIdx)->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
        if (data.importedFromSshConfig) {
            return;
        }
    }
    const QPersistentModelIndex persistent(index);
    auto* menu = new QMenu(m_window);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto* action = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Delete"));
    connect(action, &QAction::triggered, this, [this, persistent] {
        triggerDelete(persistent);
    });
    menu->popup(globalPosition);
}

void SSHManagerBackend::connectTo(const QModelIndex& index)
{
    SSHManagerPlugin::requestConnection(m_filterModel, m_model, m_controller, index);
}

void SSHManagerBackend::connectInNewTab(const QModelIndex& index)
{
    if (!m_controller || m_filterModel->mapToSource(index).parent() == m_model->invisibleRootItem()->index()) {
        return;
    }
    m_window->newTab();
    SSHManagerPlugin::requestConnection(m_filterModel, m_model, m_controller, index);
}

QString SSHManagerBackend::chooseSshKey()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    Konsole::FrameworkDialogScope scope(QStringLiteral("ssh key"));
    return QFileDialog::getOpenFileName(m_window, i18n("SSH Key"), homeFolder + QStringLiteral("/.ssh"));
}

#include "moc_sshmanagerbackend.cpp"
