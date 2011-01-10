/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "OpenConnectionTask.h"
#include <QTimer>

namespace Imap {
namespace Mailbox {

OpenConnectionTask::OpenConnectionTask( Model* _model ) :
    ImapTask( _model ), waitingForGreetings(true), gotPreauth(false)
{
    parser = new Parser( model, model->_socketFactory->create(), ++model->lastParserId );
    Model::ParserState parserState = Model::ParserState( parser );
    connect( parser, SIGNAL(responseReceived(Imap::Parser*)), model, SLOT(responseReceived(Imap::Parser*)) );
    connect( parser, SIGNAL(disconnected(Imap::Parser*,const QString)), model, SLOT(slotParserDisconnected(Imap::Parser*,const QString)) );
    connect( parser, SIGNAL(connectionStateChanged(Imap::Parser*,Imap::ConnectionState)), model, SLOT(handleSocketStateChanged(Imap::Parser*,Imap::ConnectionState)) );
    connect( parser, SIGNAL(sendingCommand(Imap::Parser*,QString)), model, SLOT(parserIsSendingCommand(Imap::Parser*,QString)) );
    connect( parser, SIGNAL(parseError(Imap::Parser*,QString,QString,QByteArray,int)), model, SLOT(slotParseError(Imap::Parser*,QString,QString,QByteArray,int)) );
    connect( parser, SIGNAL(lineReceived(Imap::Parser*,QByteArray)), model, SLOT(slotParserLineReceived(Imap::Parser*,QByteArray)) );
    connect( parser, SIGNAL(lineSent(Imap::Parser*,QByteArray)), model, SLOT(slotParserLineSent(Imap::Parser*,QByteArray)) );
    if ( model->_startTls ) {
        startTlsCmd = parser->startTls();
        parserState.commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
        emit model->activityHappening( true );
    }
    parserState.activeTasks.append( this );
    model->_parsers[ parser ] = parserState;
}

OpenConnectionTask::OpenConnectionTask( Model* _model, void* dummy): ImapTask( _model )
{
    Q_UNUSED( dummy );
}

void OpenConnectionTask::perform()
{
    // nothing should happen here
}

/** @short Process the "state" response originating from the IMAP server */
bool OpenConnectionTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( waitingForGreetings ) {
        handleInitialResponse( ptr, resp );
        return true;
    }

    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == capabilityCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( capabilityCmd, Model::Task::CAPABILITY );
        if ( resp->kind == Responses::OK ) {
            if ( gotPreauth ) {
                // The greetings indicated that we're already in the auth state, and now we
                // know capabilities, too, so we're done here
                _completed();
            } else {
                // We want to log in, but we might have to STARTTLS before
                if ( model->accessParser( ptr ).capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                    qDebug() << "Can't login yet, trying STARTTLS";
                    // ... and we are forbidden from logging in, so we have to try the STARTTLS
                    startTlsCmd = ptr->startTls();
                    model->accessParser( ptr ).commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                    emit model->activityHappening( true );
                } else {
                    // Apparently no need for STARTTLS and we are free to login
                    loginCmd = model->performAuthentication( ptr );
                }
            }
        } else {
            // FIXME: Tasks API error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == loginCmd ) {
        // The LOGIN command is finished, and we know capabilities already
        Q_ASSERT( model->accessParser( ptr ).capabilitiesFresh );
        IMAP_TASK_ENSURE_VALID_COMMAND( loginCmd, Model::Task::LOGIN );
        if ( resp->kind == Responses::OK ) {
            model->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            _completed();
        } else {
            // FIXME: error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == startTlsCmd ) {
        // So now we've got a secure connection, but we will have to login. Additionally,
        // we are obliged to forget any capabilities.
        model->accessParser( ptr ).capabilitiesFresh = false;
        // FIXME: why do I have to comment that out?
        //IMAP_TASK_ENSURE_VALID_COMMAND( startTlsCmd, Model::Task::STARTTLS );
        QMap<CommandHandle, Model::Task>::iterator command = model->accessParser( ptr ).commandMap.find( startTlsCmd );
        if ( resp->kind == Responses::OK ) {
            capabilityCmd = ptr->capability();
            model->accessParser( ptr ).commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
            emit model->activityHappening( true );
        } else {
            // Well, this place is *very* bad -- we're in the middle of a responseRecevied(), Model is iterating over active tasks
            // and we really want to emit that connectionError signal here. The problem is that a typical reaction from the GUI is
            // to show a dialog box, which unfortunately invokes the event loop, which would in turn handle the socketDisconnected()
            // (because the real SSL operation for switching on the encryption failed, too).
            // Let's just throw an exception and let the Model deal with it.
            throw StartTlsFailed( tr("Can't establish a secure connection to the server (STARTTLS failed: %1). Refusing to proceed.").arg( resp->message ).toUtf8().constData() );
        }
        //IMAP_TASK_CLEANUP_COMMAND;
        if ( command != model->accessParser( ptr ).commandMap.end() )
            model->accessParser( ptr ).commandMap.erase( command );
        model->parsersMightBeIdling();
        return true;
    } else {
        return false;
    }
}

/** @short Helper for dealing with the very first response from the server */
void OpenConnectionTask::handleInitialResponse( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    waitingForGreetings = false;
    if ( ! resp->tag.isEmpty() ) {
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/PREAUTH, but got tagged response instead",
                *resp );
    }

    using namespace Imap::Responses;
    switch ( resp->kind ) {
    case PREAUTH:
        {
            // Cool, we're already authenticated. Now, let's see if we have to issue CAPABILITY or if we already know that
            gotPreauth = true;
            model->changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
            if ( ! model->accessParser( ptr ).capabilitiesFresh ) {
                capabilityCmd = ptr->capability();
                model->accessParser( ptr ).commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
            } else {
                _completed();
            }
            break;
        }
    case OK:
        if ( model->_startTls ) {
            // The STARTTLS command is already queued -> no need to issue it once again
        } else {
            // The STARTTLS surely has not been issued yet
            if ( ! model->accessParser( ptr ).capabilitiesFresh ) {
                capabilityCmd = ptr->capability();
                model->accessParser( ptr ).commandMap[ capabilityCmd ] = Model::Task( Model::Task::CAPABILITY, 0 );
                emit model->activityHappening( true );
            } else if ( model->accessParser( ptr ).capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                qDebug() << "Can't login yet, trying STARTTLS";
                // ... and we are forbidden from logging in, so we have to try the STARTTLS
                startTlsCmd = ptr->startTls();
                model->accessParser( ptr ).commandMap[ startTlsCmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                emit model->activityHappening( true );
            } else {
                // Apparently no need for STARTTLS and we are free to login
                loginCmd = model->performAuthentication( ptr );
            }
        }
        break;
    case BYE:
        model->changeConnectionState( ptr, CONN_STATE_LOGOUT );
        // FIXME: Tasks error handling
        break;
    case BAD:
        // If it was an ALERT, we've already warned the user
        if ( resp->respCode != ALERT ) {
            emit model->alertReceived( tr("The server replied with the following BAD response:\n%1").arg( resp->message ) );
        }
        // FIXME: Tasks error handling
        break;
    default:
        throw Imap::UnexpectedResponseReceived(
                "Waiting for initial OK/BYE/BAD/PREAUTH, but got this instead",
                *resp );
    }
}

}
}
