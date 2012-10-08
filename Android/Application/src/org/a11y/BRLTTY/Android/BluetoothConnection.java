/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2012 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

package org.a11y.BRLTTY.Android;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.util.UUID;

import android.util.Log;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;

public class BluetoothConnection {
  private final String LOG_TAG = this.getClass().getName();

  protected static final UUID SERIAL_PROFILE_UUID = UUID.fromString(
    "00001101-0000-1000-8000-00805F9B34FB"
  );

  protected static final BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
  protected BluetoothDevice bluetoothDevice;
  protected String bluetoothAddress;

  protected BluetoothSocket bluetoothSocket = null;
  protected OutputStream outputStream = null;
  protected InputThread inputThread = null;

  private class InputThread extends Thread {
    private final String LOG_TAG = this.getClass().getName();

    boolean stop = false;
    InputStream inputStream;
    int pipeDescriptor;

    InputThread (InputStream inputStream, int pipeDescriptor) {
      super("bluetooth-input-" + bluetoothAddress);
      this.inputStream = inputStream;
      this.pipeDescriptor = pipeDescriptor;
    }

    @Override
    public void run () {
      try {
        File pipeFile = new File("/proc/self/fd/" + pipeDescriptor);
        OutputStream pipeStream = new FileOutputStream(pipeFile);
        byte[] buffer = new byte[0X80];

        while (!stop) {
          int byteCount = inputStream.read(buffer);

          if (byteCount > 0) {
            pipeStream.write(buffer, 0, byteCount);
            pipeStream.flush();
          } else if (byteCount < 0) {
            Log.d(LOG_TAG, "Bluetooth input end: " + bluetoothAddress);
            break;
          }
        }

        pipeStream.close();
        inputStream.close();
      } catch (Throwable cause) {
        Log.e(LOG_TAG, "Bluetooth input failed: " + bluetoothAddress, cause);
      }
    }
  }

  public BluetoothConnection (long deviceAddress, byte channelNumber, int inputPipe
  ) throws Throwable {
    byte[] hardwareAddress = new byte[6];
    {
      int i = hardwareAddress.length;

      while (i > 0) {
        hardwareAddress[--i] = (byte)(deviceAddress & 0XFF);
        deviceAddress >>= 8;
      }
    }

    bluetoothDevice = bluetoothAdapter.getRemoteDevice(hardwareAddress);
    bluetoothAddress = bluetoothDevice.getAddress();

    try {
      bluetoothSocket = bluetoothDevice.createInsecureRfcommSocketToServiceRecord(SERIAL_PROFILE_UUID);
      bluetoothSocket.connect();

      outputStream = bluetoothSocket.getOutputStream();
      inputThread = new InputThread(bluetoothSocket.getInputStream(), inputPipe);
      inputThread.start();
    } catch (Throwable cause) {
      Log.e(LOG_TAG, "Bluetooth connect failed: " + bluetoothAddress, cause);

      if (inputThread != null) {
        inputThread.stop = true;
      }

      if (bluetoothSocket != null) {
        if (bluetoothSocket.isConnected()) {
          bluetoothSocket.close();
        }
      }

      throw cause;
    }
  }

  public void close () {
    if (inputThread != null) {
      inputThread.stop = true;
      inputThread = null;
    }

    if (bluetoothSocket != null) {
      try {
        bluetoothSocket.close();
      } catch (IOException exception) {
        Log.w(LOG_TAG, "Bluetooth close failed: " + bluetoothAddress, exception);
      }

      bluetoothSocket = null;
    }
  }

  public boolean write (byte[] bytes) {
    try {
      outputStream.write(bytes);
      outputStream.flush();
      return true;
    } catch (IOException exception) {
      Log.e(LOG_TAG, "Bluetooth write failed: " + bluetoothAddress, exception);
    }

    return false;
  }
}