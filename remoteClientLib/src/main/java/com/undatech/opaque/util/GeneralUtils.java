package com.undatech.opaque.util;

public class GeneralUtils {
    public static Class<?> getClassByName(String name) {
        Class<?> remoteCanvasActivityClass = null;
        try {
            remoteCanvasActivityClass = Class.forName(name);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return remoteCanvasActivityClass;
    }
}
