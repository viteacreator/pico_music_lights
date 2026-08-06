#!/usr/bin/env python3
"""Prove the Feature 005 linker ASSERT accepts a valid end and rejects overlap."""
import pathlib, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
GUARD=ROOT/'src/storage/persistent_region_guard.ld'
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td)
 source=p/'empty.s'; source.write_text('.section .text\n.global _start\n_start:\n nop\n',encoding='ascii')
 obj=p/'empty.o'
 subprocess.run(['arm-none-eabi-as','-mcpu=cortex-m0plus','-mthumb','-o',obj,source],check=True)
 for address, success in ((0x101fdfff,True),(0x101fe001,False)):
  define=p/'define.ld'; define.write_text(f'__flash_binary_end = 0x{address:x};\n__pml_persistent_start = 0x101fe000;\n',encoding='ascii')
  result=subprocess.run(['arm-none-eabi-ld','-T',define,'-T',GUARD,'-o',p/f'{address:x}.elf',obj],capture_output=True,text=True)
  if (result.returncode==0)!=success:
   raise SystemExit(f'guard result mismatch address={address:#x}: {result.stderr}')
print('persistent_link_guard_test=PASS')
