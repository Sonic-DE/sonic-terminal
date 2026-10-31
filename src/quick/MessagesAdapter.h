/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class KMessageWidget;

namespace Konsole {
class TerminalView;

/**
 * The facade's (hidden) KMessageWidgets (read-only, output suspended) as data for their QML
 * messages: where the facade's layout put them and their icon, text and buttons, their
 * colours and their show/hide animation. Buttons and links act through the real widgets.
 */
class KONSOLEPRIVATE_EXPORT MessagesAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Konsole::TerminalView* view READ view WRITE setView NOTIFY viewChanged)
    // {geometry, shown, color, parentColor, opacity, icon {shown, geometry, image},
    //  text {geometry, text, rich, wordWrap, color, linkColor, font},
    //  buttons [{geometry, iconName, iconWidth, iconHeight, autoRaise, toolTip}]}
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)

public:
    explicit MessagesAdapter(QObject* parent = nullptr);

    TerminalView* view() const;
    void setView(TerminalView* view);
    QVariantList messages() const;

    Q_INVOKABLE void clickButton(int message, int button);
    Q_INVOKABLE void activateLink(int message, const QString& link);
    Q_INVOKABLE void hoverLink(int message, const QString& link);

Q_SIGNALS:
    void viewChanged();
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QList<KMessageWidget*> widgets() const;
    void watch(KMessageWidget* widget);

    QPointer<TerminalView> _view;
    QList<QPointer<KMessageWidget>> _watched;
};
}
