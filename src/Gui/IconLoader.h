/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef TROJITA_GUI_ICONLOADER_H
#define TROJITA_GUI_ICONLOADER_H

#include <QIcon>

namespace Gui
{

/** @short Wrapper around the QIcon::fromTheme with sane fallback
 *
 * The issue with the QIcon::fromTheme is that it does not realy work on non-X11
 * platforms, unless you ship your own theme index and specify your theme name.
 * In Trojitá, we do not really want to hardcode anything, because the
 * preference is to use whatever is available from the current theme, but *also*
 * provide an icon as a fallback. And that is exactly why this function is here.
 *
 * It is implemented inline in order to prevent a dependency from the Imap lib
 * into the Gui lib.
 * */
inline QIcon loadIcon(const QString &name)
{
    // A QIcon(QString) constructor creates non-null QIcons, even though the resulting icon will
    // not ever return a valid and usable pixmap. This means that we have to actually look at the
    // icon's pixmap to find out what to return.
    // If we do not do that, the GUI shows empty pixmaps instead of a text fallback, which is
    // clearly suboptimal.
    QIcon res = QIcon::fromTheme(name, QIcon(QString::fromAscii(":/icons/%1").arg(name)));
    if (res.pixmap(QSize(16, 16)).isNull()) {
        return QIcon();
    } else {
        return res;
    }
}

}

#endif // TROJITA_GUI_ICONLOADER_H
