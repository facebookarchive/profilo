/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.fbtrace.utils;

import java.util.Random;

/**
 * This class implements the fbtrace encoding for integers. The point of this encoding is to provide
 * a more compact representation for integers as printable strings than just representing them as
 * decimal or hex.
 */
public class FbTraceId {

  // each long integer id is encoded into custom Base64 string and truncated to 11 char long
  private static final int ID_ENCODING_LENGTH = 11;
  // metadata is 2 id strings concat together, ergo length = 22
  private static final int META_DATA_ENCODING_LENGTH = 22;
  // the encoding is a customized Base64, actually base63 encoding, this was required by the
  // partitioning scheme from the fbtrace backend, that's why we don't use android.util.Base64
  private static final String BASE64_ALPHABET =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  public static final String NULL_ID = "AAAAAAAAAAA";

  private static final Random random = new Random();

  /**
   * generate an random id and encodes it.
   *
   * @return the encoded id
   */
  public static String generate() {
    return encode(generateValidLong());
  }

  /**
   * encodes a given id.
   *
   * @param id the id to internalEncode
   * @return the encoded id
   */
  public static String encode(long id) {
    return internalEncode(id, ID_ENCODING_LENGTH);
  }

  /**
   * generate a random positive 64bit integer to serve as an ID
   *
   * @return the random ID
   */
  public static long generateValidLong() {
    long l;
    // Math.abs(Long.MIN_VALUE) is still negative, needs to skip
    // if we encounter that value, though at vanishingly low probability
    while ((l = Math.abs(random.nextLong())) <= 0) ;
    return l;
  }

  /**
   * Encode a non-negative integer as a length-n base-64 string. The string is to be interpreted as
   * big-endian.
   *
   * @param x the integer, where 0 <= <tt>x</tt> <= ((1 << (6*<tt>n</tt>)) - 1)
   * @param n size of the string
   * @return string of length <tt>n</tt>
   * @throws IllegalArgumentException if x
   */
  private static String internalEncode(long x, int n) {
    if (n < 0) throw new IllegalArgumentException("n must be positive");
    if (x < 0) throw new IllegalArgumentException("Cannot internalEncode negative integer " + x);
    if (x > (1l << Math.min(63, 6 * n)) - 1)
      throw new IllegalArgumentException(
          "Cannot internalEncode integer " + x + " in " + n + " chars");

    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < n; i++) {
      sb.append(BASE64_ALPHABET.charAt((int) (x % 64)));
      x >>= 6;
    }
    if (x > 0) {
      throw new IllegalArgumentException("Number won't fit in string");
    } else {
      sb.reverse();
      return sb.toString();
    }
  }

  /**
   * Decodes a metadata String.
   *
   * @param meta the metadata blob to internalDecode in the format of (string,string) as specified
   *     in FBTrace design.
   * @return an array containing (traceID, selfID)
   */
  public static long[] decodeMetadata(String meta) throws IllegalArgumentException {
    if (meta == null || meta.length() != META_DATA_ENCODING_LENGTH) {
      throw new IllegalArgumentException("Invalid Metadata");
    }
    long ids[] = {
      internalDecode(meta.substring(0, ID_ENCODING_LENGTH)),
      internalDecode(meta.substring(ID_ENCODING_LENGTH, META_DATA_ENCODING_LENGTH)),
    };
    return ids;
  }

  /**
   * Decode a base-64 string that represents an integer.
   *
   * @param s the string to be decoded
   * @return an integer
   * @throws IllegalArgumentException if the string is base-64 encoded
   */
  private static long internalDecode(String s) {
    long x = 0;
    for (int i = 0; i < s.length(); i++) {
      char c = s.charAt(i);
      x <<= 6;
      int d = BASE64_ALPHABET.indexOf(c);
      if (d < 0) {
        throw new IllegalArgumentException("Invalid encoded integer " + s);
      } else {
        x += d;
      }
    }
    return x;
  }
}
