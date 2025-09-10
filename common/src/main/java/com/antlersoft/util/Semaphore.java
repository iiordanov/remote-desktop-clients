
/**
 * Title:        antlersoft java software<p>
 * Description:  antlersoft Moose
 * antlersoft BBQ<p>
 * <p>Copyright (c) 2000-2005  Michael A. MacDonald<p>
 * ----- - - -- - - --
 * <p>
 *     This package is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * <p>
 *     This package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * <p>
 *     You should have received a copy of the GNU General Public License
 *     along with the package (see gpl.txt); if not, see www.gnu.org
 * <p>
 * ----- - - -- - - --
 * Company:      antlersoft<p>
 * @author Michael MacDonald
 * @version 1.0
 */
package com.antlersoft.util;

public final class Semaphore
{
    public Semaphore()
    {
        protectedCount=0;
        inCritical=false;
        criticalThread=null;
    }

    public synchronized void enterProtected()
    {
        while ( inCritical)
        {
            try
            {
                wait();
            }
            catch ( InterruptedException ie)
            {
            }
        }
        protectedCount++;
    }

    public synchronized void leaveProtected()
    {
        if ( protectedCount<=0 || inCritical)
            throw new IllegalStateException( "Not in protected state");
        --protectedCount;
        notify();
    }

    public synchronized void enterCritical()
    {
        Thread currentThread=Thread.currentThread();
        if ( inCritical && criticalThread==currentThread)
            return;
        while ( protectedCount!=0 || inCritical)
        {
            try
            {
                wait();
            }
            catch ( InterruptedException ie)
            {
            }
        }
        criticalThread=currentThread;
        inCritical=true;
    }

    public synchronized void leaveCritical()
    {
        if ( protectedCount!=0 || ! inCritical || criticalThread!=
            Thread.currentThread())
        {
            throw new IllegalStateException( "Not in critical state");
        }
        inCritical=false;
        criticalThread=null;
        notifyAll();
    }

    public synchronized void runCritical( Action toRun)
        throws Exception
    {
        enterCritical();
        try
        {
            toRun.perform();
        }
        finally
        {
            leaveCritical();
        }
    }

    public synchronized void criticalToProtected()
    {
        if ( protectedCount!=0 || ! inCritical || criticalThread!=
            Thread.currentThread())
        {
            throw new IllegalStateException( "Not in critical state");
        }
        inCritical=false;
        protectedCount=1;
        criticalThread=null;
        notifyAll();
    }

    private int protectedCount;
    private boolean inCritical;
    private Thread criticalThread;
}
