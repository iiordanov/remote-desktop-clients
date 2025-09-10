package com.antlersoft.util;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.StringTokenizer;

public class TestRunner {
    private static Class[] parameterTypes={ (new String[0]).getClass() };

    public static void anyclass( String[] args)
    throws Throwable
	{
	    String[] newArgs=new String[args.length-2];
	    System.arraycopy( args, 2, newArgs, 0, newArgs.length);
	    Class.forName( args[0]).getMethod( args[1], parameterTypes).invoke(
	        null, new Object[] { newArgs});
	}
	
	public static void main( String[] args)
	    throws Throwable
	{
	    BufferedReader commands=new BufferedReader(
	        new InputStreamReader( new FileInputStream( args[0])));
	    String line;
	    for ( line=commands.readLine(); line!=null; line=commands.readLine())
	    {
	        if ( line.startsWith( "#"))
	            continue;
	        StringTokenizer tokens=new StringTokenizer( line, "|");
	        if ( tokens.hasMoreTokens())
	        {
	            String method=tokens.nextToken();
	            ArrayList argList=new ArrayList();
	            while ( tokens.hasMoreTokens())
	            {
	                argList.add( tokens.nextToken());
	            }
	            try
	            {
	                TestRunner.class.getMethod( method, parameterTypes).invoke( null,
	                    new Object[] { argList.toArray( new String[argList.size()])});
	            }
	            catch ( InvocationTargetException ite)
	            {
	                throw ite.getTargetException();
	            }
	        }
	    }
	}
}
