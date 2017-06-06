# name: csky - all
#as: -mcpu=ck810e -W
#objdump: -D

.*: +file format .*csky.*

Disassembly of section \.text:
#...
\s*[0-9a-f]*:\s*c4818820\s*mulu\s*r1,\s*r4
\s*[0-9a-f]*:\s*c6ec8840\s*mulua\s*r12,\s*r23
\s*[0-9a-f]*:\s*c46f8880\s*mulus\s*r15,\s*r3
\s*[0-9a-f]*:\s*c4418c20\s*muls\s*r1,\s*r2
\s*[0-9a-f]*:\s*c4428c40\s*mulsa\s*r2,\s*r2
\s*[0-9a-f]*:\s*c4638c80\s*mulss\s*r3,\s*r3
\s*[0-9a-f]*:\s*c6689040\s*mulsha\s*r8,\s*r19
\s*[0-9a-f]*:\s*c4319080\s*mulshs\s*r17,\s*r1
\s*[0-9a-f]*:\s*c6ec9440\s*mulswa\s*r12,\s*r23
\s*[0-9a-f]*:\s*c4a39480\s*mulsws\s*r3,\s*r5
\s*[0-9a-f]*:\s*c43eb020\s*vmulsh\s*r30,\s*r1
\s*[0-9a-f]*:\s*c7e0b040\s*vmulsha\s*r0,\s*r31
\s*[0-9a-f]*:\s*c58cb420\s*vmulsw\s*r12,\s*r12
\s*[0-9a-f]*:\s*c6bcb440\s*vmulswa\s*r28,\s*r21
\s*[0-9a-f]*:\s*c481b480\s*vmulsws\s*r1,\s*r4
\s*[0-9a-f]*:\s*f9221201\s*vmfvr.u8\s*r1,\s*vr2\[9\]
\s*[0-9a-f]*:\s*f8041223\s*vmfvr.u16\s*r3,\s*vr4\[0\]
\s*[0-9a-f]*:\s*f8a8125f\s*vmfvr.u32\s*r31,\s*vr8\[5\]
\s*[0-9a-f]*:\s*f824128d\s*vmfvr.s8\s*r13,\s*vr4\[1\]
\s*[0-9a-f]*:\s*f9af12b7\s*vmfvr.s16\s*r23,\s*vr15\[13\]
\s*[0-9a-f]*:\s*f8101305\s*vmtvr.u8\s*vr5\[0\],\s*r16
\s*[0-9a-f]*:\s*f8ea1324\s*vmtvr.u16\s*vr4\[7\],\s*r10
\s*[0-9a-f]*:\s*f9ea134f\s*vmtvr.u32\s*vr15\[15\],\s*r10
\s*[0-9a-f]*:\s*f94a0e81\s*vdup.8\s*fr1,\s*vr10\[10\]
\s*[0-9a-f]*:\s*f83a0e8f\s*vdup.16\s*fr15,\s*vr10\[1\]
\s*[0-9a-f]*:\s*faaa0e87\s*vdup.32\s*fr7,\s*vr10\[5\]
