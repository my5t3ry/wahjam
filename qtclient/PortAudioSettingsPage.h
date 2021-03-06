/*
    Copyright (C) 2012-2020 Stefan Hajnoczi <stefanha@gmail.com>

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

#ifndef _PORTAUDIOSETTINGSPAGE_H_
#define _PORTAUDIOSETTINGSPAGE_H_

#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include "LockableSettingsPage.h"

class PortAudioSettingsPage : public LockableSettingsPage
{
  Q_OBJECT
  Q_PROPERTY(QString hostAPI READ hostAPI WRITE setHostAPI)
  Q_PROPERTY(QString inputDevice READ inputDevice WRITE setInputDevice)
  Q_PROPERTY(bool unmuteLocalChannels READ unmuteLocalChannels WRITE setUnmuteLocalChannels)
  Q_PROPERTY(QList<QVariant> inputChannels READ inputChannels WRITE setInputChannels)
  Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice)
  Q_PROPERTY(QList<QVariant> outputChannels READ outputChannels WRITE setOutputChannels)
  Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate)
  Q_PROPERTY(double latency READ latency WRITE setLatency)
  Q_PROPERTY(bool detectLoudNoises READ detectLoudNoises WRITE setDetectLoudNoises)

public:
  PortAudioSettingsPage(QWidget *parent = 0);
  QString hostAPI() const;
  void setHostAPI(const QString &name);
  QString inputDevice() const;
  void setInputDevice(const QString &name);
  bool unmuteLocalChannels() const;
  void setUnmuteLocalChannels(bool unmute);
  QList<QVariant> inputChannels() const;
  void setInputChannels(const QList<QVariant> &channels);
  QString outputDevice() const;
  void setOutputDevice(const QString &name);
  QList<QVariant> outputChannels() const;
  void setOutputChannels(const QList<QVariant> &channels);
  double sampleRate() const;
  void setSampleRate(double sampleRate);
  double latency() const;
  void setLatency(double latency);
  bool detectLoudNoises() const;
  void setDetectLoudNoises(bool enable);

private slots:
  void inputDeviceIndexChanged(int index);
  void outputDeviceIndexChanged(int index);
  void sampleRateIndexChanged(int index);
  void hostAPIIndexChanged(int index);

private:
  QComboBox *hostAPIList;
  QComboBox *inputDeviceList;
  QCheckBox *unmuteLocalChannelsBox;
  QListWidget *inputChannelsList;
  QComboBox *outputDeviceList;
  QListWidget *outputChannelsList;
  QComboBox *sampleRateList;
  QComboBox *latencyList;
  QCheckBox *detectLoudNoisesBox;

  QList<QVariant> getChannelsList(QListWidget *list) const;
  void setChannelsList(QListWidget *list, const QList<QVariant> &channels);
  void populateChannelsList(QComboBox *deviceList, QListWidget *channelsList,
                            bool isInput);
  void populateHostAPIList();
  void populateDeviceList();
  void populateSampleRateList();
  void populateLatencyList();
  void autoselectHostAPI();
  void autoselectDevice();
  void autoselectSampleRate();
  void autoselectLatency();
};

#endif /* _PORTAUDIOSETTINGSPAGE_H_ */
