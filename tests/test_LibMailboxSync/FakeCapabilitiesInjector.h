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

#ifndef FAKECAPABILITIESINJECTOR_H
#define FAKECAPABILITIESINJECTOR_H

#include "Imap/Model/Model.h"

/** @short Class for persuading the model that the parser supports certain capabilities */
class FakeCapabilitiesInjector
{
public:
    FakeCapabilitiesInjector(Imap::Mailbox::Model *_model): model(_model)
    {}

    /** @short Add the specified capability to the list of capabilities "supported" by the server */
    void injectCapability(const QString& cap)
    {
        Q_ASSERT(!model->m_parsers.isEmpty());
        QStringList existingCaps = model->capabilities();
        if (!existingCaps.contains(QLatin1String("IMAP4REV1"))) {
            existingCaps << QLatin1String("IMAP4rev1");
        }
        model->updateCapabilities( model->m_parsers.begin().key(), existingCaps << cap );
    }
private:
    Imap::Mailbox::Model *model;
};

#endif // FAKECAPABILITIESINJECTOR_H
