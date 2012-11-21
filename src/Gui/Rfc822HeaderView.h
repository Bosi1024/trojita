/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GUI_RFC822HEADERVIEW_H
#define GUI_RFC822HEADERVIEW_H

#include <QLabel>
#include <QPersistentModelIndex>

class QModelIndex;

namespace Gui
{

class Rfc822HeaderView : public QLabel
{
    Q_OBJECT
public:
    Rfc822HeaderView(QWidget *parent, QModelIndex index);
private slots:
    void handleDataChanged(const QModelIndex &, const QModelIndex &);
    void setCorrectText();
private:
    QPersistentModelIndex index;

    Rfc822HeaderView(const Rfc822HeaderView &); // don't implement
    Rfc822HeaderView &operator=(const Rfc822HeaderView &); // don't implement
};

}

#endif // GUI_RFC822HEADERVIEW_H
