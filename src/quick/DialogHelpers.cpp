/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DialogHelpers.h"

#include "FrameworkDialogScope.h"
#include "widgets/TabTitleFormatButton.h"

#include <KHelpClient>
#include <KIconDialog>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QColorDialog>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

namespace Konsole {
DialogHelpers::DialogHelpers(QObject* parent)
    : QObject(parent)
{
}

QVariantList DialogHelpers::standardColors()
{
    // KColorCombo's standard palette (kcolorcombo.cpp), in its order.
    static const uchar palette[][3] = {{255, 255, 255}, {192, 192, 192}, {160, 160, 160}, {128, 128, 128}, {0, 0, 0}, {255, 128, 128}, {255, 192, 128},
        {255, 255, 128}, {128, 255, 128}, {128, 255, 255}, {128, 128, 255}, {255, 128, 255}, {255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {0, 255, 0},
        {0, 255, 255}, {0, 0, 255}, {255, 0, 255}, {128, 0, 0}, {128, 64, 0}, {128, 128, 0}, {0, 128, 0}, {0, 128, 128}, {0, 0, 128}, {128, 0, 128}};
    QVariantList colors;
    for (const auto& entry : palette) {
        colors.append(QColor(entry[0], entry[1], entry[2]));
    }
    return colors;
}

QColor DialogHelpers::noneColor()
{
    QColor color = QGuiApplication::palette().base().color();
    color.setAlpha(0);
    return color;
}

QVariant DialogHelpers::chooseColor(const QColor& initial, QQuickItem* from)
{
    // As QColorDialog::getColor(): the colour set once the dialog is built.
    FrameworkDialogScope scope(QStringLiteral("color"));
    QColorDialog dialog;
    dialog.setCurrentColor(initial);
    if (from && from->window()) {
        dialog.winId();
        dialog.windowHandle()->setTransientParent(from->window());
    }
    return dialog.exec() == QDialog::Accepted ? QVariant(dialog.selectedColor()) : QVariant();
}

QString DialogHelpers::chooseTitleFormat(int context, QQuickItem* below)
{
    QMenu menu;
    menu.addActions(TabTitleFormatButton::elementActions(static_cast<Session::TabTitleContext>(context), &menu));
    const QPoint position = below ? below->mapToGlobal(QPointF(0, below->height())).toPoint() : QCursor::pos();
    QAction* chosen = menu.exec(position);
    return chosen ? chosen->data().toString() : QString();
}

QString DialogHelpers::temporaryDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

QString DialogHelpers::cacheDirectory()
{
    // As TemporaryFilesSettings: a long home path would make the page wide.
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (cachePath.startsWith(homePath)) {
        cachePath.replace(0, homePath.length(), QStringLiteral("~"));
    }
    return cachePath;
}

QString DialogHelpers::chooseDirectory(const QString& start, QQuickItem* from)
{
    FrameworkDialogScope scope(QStringLiteral("directory"));
    QFileDialog dialog(nullptr, QString(), start);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    if (from && from->window()) {
        dialog.winId();
        dialog.windowHandle()->setTransientParent(from->window());
    }
    return dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty() ? dialog.selectedFiles().constFirst() : QString();
}

void DialogHelpers::enableAllMessages()
{
    KMessageBox::enableAllMessages();
}

void DialogHelpers::showHelp()
{
    KHelpClient::invokeHelp();
}

namespace {
    void setTransientFor(QWidget* dialog, QQuickItem* from)
    {
        if (from && from->window()) {
            dialog->winId();
            dialog->windowHandle()->setTransientParent(from->window());
        }
    }
}

QString DialogHelpers::chooseToolBarIcon(QQuickItem* from)
{
    // KIconDialog::getIcon() as KEditToolBar calls it.
    FrameworkDialogScope scope(QStringLiteral("toolbar icon"));
    KIconDialog dialog;
    dialog.setup(KIconLoader::Toolbar, KIconLoader::Action, false, 0, false);
    dialog.setWindowTitle(i18nd("kxmlgui6", "Change Icon"));
    setTransientFor(&dialog, from);
    return dialog.openDialog();
}

bool DialogHelpers::confirmToolBarReset(QQuickItem* from)
{
    // KMessageBox::warningContinueCancel()'s box, as KEditToolBar's Defaults asks.
    FrameworkDialogScope scope(QStringLiteral("toolbar reset"));
    auto* dialog = new QDialog;
    dialog->setWindowTitle(i18nd("kxmlgui6", "Reset Toolbars"));
    dialog->setObjectName(QStringLiteral("warningYesNo"));
    auto* buttonBox = new QDialogButtonBox(dialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Yes), KGuiItem(i18nd("kxmlgui6", "Reset")));
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::No), KStandardGuiItem::cancel());
    dialog->setModal(true);
    setTransientFor(dialog, from);
    const int result = KMessageBox::createKMessageBox(dialog,
        buttonBox,
        QMessageBox::Warning,
        i18nd("kxmlgui6",
            "Do you really want to reset all toolbars of this application to their default? The changes will be applied immediately."),
        QStringList(),
        QString(),
        nullptr,
        KMessageBox::Notify);
    return result == QDialogButtonBox::Yes;
}
}

#include "moc_DialogHelpers.cpp"
