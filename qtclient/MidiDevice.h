/*
    Copyright (C) 2012 Stefan Hajnoczi <stefanha@gmail.com>

    Wahjam is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Wahjam is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wahjam; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _MIDIDEVICE_H_
#define _MIDIDEVICE_H_

#include <QObject>
#include "ChatOutput.h"
#include "NJClient.h"
#include <alsa/asoundlib.h>


/**
 * A custom widget to show beats
 *
 * Beats are grouped into intervals.  Each interval has a fixed number of
 * beats.  This widget shows the current beat similar to a progress bar widget.
 */
class MidiDevice : public QObject
{
       Q_OBJECT
    public:
          MidiDevice(QObject *parent = 0,ChatOutput *chatOutput_ = 0);
          void setNJClient(NJClient *client_);
           void start() ;
           void stopRelay() ;
    protected:
          ChatOutput *chatOutput;
          NJClient *client;
private:
             char *device_in = NULL;
                int err;
                int stop = 0;

            char *device_out = NULL;
              char *node_in = NULL;
              char *node_out = NULL;
 };

#endif /* _MIDIDEVICE_H_ */
