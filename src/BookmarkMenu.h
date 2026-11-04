/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKMENU_H
#define BOOKMARKMENU_H

// KDE
#include <KBookmarkMenu>

// Konsole
#include "konsoleprivate_export.h"

class KActionCollection;

/* Hackish hack to mitigate a broken behavior of KBookmarkMenu.
 * slotAddBookmark accepts duplicates and it's fragile code,
 * that thing really deserves a rewrite.
 * the easiest way is to "hijack" it's protected method to public
 * and just cast around.
 */
class KONSOLEPRIVATE_EXPORT BookmarkMenu : public KBookmarkMenu
{
    Q_OBJECT

public:
    BookmarkMenu(KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collection);
    // A folder's submenu.
    BookmarkMenu(KBookmarkManager* mgr, KBookmarkOwner* owner, QMenu* parentMenu, const QString& parentAddress);

protected:
    void refill() override;
    QAction* actionForBookmark(const KBookmark& bm) override;
    QMenu* contextMenu(QAction* action) override;

private Q_SLOTS:
    void maybeAddBookmark();

private:
    // KBookmarkMenu's dialogs (add bookmark, bookmark tabs, new folder) inside a FrameworkDialogScope.
    void scopeDialogs();
};

#endif // BOOKMARKMENU_H
