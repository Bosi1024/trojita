/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#ifndef SIMPLEPARTWIDGET_H
#define SIMPLEPARTWIDGET_H

#include "AbstractPartWidget.h"
#include "EmbeddedWebView.h"
#include "Composer/PlainTextFormatter.h"
#include <QAction>
#include <QFile>

class QModelIndex;
class QNetworkReply;

namespace Imap
{
namespace Network
{
class FileDownloadManager;
class MsgPartNetAccessManager;
}
}

namespace Gui
{

/** @short Widget that handles display of primitive message parts

More complicated parts are handled by other widgets. Role of this one is to
simply render data that can't be broken down to more trivial pieces.
*/

class SimplePartWidget : public EmbeddedWebView, public AbstractPartWidget
{
    Q_OBJECT
public:
    SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex);
    virtual QString quoteMe() const;
    virtual void reloadContents();
    QList<QAction *> contextMenuSpecificActions() const;
    void connectGuiInteractionEvents(QObject *guiInteractionTarget);
private slots:
    void slotTransferError(const QString &errorString);
    void slotFileNameRequested(QString *fileName);
    void slotMarkupPlainText();
private:
    QAction *saveAction;
    Imap::Network::FileDownloadManager *fileDownloadManager;
    Composer::Util::FlowedFormat flowedFormat;

    SimplePartWidget(const SimplePartWidget &); // don't implement
    SimplePartWidget &operator=(const SimplePartWidget &); // don't implement
};

}

#endif // SIMPLEPARTWIDGET_H
