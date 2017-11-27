// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.common.dextricks.classid;

import android.os.Build;
import android.support.annotation.RequiresApi;
import com.android.dex.Dex;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.ConcurrentHashMap;

@RequiresApi(api = Build.VERSION_CODES.KITKAT)
@DoNotStrip
public class ClassId {

  public static final boolean sInitialized;
  public static final long CLASS_ID_NOT_FOUND = -1L;

  private static final ConcurrentHashMap<Object, Integer> sDexKeyToDexSignature;
  private static Field javaLangClass_dexClassDefIndex;
  private static Field javaLangClass_dexCache;
  private static Method javaLangDexCache_getDex;
  private static Field javaLangDexCache_dexFile;
  private static Method javaLangClass_getDexClassDefIndex;
  private static Method javaLangClass_getDex;

  static {
    SoLoader.loadLibrary("classid");

    sDexKeyToDexSignature = new ConcurrentHashMap<>(0, 0.9f);
    sInitialized = initialize();
  }

  static synchronized boolean initialize() {
    Class<?> jlClass = Class.class;
    try {
      // Should work on ART
      Field dexClassdefIndex = jlClass.getDeclaredField("dexClassDefIndex");
      Field dexCache = jlClass.getDeclaredField("dexCache");
      Class<?> jlDexCache = Class.forName("java.lang.DexCache");

      dexClassdefIndex.setAccessible(true);
      dexCache.setAccessible(true);

      javaLangClass_dexClassDefIndex = dexClassdefIndex;
      javaLangClass_dexCache = dexCache;

      try {
        Method jlDexCache_getDex = jlDexCache.getDeclaredMethod("getDex");
        jlDexCache_getDex.setAccessible(true);
        javaLangDexCache_getDex = jlDexCache_getDex;
      } catch (Exception e) {
        // Oreo doesn't have getDex, get at it more indirectly
        Field jlDexCache_dexFile = jlDexCache.getDeclaredField("dexFile");
        jlDexCache_dexFile.setAccessible(true);
        javaLangDexCache_dexFile = jlDexCache_dexFile;
      }

      verifyInitialize();

      return true;
    } catch (Exception e) {
      // intentionally ignored
    }

    try {
      // Should work on some versions of dalvik
      Method getDexClassDefIndex = jlClass.getDeclaredMethod("getDexClassDefIndex");
      Method getDex = jlClass.getDeclaredMethod("getDex");

      getDexClassDefIndex.setAccessible(true);
      getDex.setAccessible(true);

      javaLangClass_getDexClassDefIndex = getDexClassDefIndex;
      javaLangClass_getDex = getDex;

      verifyInitialize();

      return true;
    } catch (Exception e) {
      // intentionally ignored
    }

    return false;
  }

  private static void verifyInitialize() {
    getClassDef(ClassId.class);
    getDexSignature(ClassId.class);
  }

  public static long getClassId(final Class<?> cls) {
    if (!sInitialized) {
      return CLASS_ID_NOT_FOUND;
    }

    final long signature = ((long) getDexSignature(cls)) & 0xFFFFFFFFL;
    final long id = ((long) getClassDef(cls) << 32) | signature;
    return id;
  }

  private static int getClassDef(Class<?> cls) {
    try {
      if (javaLangClass_dexClassDefIndex != null) {
        return (int) javaLangClass_dexClassDefIndex.get(cls);
      } else if (javaLangClass_getDexClassDefIndex != null) {
        return (int) javaLangClass_getDexClassDefIndex.invoke(cls);
      }
    } catch (Exception e) {
      throw new RuntimeException(e);
    }

    throw new IllegalStateException();
  }

  private static int getDexSignature(Class<?> cls) {
    try {
      if (javaLangDexCache_dexFile != null) {
        return calculateViaDexCacheDexFileSignature(cls);
      } else if (javaLangClass_dexCache != null) {
        return calculateViaDexCacheDexSignature(cls);
      } else if (javaLangClass_getDex != null) {
        return calculateViaDexObjDexSignature(cls);
      } else {
        throw new IllegalStateException();
      }
    } catch (InvocationTargetException | IllegalAccessException | IOException e) {
      throw new RuntimeException(e);
    }
  }

  private static int calculateViaDexCacheDexFileSignature(Class<?> cls)
      throws IllegalAccessException, IOException, InvocationTargetException {
    Object dexCache = javaLangClass_dexCache.get(cls);

    if (dexCache == null) {
      // Some OEMs hit this value more often than others.
      // Regardless, let's fail gracefully.
      return 0;
    }

    Integer cachedSignature = sDexKeyToDexSignature.get(dexCache);
    if (cachedSignature == null) {
      long dexFile = javaLangDexCache_dexFile.getLong(dexCache);
      cachedSignature = getSignatureFromDexFile(dexFile);
      sDexKeyToDexSignature.put(dexCache, cachedSignature);
    }

    return cachedSignature;
  }

  private static int calculateViaDexCacheDexSignature(Class<?> cls)
      throws InvocationTargetException, IllegalAccessException {
    Object dexCache = javaLangClass_dexCache.get(cls);

    if (dexCache == null) {
      // Some OEMs hit this value more often than others.
      // Regardless, let's fail gracefully.
      return 0;
    }

    Integer cachedSignature = sDexKeyToDexSignature.get(dexCache);
    if (cachedSignature == null) {
      Dex dex = (Dex) javaLangDexCache_getDex.invoke(dexCache);
      cachedSignature = getSignatureForDex(dex);
      sDexKeyToDexSignature.put(dexCache, cachedSignature);
    }

    return cachedSignature;
  }

  private static int calculateViaDexObjDexSignature(Class<?> cls)
      throws InvocationTargetException, IllegalAccessException {
    Dex dex = (Dex) javaLangClass_getDex.invoke(cls);
    if (dex == null) {
      // Just in case
      return 0;
    }

    Integer cachedSignature = sDexKeyToDexSignature.get(dex);
    if (cachedSignature == null) {
      cachedSignature = getSignatureForDex(dex);
      sDexKeyToDexSignature.put(dex, cachedSignature);
    }

    return cachedSignature;
  }

  private static int getSignatureForDex(Dex dex)
      throws IllegalAccessException, InvocationTargetException {
    final int SIGNATURE_OFFSET = 12;
    Dex.Section signatureSection = dex.open(SIGNATURE_OFFSET);
    return signatureSection.readInt();
  }

  private static native int getSignatureFromDexFile(long dexFilePointer);
}
