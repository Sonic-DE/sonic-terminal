/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "BookmarkMenu.h"

// Konsole
#include "quick/FrameworkDialogScope.h"

// KDE
#include <KActionCollection>
#include <KBookmark>
#include <KBookmarkActionMenu>
#include <KBookmarkManager>
#include <KBookmarkOwner>

// Qt
#include <QAction>
#include <QMenu>

#include <algorithm> // std::any_of

BookmarkMenu::BookmarkMenu(KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collection)
    : KBookmarkMenu(mgr, owner, parentMenu)
{
    QAction *bookmarkAction;
    collection->addActions(parentMenu->actions());

    bookmarkAction = addBookmarkAction();

    Q_ASSERT(bookmarkAction);

    // We need to hijack the action - note this only hijacks top-level action
    scopeDialogs();

#ifndef Q_OS_MACOS // not needed on this platform, Cmd+B (shortcut) is different to Ctrl+B (^B in terminal)
    // replace Ctrl+B shortcut for bookmarks only if user hasn't already
    // changed the shortcut; however, if the user changed it to Ctrl+B
    // this will still get changed to Ctrl+Shift+B
    if (bookmarkAction->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_B)) {
        collection->setDefaultShortcut(bookmarkAction, Qt::CTRL | Qt::SHIFT | Qt::Key_B);
    }
#endif
}

BookmarkMenu::BookmarkMenu(KBookmarkManager* mgr, KBookmarkOwner* owner, QMenu* parentMenu, const QString& parentAddress)
    : KBookmarkMenu(mgr, owner, parentMenu, parentAddress)
{
}

void BookmarkMenu::refill()
{
    KBookmarkMenu::refill();
    // A submenu creates its actions here, the first time.
    scopeDialogs();
}

QAction* BookmarkMenu::actionForBookmark(const KBookmark& bm)
{
    if (!bm.isGroup()) {
        return KBookmarkMenu::actionForBookmark(bm);
    }
    // KBookmarkMenu::actionForBookmark() with a BookmarkMenu for the folder.
    auto* actionMenu = new KBookmarkActionMenu(bm, this);
    m_actions.append(actionMenu);
    m_lstSubMenus.append(new BookmarkMenu(manager(), owner(), actionMenu->menu(), bm.address()));
    return actionMenu;
}

QMenu* BookmarkMenu::contextMenu(QAction* action)
{
    // Its Properties and Delete dialogs open after it hides, while it still exists.
    QMenu* menu = KBookmarkMenu::contextMenu(action);
    if (menu) {
        Konsole::FrameworkDialogScope::attach(menu, QStringLiteral("bookmark context menu"));
    }
    return menu;
}

void BookmarkMenu::scopeDialogs()
{
    // Reconnecting is idempotent: the old connection to this goes first.
    const bool root = parentAddress().isEmpty();
    const std::pair<QAction*, void (BookmarkMenu::*)()> dialogs[] = {
        {addBookmarkAction(), root ? &BookmarkMenu::maybeAddBookmark : &BookmarkMenu::slotAddBookmark},
        {bookmarkTabsAsFolderAction(), &BookmarkMenu::slotAddBookmarksList},
        {newBookmarkFolderAction(), &BookmarkMenu::slotNewFolder},
        {editBookmarksAction(), &BookmarkMenu::slotEditBookmarks},
    };
    for (const auto& [action, open] : dialogs) {
        if (!action) {
            continue;
        }
        disconnect(action, &QAction::triggered, this, nullptr);
        connect(action, &QAction::triggered, this, [this, open, name = action->text()] {
            Konsole::FrameworkDialogScope scope(name);
            (this->*open)();
        });
    }
}

void BookmarkMenu::maybeAddBookmark()
{
    // Check for duplicates first
    // This only catches duplicates in top-level (ie not sub-folders);
    //  this is due to only the top-level add_bookmark is hijacked.
    const KBookmarkGroup rootGroup = manager()->root();
    const QUrl currUrl = owner()->currentUrl();
    const auto urlList = rootGroup.groupUrlList();
    if (std::any_of(urlList.constBegin(), urlList.constEnd(), [currUrl](const QUrl &url) {
            return url == currUrl;
        })) {
        return;
    }

    Konsole::FrameworkDialogScope scope(QStringLiteral("add bookmark"));
    slotAddBookmark();
}

#include "moc_BookmarkMenu.cpp"
