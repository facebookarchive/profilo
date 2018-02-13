// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.jni.annotations;

import static java.lang.annotation.RetentionPolicy.CLASS;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 * Add this annotation to a class, method, or field to instruct Proguard to not strip it out.
 *
 * This is useful for methods called via reflection that could appear as unused to Proguard.
 */
@Target({ ElementType.TYPE, ElementType.FIELD, ElementType.METHOD, ElementType.CONSTRUCTOR })
@Retention(CLASS)
public @interface DoNotStrip {
}
