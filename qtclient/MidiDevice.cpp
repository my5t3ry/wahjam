/*
    Copyright (C) 2012 Stefan Hajnoczi <stefanha@gmail.com>

    Based on a demo by Ikkei Shimomura (tea) <Ikkei.Shimomura@gmail.com>

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

#include "MidiDevice.h"

MidiDevice::MidiDevice(QObject *parent, ChatOutput *chatOutput_)
: QObject(parent),chatOutput(chatOutput_)
{

snd_rawmidi_t *handle_in = 0,*handle_out = 0;
  int mode = SND_RAWMIDI_NONBLOCK;


 device_in = "virtual";
  device_out = "virtual";
    err = snd_rawmidi_open(&handle_in,NULL,device_in,mode);
          if (err) {
              fprintf(stderr,"snd_rawmidi_open %s failed: %d\n",device_in,err);
          }
    err = snd_rawmidi_open(NULL,&handle_out,device_out,mode);
           if (err) {
               fprintf(stderr,"snd_rawmidi_open %s failed: %d\n",device_out,err);
           }
  chatOutput->addInfoMessage(tr("Added Midi Device:%1").arg("test"));
}


void MidiDevice::setNJClient(NJClient *client_) {
    client = client_;

}
void MidiDevice::stopRelay() {

}
void MidiDevice::start() {
  }
