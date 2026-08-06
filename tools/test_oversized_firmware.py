#!/usr/bin/env python3
"""Build the real firmware against an intentionally overlapping boundary."""
import os,pathlib,shutil,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
sdk=os.environ.get('PICO_SDK_PATH')
if not sdk: raise SystemExit('PICO_SDK_PATH is required')
with tempfile.TemporaryDirectory(prefix='pml-overlap-') as temporary:
 build=pathlib.Path(temporary)/'build'
 configure=['cmake','-S',str(root),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',f'-DPICO_SDK_PATH={sdk}','-DPICO_BOARD=pico_w','-DPML_BUILD_OVERSIZED_FIRMWARE=ON']
 subprocess.run(configure,check=True,stdout=subprocess.DEVNULL)
 result=subprocess.run(['cmake','--build',str(build)],capture_output=True,text=True)
 if result.returncode==0: raise SystemExit('intentionally overlapping firmware unexpectedly linked')
 if ('application image overlaps Feature 005 persistent region' not in result.stdout+result.stderr and 'region `FLASH` overflowed' not in result.stdout+result.stderr):
  raise SystemExit('negative build failed for an unrelated reason')
 for suffix in ('.elf','.uf2','.bin','.hex'):
  if (build/f'pico_music_lights{suffix}').exists():
   raise SystemExit(f'negative build produced flashable artifact {suffix}')
print('oversized_firmware_negative_test=PASS')
