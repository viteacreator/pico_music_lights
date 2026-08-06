#!/usr/bin/env python3
"""Arm Release compiler stack frames and explicit Feature 005 call paths."""
import argparse,pathlib,subprocess,tempfile,os
parser=argparse.ArgumentParser()
parser.add_argument('--pico-build',type=pathlib.Path)
args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
sources=['src/storage/device_config_store.cpp','src/storage/save_coordinator.cpp','src/config/config_service.cpp','src/storage/persistent_record.cpp','src/config/device_config_codec.cpp','src/config/device_config.cpp']
compiler=os.environ.get('PML_ARM_CXX','arm-none-eabi-g++')
with tempfile.TemporaryDirectory() as td:
 frames={}
 for source in sources:
  obj=pathlib.Path(td)/(pathlib.Path(source).stem+'.o')
  subprocess.run([compiler,'-std=c++17','-O3','-mcpu=cortex-m0plus','-mthumb','-fno-exceptions','-fno-rtti','-fstack-usage','-I'+str(root/'include'),'-I'+str(root/'src'),'-c',str(root/source),'-o',str(obj)],check=True)
  for su in pathlib.Path(td).glob('*.su'):
   for line in su.read_text().splitlines():
    parts=line.rsplit('\t',2)
    if len(parts)==3 and parts[1].isdigit(): frames[parts[0]]=int(parts[1])
 def frame(token):
  values=[v for n,v in frames.items() if token in n]
  if not values: raise SystemExit(f'missing stack frame {token}')
  return max(values)
 decode_path=frame('inspect_record')+frame('config::decode')+frame('config::encode')+frame('config::validate')
 inspect_path=frame('inspect_both')+frame('inspect_slot')+decode_path
 load=frame('DeviceConfigStore::load')+inspect_path
 prepare=frame('DeviceConfigStore::prepare_save')+max(inspect_path,frame('build_record')+frame('config::encode'))
 commit=frame('DeviceConfigStore::commit_prepared')+inspect_path
 save=frame('SaveCoordinator::persist')+max(prepare,commit)+frame('SaveCoordinator::finish')
 # Preview/discard do not call finish(); activation is a synchronous callback
 # whose concrete firmware frame is reported by the Pico build stack-usage
 # artifacts and does not overlap the store workspace (which is object-owned).
 preview=frame('SaveCoordinator::preview')+frame('activate_only')
 discard=frame('SaveCoordinator::reload')+frame('activate_only')
 reset=frame('factory_reset')+save
 paths={'load':load,'save':save,'preview':preview,'discard':discard,'reset':reset}
 if args.pico_build:
  pico_frames={}
  for su in args.pico_build.rglob('*.su'):
   for line in su.read_text(errors='replace').splitlines():
    parts=line.rsplit('\t',2)
    if len(parts)==3 and parts[1].isdigit(): pico_frames[parts[0]]=int(parts[1])
  def pico(token):
   values=[v for n,v in pico_frames.items() if token in n]
   if not values: raise SystemExit(f'missing Pico stack frame {token}')
   return max(values)
  prepare_runtime=pico('RendererPublicationBackend::prepare')+pico('build_runtime_configuration')+pico('adapt_current_board')+max(pico('config::validate'),pico('config::to_runtime'))
  driver_initialize=pico('Sk6812RgbwDriver::initialize')+pico('Sk6812RgbwDriver::reset_to_known_idle_state')
  claim_runtime=pico('RendererPublicationBackend::acquire_new_resources')+pico('LedOutputManager::ensure_drivers')+max(driver_initialize,pico('Sk6812RgbwDriver::claim_dma_channel'))
  manager_configure=pico('LedOutputManager::configure')+max(pico('LedStrip::configure'),pico('LedStrip::bind_slices'))
  led_runtime=pico('RendererPublicationBackend::switch_led')+manager_configure
  effect_stage=pico('EffectEngine::stage_scene')+pico('EffectEngine::validate_config')
  idle_stage=pico('IdleLightingController::stage_config')+pico('IdleLightingController::validate_config')
  effects_runtime=pico('RendererPublicationBackend::switch_effects_idle')+max(effect_stage,pico('EffectEngine::apply_pending'),idle_stage,pico('IdleLightingController::apply_pending_config'))
  rollback_runtime=pico('RendererPublicationBackend::rollback')+manager_configure
  publication=pico('activate_prepared')+pico('diagnostic_renderer_publish_configuration')+pico('RuntimePublicationCoordinator::publish')+max(prepare_runtime,claim_runtime,led_runtime,effects_runtime,rollback_runtime)
  renderer_wait=pico('diagnostic_renderer_wait_until_idle')+pico('diagnostic_renderer_service')+pico('LedOutputManager::poll_frame_completion')
  led_safe=pico('FirmwareConfigRuntime::acquire_led')+renderer_wait
  audio_safe=pico('FirmwareConfigRuntime::acquire_audio')+pico('audio_capture_pause')
  restore=pico('FirmwareConfigRuntime::restore')+max(pico('audio_capture_resume'),pico('diagnostic_renderer_set_enabled'))
  safe_points=max(pico('FirmwareConfigRuntime::prepare_activation'),led_safe,audio_safe,publication,restore)
  paths['save']+=safe_points
  paths['preview']+=safe_points
  paths['discard']+=safe_points
  paths['reset']+=safe_points
  print(f'stack_runtime_publication_compiler_path_arm_bytes={publication}')
 for name,value in paths.items():
  print(f'stack_{name}_compiler_path_upper_bound_arm_bytes={value}')
 print(f'stack_largest_single_frame_arm_bytes={max(frames.values())}')
 print('config_stack_report=PASS')
