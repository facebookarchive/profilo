/*	$OpenBSD: setjmp.h,v 1.2 2004/08/10 21:10:56 pefo Exp $	*/

/* Public domain */

#ifndef _MIPS_SETJMP_H_
#define _MIPS_SETJMP_H_
#define MIPS_SETJMP_H_
#define MIPS_SETJMP_H
#define NDK_ANDROID_SUPPORT_MIPS_SETJMP_H_
#define NDK_ANDROID_SUPPORT_MIPS_SETJMP_H
#define _MIPS_SETJMP_H

#ifdef __LP64__
#define	_JBLEN	25	/* size, in 8-byte longs, of a mips64 jmp_buf/sigjmp_buf */
#else
#define	_JBLEN	157	/* historical size, in 4-byte longs, of a mips32 jmp_buf */
			/* actual used size is 34 */
#endif

#endif /* !_MIPS_SETJMP_H_ */
