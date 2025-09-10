package com.antlersoft.util.xml;

import org.xml.sax.Attributes;
import org.xml.sax.helpers.AttributesImpl;

public class SimpleAttributes
{
  private Attributes attr;
  public int defaultInt = 0;
  public String defaultString = "";
  public double defaultDouble = 0.0D;

  public SimpleAttributes(Attributes paramAttributes)
  {
    this.attr = paramAttributes;
  }

  public SimpleAttributes()
  {
    this.attr = new AttributesImpl();
  }

  public Attributes getAttributes()
  {
    return this.attr;
  }

  public void addValue(Object paramObject1, Object paramObject2)
  {
    ((AttributesImpl)this.attr).addAttribute("", "", paramObject1.toString(), "", paramObject2.toString());
  }

  public int intValue(Object paramObject, int paramInt)
  {
    String str = this.attr.getValue(paramObject.toString());
    if (str != null)
      try
      {
        return Integer.valueOf(str).intValue();
      }
      catch (NumberFormatException localNumberFormatException)
      {
      }
    return paramInt;
  }

  public int intValue(Object paramObject)
  {
    return intValue(paramObject, this.defaultInt);
  }

  public void setDefaultInt(int paramInt)
  {
    this.defaultInt = paramInt;
  }

  public String stringValue(Object paramObject, String paramString)
  {
    String str = this.attr.getValue(paramObject.toString());
    if (str == null)
      str = paramString;
    return str;
  }

  public String stringValue(Object paramObject)
  {
    return stringValue(paramObject, this.defaultString);
  }

  public double doubleValue(Object paramObject, double paramDouble)
  {
    String str = this.attr.getValue(paramObject.toString());
    if (str != null)
      try
      {
        return Double.valueOf(str).doubleValue();
      }
      catch (NumberFormatException localNumberFormatException)
      {
      }
    return paramDouble;
  }

  public double doubleValue(Object paramObject)
  {
    return doubleValue(paramObject, this.defaultDouble);
  }

  public long longValue(Object paramObject, long paramLong)
  {
    String str = this.attr.getValue(paramObject.toString());
    if (str != null)
      try
      {
        return Long.valueOf(str).longValue();
      }
      catch (NumberFormatException localNumberFormatException)
      {
      }
    return paramLong;
  }

  public long longValue(Object paramObject)
  {
    return longValue(paramObject, this.defaultInt);
  }

  public boolean booleanValue(Object paramObject, boolean paramBoolean)
  {
    String str = this.attr.getValue(paramObject.toString());
    if (str != null)
      return Boolean.valueOf(str).booleanValue();
    return paramBoolean;
  }

  public boolean booleanValue(Object paramObject)
  {
    return booleanValue(paramObject, false);
  }
}
