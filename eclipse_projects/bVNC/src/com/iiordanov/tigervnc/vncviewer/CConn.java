/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

//
// CConn
//
// Methods on CConn are called from both the GUI thread and the thread which
// processes incoming RFB messages ("the RFB thread").  This means we need to
// be careful with synchronization here.
//
// Any access to writer() must not only be synchronized, but we must also make
// sure that the connection is in RFBSTATE_NORMAL.  We are guaranteed this for
// any code called after serverInit() has been called.  Since the DesktopWindow
// isn't created until then, any methods called only from DesktopWindow can
// assume that we are in RFBSTATE_NORMAL.

package com.iiordanov.tigervnc.vncviewer;

import java.io.IOException;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.jar.Attributes;
import java.util.jar.Manifest;
import java.net.URL;
import java.util.*;

import android.graphics.Bitmap;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.iiordanov.bVNC.AbstractConnectionBean;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.AbstractBitmapData;
import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.tigervnc.rdr.*;
import com.iiordanov.tigervnc.rfb.*;
import com.iiordanov.tigervnc.rfb.Exception;

public class CConn extends CConnection
  implements UserPasswdGetter, UserMsgBox, RfbConnectable
{
  private final static String TAG = "CConn";

  ////////////////////////////////////////////////////////////////////
  // The following methods are all called from the RFB thread

  public CConn(RemoteCanvas viewer_, java.net.Socket sock_, 
               String vncServerName, boolean reverse, AbstractConnectionBean connection)
  {
    super(viewer_);
    this.viewOnly = connection.getViewOnly();
    this.connection = connection; 
    serverHost = null; serverPort = 0; sock = sock_;
    currentEncoding = Encodings.encodingTight; lastServerEncoding = -1;
    fullColour = true; // TODO: viewer.fullColour.getValue();
    lowColourLevel = 2;
    autoSelect = false; // TODO: viewer.autoSelect.getValue();
    shared = true; // TODO: viewer.shared.getValue();
    formatChange = false;
    encodingChange = false; sameMachine = false;
    fullScreen = true; //TODO: viewer.fullScreen.getValue();
    menuKey = Keysyms.F8;
    // TODO: options = new OptionsDialog(this);
    // TODO: options.initDialog();
    // TODO: clipboardDialog = new ClipboardDialog(this);
    firstUpdate = true; pendingUpdate = false;

    setShared(shared);
    upg = this;
    msg = this;

    String encStr = "Tight"; //TODO: viewer.preferredEncoding.getValue();
    int encNum = Encodings.encodingNum(encStr);
    if (encNum != -1) {
      currentEncoding = encNum;
    }
    cp.supportsDesktopResize = true;
    cp.supportsExtendedDesktopSize = true;
    cp.supportsClientRedirect = true;
    cp.supportsDesktopRename = true;
    cp.supportsLocalCursor = false; // TODO: viewer.useLocalCursor.getValue();
    cp.customCompressLevel = true; // TODO: viewer.customCompressLevel.getValue();
    cp.compressLevel = 9; // TODO: viewer.compressLevel.getValue();
    cp.noJpeg = false; // TODO: viewer.noJpeg.getValue();
    cp.qualityLevel = 6; // TODO: viewer.qualityLevel.getValue();
    //initMenu();

    if (sock != null) {
      String name = sock.getRemoteSocketAddress()+"::"+sock.getPort();
      vlog.info("Accepted connection from "+name);
    } else {
      if (vncServerName != null) {
        serverHost = Hostname.getHost(vncServerName);
        serverPort = Hostname.getPort(vncServerName);
      } else {
        // TODO: ServerDialog dlg = new ServerDialog(options, vncServerName, this);
        //if (!dlg.showDialog()) {
        //  System.exit(1);
        //}
        serverHost = connection.getAddress();
        serverPort = connection.getPort();
      }

      try {
        sock = new java.net.Socket(serverHost, serverPort);
      } catch (java.io.IOException e) { 
        throw new Exception(e.toString());
      }
      vlog.info("connected to host "+serverHost+" port "+serverPort);
    }

    sameMachine = (sock.getLocalSocketAddress() == sock.getRemoteSocketAddress());
    try {
      sock.setTcpNoDelay(true);
      sock.setSoTimeout(0);
      setServerName(serverHost);
      jis = new JavaInStream(sock.getInputStream());
      jos = new JavaOutStream(sock.getOutputStream());
    } catch (java.net.SocketException e) {
      throw new Exception(e.toString());
    } catch (java.io.IOException e) { 
      throw new Exception(e.toString());
    }
    processSecurityTypes ();
    setStreams(jis, jos);
    initialiseProtocol();
  }

  /**
   * Processes messages from VNC server indefinitely.
   */
  public void processProtocol () {
      while (true)
          processMsg();
  }
  
  public boolean showMsgBox(int flags, String title, String text)
  {
    //StringBuffer titleText = new StringBuffer("VNC Viewer: "+title);
    return true;
  }

  // getUserPasswd() is called by the CSecurity object when it needs us to read
  // a password from the user.

  public final boolean getUserPasswd(StringBuffer user, StringBuffer passwd) {
    String title = ("VNC Authentication ["
                    +csecurity.description() + "]");
    String passwordFileStr = ""; // TODO: viewer.passwordFile.getValue();
    //PasswdDialog dlg;    

    if (user == null && passwordFileStr != "") {
      InputStream fp = null;
      try {
        fp = new FileInputStream(passwordFileStr);
      } catch(FileNotFoundException e) {
        throw new Exception("Opening password file failed");
      }
      byte[] obfPwd = new byte[256];
      try {
        fp.read(obfPwd);
        fp.close();
      } catch(IOException e) {
        throw new Exception("Failed to read VncPasswd file");
      }
      String PlainPasswd =
        VncAuth.unobfuscatePasswd(obfPwd);
      passwd.append(PlainPasswd);
      passwd.setLength(PlainPasswd.length());
      return true;
    }

    if (user == null) {
      //dlg = new PasswdDialog(title, (user == null), (passwd == null));
    } else {
      if ((passwd == null) && false /*TODO: viewer.sendLocalUsername.getValue()*/) {
         user.append((String)System.getProperties().get("user.name"));
         return true;
      }
      //dlg = new PasswdDialog(title, viewer.sendLocalUsername.getValue(), 
      //   (passwd == null));
    }
    //if (!dlg.showDialog()) return false;
    if (user != null) {
      if (false /*TODO: viewer.sendLocalUsername.getValue()*/) {
         user.append((String)System.getProperties().get("user.name"));
      } else {
         user.append(connection.getUserName());
      }
    }
    if (passwd != null)
      passwd.append(connection.getPassword());
    return true;
  }

  // CConnection callback methods

  // serverInit() is called when the serverInit message has been received.  At
  // this point we create the desktop window and display it.  We also tell the
  // server the pixel format and encodings to use and request the first update.
  public void serverInit() {
    super.serverInit();

    // If using AutoSelect with old servers, start in FullColor
    // mode. See comment in autoSelectFormatAndEncoding. 
    if (cp.beforeVersion(3, 8) && autoSelect) {
      fullColour = true;
    }

    serverPF = cp.pf();
    //desktop = new DesktopWindow(cp.width, cp.height, serverPF, this);
    //desktopEventHandler = desktop.setEventHandler(this);
    //desktop.addEventMask(KeyPressMask | KeyReleaseMask);
    //fullColourPF = desktop.getPreferredPF();
    if (!serverPF.trueColour)
      fullColour = true;
    
    //recreateViewport();
    formatChange = true; encodingChange = true;
    requestNewUpdate();
  }

  // setDesktopSize() is called when the desktop size changes (including when
  // it is set initially).
  public void setDesktopSize(int w, int h) {
    super.setDesktopSize(w,h);
    resizeFramebuffer();
  }

  // setExtendedDesktopSize() is a more advanced version of setDesktopSize()
  public void setExtendedDesktopSize(int reason, int result, int w, int h,
                                     ScreenSet layout) {
    super.setExtendedDesktopSize(reason, result, w, h, layout);

    if ((reason == screenTypes.reasonClient) &&
        (result != screenTypes.resultSuccess)) {
      vlog.error("SetDesktopSize failed: "+result);
      return;
    }

    resizeFramebuffer();
  }

  // clientRedirect() migrates the client to another host/port
  public void clientRedirect(int port, String host, 
                             String x509subject) {
    try {
      getSocket().close();
      setServerPort(port);
      sock = new java.net.Socket(host, port);
      sock.setTcpNoDelay(true);
      sock.setSoTimeout(0);
      setSocket(sock);
      vlog.info("Redirected to "+host+":"+port);
      setStreams(new JavaInStream(sock.getInputStream()),
                 new JavaOutStream(sock.getOutputStream()));
      initialiseProtocol();
    } catch (java.io.IOException e) {
      e.printStackTrace();
    }
  }

  // setName() is called when the desktop name changes
  public void setName(String name) {
    super.setName(name);
  }

  // framebufferUpdateStart() is called at the beginning of an update.
  // Here we try to send out a new framebuffer update request so that the
  // next update can be sent out in parallel with us decoding the current
  // one. We cannot do this if we're in the middle of a format change
  // though.
  public void framebufferUpdateStart() {
    if (!formatChange) {
      pendingUpdate = true;
      requestNewUpdate();
    } else 
      pendingUpdate = false;
  }

  // framebufferUpdateEnd() is called at the end of an update.
  // For each rectangle, the FdInStream will have timed the speed
  // of the connection, allowing us to select format and encoding
  // appropriately, and then request another incremental update.
  public void framebufferUpdateEnd() {
    //desktop.framebufferUpdateEnd();

    if (firstUpdate) {
      int width, height;
      
      if (cp.supportsSetDesktopSize) {
        width = viewer.getVisibleWidth();
        height = viewer.getVisibleHeight();
        ScreenSet layout;

        layout = cp.screenLayout;

        if (layout.num_screens() == 0)
          layout.add_screen(new Screen());
        else if (layout.num_screens() != 1) {

          while (true) {
            Iterator iter = layout.screens.iterator(); 
            Screen screen = (Screen)iter.next();
        
            if (!iter.hasNext())
              break;

            layout.remove_screen(screen.id);
          }
        }

        Screen screen0 = (Screen)layout.screens.iterator().next();
        screen0.dimensions.tl.x = 0;
        screen0.dimensions.tl.y = 0;
        screen0.dimensions.br.x = width;
        screen0.dimensions.br.y = height;

        writer().writeSetDesktopSize(width, height, layout);
      }

      firstUpdate = false;
    }

    // A format change prevented us from sending this before the update,
    // so make sure to send it now.
    if (formatChange && !pendingUpdate)
      requestNewUpdate();

    // Compute new settings based on updated bandwidth values
    if (autoSelect)
      autoSelectFormatAndEncoding();
  }

  // The rest of the callbacks are fairly self-explanatory...

  // TODO: Implement
//  public void setColourMapEntries(int firstColour, int nColours, int[] rgbs) {
//    desktop.setColourMapEntries(firstColour, nColours, rgbs);
//  }

  public void serverCutText(String str, int len) {
      viewer.serverJustCutText = true;
      viewer.setClipboardText(str);
  }

  // We start timing on beginRect and stop timing on endRect, to
  // avoid skewing the bandwidth estimation as a result of the server
  // being slow or the network having high latency
  public void beginRect(Rect r, int encoding) {
    ((JavaInStream)getInStream()).startTiming();
    if (encoding != Encodings.encodingCopyRect) {
      lastServerEncoding = encoding;
    }
  }

  public void endRect(Rect r, int encoding) {
    ((JavaInStream)getInStream()).stopTiming();
  }

  public void fillRect(Rect r, int p) {
      int w = r.width();
      int h = r.height();
      if (viewer.bitmapData.validDraw(r.tl.x, r.tl.y, w, h)) {
          viewer.bitmapData.fillRect(r.tl.x, r.tl.y, w, h, p);
          viewer.reDraw(r.tl.x, r.tl.y, w, h);
      }
  }

  public void imageRect(Rect r, int[] p) {
      int w = r.width();
      int h = r.height();
      if (viewer.bitmapData.validDraw(r.tl.x, r.tl.y, w, h)) {
          viewer.bitmapData.imageRect(r.tl.x, r.tl.y, w, h, p);
          viewer.reDraw(r.tl.x, r.tl.y, w, h);
      }
  }

  public void imageRect(Rect r, Bitmap b) {
      int w = r.width();
      int h = r.height();
      if (viewer.bitmapData.validDraw(r.tl.x, r.tl.y, w, h)) {
          viewer.bitmapData.updateBitmap(b, r.tl.x, r.tl.y, w, h);
          viewer.reDraw(r.tl.x, r.tl.y, w, h);
      }
  }
  
  public void copyRect(Rect r, int sx, int sy) {
      int w = r.width();
      int h = r.height();
      if (viewer.bitmapData.validDraw(r.tl.x, r.tl.y, w, h)) {
          viewer.bitmapData.copyRect(sx, sy, r.tl.x, r.tl.y, w, h);
          viewer.reDraw(r.tl.x, r.tl.y, w, h);
      }
  }

  public PixelFormat getPreferredPF() {
    return fullColourPF;
  }

  public void setCursor(int width, int height, Point hotspot,
                        int[] data, byte[] mask) {
    //TODO: Implement
    //viewer.bitmapData.setCursor(width, height, hotspot, data, mask);
  }

  private void resizeFramebuffer()
  {
    if ((cp.width == 0) && (cp.height == 0))
      return;
    if (viewer.bitmapData == null)
      return;
    if ((viewer.bitmapData.fbWidth() == cp.width) && (viewer.bitmapData.fbHeight() == cp.height))
      return;
    
    viewer.updateFBSize();
  }

  // autoSelectFormatAndEncoding() chooses the format and encoding appropriate
  // to the connection speed:
  //
  //   First we wait for at least one second of bandwidth measurement.
  //
  //   Above 16Mbps (i.e. LAN), we choose the second highest JPEG quality,
  //   which should be perceptually lossless.
  //
  //   If the bandwidth is below that, we choose a more lossy JPEG quality.
  //
  //   If the bandwidth drops below 256 Kbps, we switch to palette mode.
  //
  //   Note: The system here is fairly arbitrary and should be replaced
  //         with something more intelligent at the server end.
  //
  private void autoSelectFormatAndEncoding() {
    long kbitsPerSecond = ((JavaInStream)getInStream()).kbitsPerSecond();
    long timeWaited = ((JavaInStream)getInStream()).timeWaited();
    boolean newFullColour = fullColour;
    int newQualityLevel = cp.qualityLevel;

    // Always use Tight
    if (currentEncoding != Encodings.encodingTight) {
      currentEncoding = Encodings.encodingTight;
      encodingChange = true;
    }

    // Check that we have a decent bandwidth measurement
    if ((kbitsPerSecond == 0) || (timeWaited < 10000))
      return;
  
    // Select appropriate quality level
    if (!cp.noJpeg) {
      if (kbitsPerSecond > 16000)
        newQualityLevel = 8;
      else
        newQualityLevel = 6;
  
      if (newQualityLevel != cp.qualityLevel) {
        vlog.info("Throughput "+kbitsPerSecond+
                  " kbit/s - changing to quality "+newQualityLevel);
        cp.qualityLevel = newQualityLevel;
        //TODO: Implement
        //viewer.qualityLevel.setParam(Integer.toString(newQualityLevel));
        encodingChange = true;
      }
    }

    if (cp.beforeVersion(3, 8)) {
      // Xvnc from TightVNC 1.2.9 sends out FramebufferUpdates with
      // cursors "asynchronously". If this happens in the middle of a
      // pixel format change, the server will encode the cursor with
      // the old format, but the client will try to decode it
      // according to the new format. This will lead to a
      // crash. Therefore, we do not allow automatic format change for
      // old servers.
      return;
    }
    
    // Select best color level
    newFullColour = (kbitsPerSecond > 256);
    if (newFullColour != fullColour) {
      vlog.info("Throughput "+kbitsPerSecond+
                " kbit/s - full color is now "+ 
                  (newFullColour ? "enabled" : "disabled"));
      fullColour = newFullColour;
      formatChange = true;
    } 
  }
  
  // requestNewUpdate() requests an update from the server, having set the
  // format and encoding appropriately.
  private void requestNewUpdate()
  {
    if (formatChange) {

      /* Catch incorrect requestNewUpdate calls */
      assert(pendingUpdate == false);
      // TODO: Implement
      
      if (fullColour) {
        cp.setPF(new PixelFormat(32, 24, false, true, 255, 255, 255, 16, 8, 0));
      } else {
        if (lowColourLevel == 0) {
            cp.setPF(new PixelFormat(8,3,false,true,1,1,1,2,1,0));
        } else if (lowColourLevel == 1) {
            cp.setPF(new PixelFormat(8,6,false,true,3,3,3,4,2,0));
        } else {
            cp.setPF(new PixelFormat(8,8,false,true,7,7,3,0,3,6));
        }
      }
      String str = cp.pf().print();
      vlog.info("Using pixel format "+str);
      synchronized (this) {
        writer().writeSetPixelFormat(cp.pf());
      }  
    }
    
    checkEncodings();
    viewer.writeFullUpdateRequest(!formatChange);
    formatChange = false;
  }

  ////////////////////////////////////////////////////////////////////
  // The following methods are all called from the GUI thread

  // close() closes the socket, thus waking up the RFB thread.
  public void close() {
    try {
      shuttingDown = true;
      sock.close();
    } catch (java.io.IOException e) {
      e.printStackTrace();
    }
  }

  // Menu callbacks.  These are guaranteed only to be called after serverInit()
  // has been called, since the menu is only accessible from the DesktopWindow

//  private void initMenu() {
//    menu = new F8Menu(this);
//  }


  public void refresh() {
    viewer.writeFullUpdateRequest(false);
    pendingUpdate = true;
  }

  void processSecurityTypes () {
        if (state() != RFBSTATE_NORMAL) {

            /* Process security types which don't use encryption */
                //Security.EnableSecType(Security.secTypeNone);
                //Security.EnableSecType(Security.secTypeVncAuth);
                //Security.EnableSecType(Security.secTypePlain);
                //Security.EnableSecType(Security.secTypeIdent);

              Security.DisableSecType(Security.secTypeNone);
              Security.DisableSecType(Security.secTypeVncAuth);
              Security.DisableSecType(Security.secTypePlain);
              Security.DisableSecType(Security.secTypeIdent);

           /* Process security types which use TLS encryption */
                //Security.EnableSecType(Security.secTypeTLS);
                Security.EnableSecType(Security.secTypeTLSNone);
                Security.EnableSecType(Security.secTypeTLSVnc);
                Security.EnableSecType(Security.secTypeTLSPlain);
                Security.EnableSecType(Security.secTypeTLSIdent);

              Security.DisableSecType(Security.secTypeTLS);
              //Security.DisableSecType(Security.secTypeTLSNone);
              //Security.DisableSecType(Security.secTypeTLSVnc);
              //Security.DisableSecType(Security.secTypeTLSPlain);
              //Security.DisableSecType(Security.secTypeTLSIdent);

            /* Process security types which use X509 encryption */
                Security.EnableSecType(Security.secTypeX509None);
                Security.EnableSecType(Security.secTypeX509Vnc);
                Security.EnableSecType(Security.secTypeX509Plain);
                Security.EnableSecType(Security.secTypeX509Ident);
              //Security.DisableSecType(Security.secTypeX509None);
              //Security.DisableSecType(Security.secTypeX509Vnc);
              //Security.DisableSecType(Security.secTypeX509Plain);
              //Security.DisableSecType(Security.secTypeX509Ident);
        
            /* Process *None security types 
                Security.EnableSecType(Security.secTypeNone);
                Security.EnableSecType(Security.secTypeTLSNone);
                Security.EnableSecType(Security.secTypeX509None);
              //Security.DisableSecType(Security.secTypeNone);
              //Security.DisableSecType(Security.secTypeTLSNone);
              //Security.DisableSecType(Security.secTypeX509None);
             */
            /* Process *Vnc security types
                Security.EnableSecType(Security.secTypeVncAuth);
                Security.EnableSecType(Security.secTypeTLSVnc);
                Security.EnableSecType(Security.secTypeX509Vnc);
              //Security.DisableSecType(Security.secTypeVncAuth);
              //Security.DisableSecType(Security.secTypeTLSVnc);
              //Security.DisableSecType(Security.secTypeX509Vnc);
             */
            /* Process *Plain security types
                Security.EnableSecType(Security.secTypePlain);
                Security.EnableSecType(Security.secTypeTLSPlain);
                Security.EnableSecType(Security.secTypeX509Plain);
              //Security.DisableSecType(Security.secTypePlain);
              //Security.DisableSecType(Security.secTypeTLSPlain);
              //Security.DisableSecType(Security.secTypeX509Plain);
             */
            /* Process *Ident security types
                Security.EnableSecType(Security.secTypeIdent);
                Security.EnableSecType(Security.secTypeTLSIdent);
                Security.EnableSecType(Security.secTypeX509Ident);
              //Security.DisableSecType(Security.secTypeIdent);
              //Security.DisableSecType(Security.secTypeTLSIdent);
              //Security.DisableSecType(Security.secTypeX509Ident);
             */
        }
  }
  
  
  // writeClientCutText() is called from the clipboard dialog
  synchronized public void writeClientCutText(String str, int len) {
    if (state() != RFBSTATE_NORMAL || viewOnly) return;
    try {
        writer().writeClientCutText(str,len);
    } catch (java.lang.Exception e) {
        Log.e(TAG, "Could not write text to VNC server clipboard.");
        e.printStackTrace();
    }

  }

  synchronized public void writeKeyEvent(int keysym, int metaState, boolean down) {
        if (state() != RFBSTATE_NORMAL || viewOnly) return;
        writeModifiers(metaState);
        writer().writeKeyEvent(keysym, down);
        if (!down)
            writeModifiers(0);
  }
  
  synchronized public void writeKeyEvent(int keysym, boolean down) {
    if (state() != RFBSTATE_NORMAL || viewOnly) return;
    writer().writeKeyEvent(keysym, down);
  }

  // TODO: Get this ported.
  synchronized public void writeKeyEvent(KeyEvent ev) {
    if (ev.getAction() != KeyEvent.ACTION_DOWN || viewOnly)
      return;

    int keysym = 0;

    switch (ev.getKeyCode()) {
        case KeyEvent.KEYCODE_DEL:        keysym = Keysyms.BackSpace; break;
        case KeyEvent.KEYCODE_TAB:        keysym = Keysyms.Tab; break;
        case KeyEvent.KEYCODE_ENTER:      keysym = Keysyms.Return; break;
        case 111: /*KEYCODE_ESCAPE*/      keysym = Keysyms.Escape; break;
        default:
            vlog.debug("key press "+ev.getUnicodeChar());
            if (ev.getUnicodeChar() < 32) {
              // if the ctrl modifier key is down, send the equivalent ASCII since we
              // will send the ctrl modifier anyway

              if ((ev.getMetaState() & 28672 /*KeyEvent.META_CTRL_MASK*/) != 0) {
                if ((ev.getMetaState() & KeyEvent.META_SHIFT_ON) != 0) {
                  keysym = ev.getUnicodeChar() + 64;
                  if (keysym == -1)
                    return;
                } else { 
                  keysym = ev.getUnicodeChar() + 96;
                  if (keysym == 127) keysym = 95;
                }
              } else if (ev.getUnicodeChar() == 127) {
                  keysym = Keysyms.Delete;
              } else {
                // KEY_ACTION
                vlog.debug("key action "+ev.getKeyCode());
                switch (ev.getKeyCode()) {
                case KeyEvent.KEYCODE_HOME:         keysym = Keysyms.Home; break;
                /*
                case KeyEvent.KEYCODE_END:          keysym = Keysyms.End; break;
                case KeyEvent.KEYCODE_PAGE_UP:      keysym = Keysyms.Page_Up; break;
                case KeyEvent.KEYCODE_PAGE_DOWN:    keysym = Keysyms.Page_Down; break;
                case KeyEvent.KEYCODE_UP:           keysym = Keysyms.Up; break;
                case KeyEvent.KEYCODE_DOWN:         keysym = Keysyms.Down; break;
                case KeyEvent.KEYCODE_LEFT:         keysym = Keysyms.Left; break;
                case KeyEvent.KEYCODE_RIGHT:        keysym = Keysyms.Right; break;
                case KeyEvent.KEYCODE_F1:           keysym = Keysyms.F1; break;
                case KeyEvent.KEYCODE_F2:           keysym = Keysyms.F2; break;
                case KeyEvent.KEYCODE_F3:           keysym = Keysyms.F3; break;
                case KeyEvent.KEYCODE_F4:           keysym = Keysyms.F4; break;
                case KeyEvent.KEYCODE_F5:           keysym = Keysyms.F5; break;
                case KeyEvent.KEYCODE_F6:           keysym = Keysyms.F6; break;
                case KeyEvent.KEYCODE_F7:           keysym = Keysyms.F7; break;
                case KeyEvent.KEYCODE_F8:           keysym = Keysyms.F8; break;
                case KeyEvent.KEYCODE_F9:           keysym = Keysyms.F9; break;
                case KeyEvent.KEYCODE_F10:          keysym = Keysyms.F10; break;
                case KeyEvent.KEYCODE_F11:          keysym = Keysyms.F11; break;
                case KeyEvent.KEYCODE_F12:          keysym = Keysyms.F12; break;
                case KeyEvent.KEYCODE_PRINTSCREEN:  keysym = Keysyms.Print; break;
                case KeyEvent.KEYCODE_PAUSE:        keysym = Keysyms.Pause; break;
                case KeyEvent.KEYCODE_INSERT:       keysym = Keysyms.Insert; break;
                */
                default:
                    keysym = UnicodeToKeysym.translate(ev.getUnicodeChar());
                    if (keysym == -1)
                      return;
                }

              }
            }
    }

    writeModifiers(ev.getMetaState());
    writeKeyEvent(keysym, true);
    writeKeyEvent(keysym, false);
    writeModifiers(0);
  }


  public void writePointerEvent(int mouseX, int mouseY, int metaState, int pointerMask) {
        if (state() != RFBSTATE_NORMAL || viewOnly) return;
        
        synchronized (this) {
            writeModifiers(metaState);
          }
        
        synchronized (this) {
          writer().writePointerEvent(new Point(mouseX,mouseY), pointerMask);
        }

        if (buttonMask == 0) writeModifiers(0);
  }
  
  public void writePointerEvent(MotionEvent ev) {
    if (state() != RFBSTATE_NORMAL || viewOnly) return;
    
    switch (ev.getAction()) {
    case MotionEvent.ACTION_DOWN:
      android.util.Log.e("CConn", "PointerEvent action down");
      buttonMask = 1;
      if ((ev.getMetaState() & 50) != 0) buttonMask = 2;
      if ((ev.getMetaState() & 458752) != 0) buttonMask = 4;
      break;
    case MotionEvent.ACTION_UP:
      android.util.Log.e("CConn", "PointerEvent action up");
      buttonMask = 0;
      break;
    }

    synchronized (this) {
      writeModifiers(ev.getMetaState() & ~50 & ~458752);
    }

    /*
    if (cp.width != viewer.bitmapData.width() || 
        cp.height != viewer.bitmapData.height()) {
        // TODO: Inspect
      int sx = (int) ((viewer.bitmapData.scaleWidthRatio == 1.00) 
        ? ev.getX() : (int)Math.floor(ev.getX()/viewer.bitmapData.scaleWidthRatio));
      int sy = (int) ((viewer.bitmapData.scaleHeightRatio == 1.00) 
        ? ev.getY() : (int)Math.floor(ev.getY()/viewer.bitmapData.scaleHeightRatio));
      ev.setLocation(sx - ev.getX(), sy - ev.getY());
    }
    */
    
    synchronized (this) {
      writer().writePointerEvent(new Point(ev.getX(),ev.getY()), buttonMask);
    }

    if (buttonMask == 0) writeModifiers(0);
  }


/*  synchronized public void writeWheelEvent(MouseWheelEvent ev) {
    if (state() != RFBSTATE_NORMAL) return;
    int x, y;
    int clicks = ev.getWheelRotation();
    if (clicks < 0) {
      buttonMask = 8;
    } else {
      buttonMask = 16;
    }
    writeModifiers(ev.getModifiers() & ~KeyEvent.ALT_MASK & ~KeyEvent.META_MASK);
    for (int i=0;i<Math.abs(clicks);i++) {
      x = ev.getX();
      y = ev.getY();
      writer().writePointerEvent(new Point(x, y), buttonMask);
      buttonMask = 0;
      writer().writePointerEvent(new Point(x, y), buttonMask);
    }
    writeModifiers(0);

  }
*/
  
  // TODO: Get this ported.
  synchronized void writeModifiers(int newModifiers) {
      if ((newModifiers & RemoteKeyboard.CTRL_MASK) != (oldModifiers & RemoteKeyboard.CTRL_MASK))
          writeKeyEvent(0xffe3, (newModifiers & RemoteKeyboard.CTRL_MASK) != 0);

      if ((newModifiers & RemoteKeyboard.SHIFT_MASK) != (oldModifiers & RemoteKeyboard.SHIFT_MASK))
          writeKeyEvent(0xffe1, (newModifiers & RemoteKeyboard.SHIFT_MASK) != 0);

      if ((newModifiers & RemoteKeyboard.META_MASK) != (oldModifiers & RemoteKeyboard.META_MASK))
          writeKeyEvent(0xffe7, (newModifiers & RemoteKeyboard.META_MASK) != 0);

      if ((newModifiers & RemoteKeyboard.ALT_MASK) != (oldModifiers & RemoteKeyboard.ALT_MASK))
          writeKeyEvent(0xffe9, (newModifiers & RemoteKeyboard.ALT_MASK) != 0);
      oldModifiers = newModifiers;
  }


  ////////////////////////////////////////////////////////////////////
  // The following methods are called from both RFB and GUI threads

  // checkEncodings() sends a setEncodings message if one is needed.
  synchronized private void checkEncodings() {
    if (encodingChange && state() == RFBSTATE_NORMAL) {
      vlog.info("Using "+Encodings.encodingName(currentEncoding)+" encoding");
      writer().writeSetEncodings(currentEncoding, true);
      encodingChange = false;
    }
  }

  public String getEncoding () {
        switch (currentEncoding) {
        case Encodings.encodingRaw:
            return "RAW";
        case Encodings.encodingTight:
            return "TIGHT";
        case Encodings.encodingCoRRE:
            return "CoRRE";
        case Encodings.encodingHextile:
            return "HEXTILE";
        case Encodings.encodingRRE:
            return "RRE";
        case Encodings.encodingZRLE:
            return "ZRLE";
        }
        return "";
  }

  // The following methods are implementations of the RfbConnectable interface
  @Override
  public int framebufferWidth () {
      return cp.width;
  }
  
  @Override
  public int framebufferHeight () {
      return cp.height;
  }
  
  @Override
  public String desktopName () {
      return cp.name();
  }
  
  @Override
  public void requestUpdate (boolean incremental) {
      if (incremental)
          refresh();
      else
          requestNewUpdate();
  }

  @Override
  public void writeClientCutText(String text) {
      writeClientCutText(text, text.length());      
  }

  @Override
  public void setIsInNormalProtocol(boolean state) {
      if (state)
          setState (RFBSTATE_NORMAL);
      else
          setState (RFBSTATE_INVALID);
  }
  
  @Override
  public boolean isInNormalProtocol() {
      return (state() == RFBSTATE_NORMAL);
  }

  @Override
  public synchronized void writeFramebufferUpdateRequest(int x, int y, int w, int h, boolean incremental) {
      synchronized (this) {
          writer().writeFramebufferUpdateRequest(new Rect(x, y, x+w, y+h), incremental);
      }
  }
  
  public void writeSetPixelFormat(int bitsPerPixel, int depth, boolean bigEndian,
           boolean trueColour, int redMax, int greenMax, int blueMax,
           int redShift, int greenShift, int blueShift, boolean fGreyScale) {
      // TODO: This is broken for anything less than true color.
    /*
    fullColour = false;
    formatChange = true;
    //encodingChange = false;
    lowColourLevel = 2;
    this.requestNewUpdate();
    */

    /*
    cp.setPF(new PixelFormat(bitsPerPixel, depth, bigEndian, trueColour, redMax, 
            greenMax, blueMax, redShift, greenShift, blueShift));
 
    String str = cp.pf().print();
    vlog.info("Using pixel format "+str);
    synchronized (this) {
        writer().writeSetPixelFormat(cp.pf());
    }
  
    checkEncodings();
    synchronized (this) {
        writer().writeFramebufferUpdateRequest(new Rect(0,0,cp.width,cp.height), false);
    }
    */
  }
  
  
  // the following never change so need no synchronization:
  JavaInStream jis;
  JavaOutStream jos;

  // access to desktop by different threads is specified in DesktopWindow

  // the following need no synchronization:

  ClassLoader cl = this.getClass().getClassLoader();
  public static UserPasswdGetter upg;
  public UserMsgBox msg;

  // shuttingDown is set by the GUI thread and only ever tested by the RFB
  // thread after the window has been destroyed.
  boolean shuttingDown;

  // reading and writing int and boolean is atomic in java, so no
  // synchronization of the following flags is needed:
  int currentEncoding, lastServerEncoding;
  
  int lowColourLevel;


  // All menu, options, about and info stuff is done in the GUI thread (apart
  // from when constructed).
  //F8Menu menu;
  //OptionsDialog options;

  // clipboard sync issues?
  //ClipboardDialog clipboardDialog;

  // the following are only ever accessed by the GUI thread:
  int buttonMask;
  int oldModifiers;

  public String serverHost;
  public int serverPort;
  public int menuKey;
  PixelFormat serverPF;
  PixelFormat fullColourPF;
  boolean fullColour;
  boolean autoSelect;
  boolean shared;
  boolean formatChange;
  boolean encodingChange;
  boolean sameMachine;
  boolean fullScreen;
  boolean reverseConnection;
  boolean firstUpdate;
  boolean pendingUpdate;
  boolean viewOnly;
  AbstractConnectionBean connection;
  
  static LogWriter vlog = new LogWriter("CConn");

@Override
public void requestResolution(int x, int y) {
    // TODO Auto-generated method stub
    
}
}
