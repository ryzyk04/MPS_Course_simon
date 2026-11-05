"""Host checks of the local protocol and firmware (requires g++)."""
from pathlib import Path
import shutil
import subprocess
from host_stubs import write_uno_stubs

project = Path(__file__).resolve().parents[1]
out = project / ".pio/bus-checks"
out.mkdir(parents=True, exist_ok=True)
write_uno_stubs(out)
common = r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include "puzzle_i2c.h"
void deliver(uint8_t type,uint64_t round,uint16_t seed,uint8_t difficulty,uint8_t op=ModuleBus::GENERATE) {
  ModuleBus::Frame f;f.type=type;f.operation=op;f.round=round;f.seed=seed;f.difficulty=difficulty;
  uint8_t bytes[ModuleBus::FRAME_SIZE];ModuleBus::encode(f,bytes);Wire.inject(bytes,sizeof(bytes));
}
ModuleBus::Frame response() {
  auto bytes=Wire.request();ModuleBus::Frame f;
  assert(ModuleBus::decode(bytes.data(),bytes.size(),f));return f;
}
'''
protocol = r'''
int main() {
  PuzzleI2c endpoint;endpoint.begin(ModuleBus::SIMON,ModuleBus::SIMON_ADDRESS);
  assert(response().state==ModuleBus::WAITING);
  deliver(ModuleBus::SIMON,4294967297ULL,42,2);
  assert(endpoint.status.round==0); // No work inside onReceive ISR.
  assert(endpoint.update()==PuzzleI2c::NEW_GAME);
  assert(response().round==4294967297ULL && response().seed==42 && response().difficulty==2);
  endpoint.smallError();endpoint.smallError();endpoint.solved();
  assert(response().errors==2 && response().state==ModuleBus::SOLVED);
  deliver(ModuleBus::SIMON,4294967297ULL,42,2);
  assert(endpoint.update()==PuzzleI2c::NONE && response().errors==2);
  deliver(ModuleBus::LABYRINTH,10,1234,0);
  assert(endpoint.update()==PuzzleI2c::NONE);
  ModuleBus::Frame f;f.type=ModuleBus::SIMON;f.operation=ModuleBus::GENERATE;f.round=99;
  uint8_t data[ModuleBus::FRAME_SIZE];ModuleBus::encode(f,data);
  for(unsigned i=0;i<sizeof(data);++i) {
    data[i]^=1;Wire.inject(data,sizeof(data));assert(endpoint.update()==PuzzleI2c::NONE);data[i]^=1;
  }
  Wire.inject(data,sizeof(data)-1);assert(endpoint.update()==PuzzleI2c::NONE);
  deliver(ModuleBus::SIMON,10,0,0);assert(endpoint.update()==PuzzleI2c::NEW_GAME);
  assert(response().errors==0 && response().seed==0);
  deliver(ModuleBus::SIMON,9,0,0,ModuleBus::STOP);
  assert(endpoint.update()==PuzzleI2c::NONE);
  deliver(ModuleBus::SIMON,10,0,0,ModuleBus::STOP);
  assert(endpoint.update()==PuzzleI2c::HALT && response().state==ModuleBus::STOPPED);
  endpoint.smallError();assert(response().errors==0);
  deliver(ModuleBus::SIMON,10,0,0,ModuleBus::RESET);
  assert(endpoint.update()==PuzzleI2c::HALT && response().round==0);
  uint8_t module;char event[8];uint64_t round;uint16_t count;
  assert(ModuleBus::parseEvent("G:3:ERROR:18446744073709551615:65535",module,event,round,count));
  assert(module==3 && round==UINT64_MAX && count==65535);
  assert(ModuleBus::parseEvent("E:GACK:2:SOLVED:42:0",module,event,round,count,"E:GACK:"));
  for(auto bad:{"G:1:ERROR:1:1","G:2:ERROR:1:65536","G:2:BOGUS:1:1","G:2:ERROR:-1:1","G:2:ERROR:1:1:1"})
    assert(!ModuleBus::parseEvent(bad,module,event,round,count));
  puts("PASS: CRC, deferred ISR, duplicate rounds, reset/stop, counters and UART envelopes");
}
'''
simon = r'''
#include "src/main.cpp"
int main() {
  setup();assert(!gameActive && response().round==0);
  for(int levelIndex=0;levelIndex<3;++levelIndex) {
    uint64_t round=100+levelIndex;
    deliver(ModuleBus::SIMON,round,42,levelIndex);loop();
    assert(gameActive && stage==0 && display.number==42 && level==levelIndex);
    auto length=seqLen;uint8_t original[6];memcpy(original,seq,length);
    deliver(ModuleBus::SIMON,round,42,levelIndex);loop();assert(stage==0 && !memcmp(original,seq,length));
    onPress(expected[0]%5+1);assert(response().errors==1 && inputPos==0);
    for(int st=0;st<3;++st) {
      for(int i=0;i<seqLen;++i)onPress(expected[i]);
      if(st<2) {
        assert(stagePending);testNow=stageReadyAt;loop();assert(!stagePending);
      }
    }
    assert(response().state==ModuleBus::SOLVED && !gameActive);
    deliver(ModuleBus::SIMON,round+10,42,levelIndex);loop();
    assert(seqLen==length && !memcmp(original,seq,length) && response().errors==0);
  }
  // A reset interrupts the stage transition without waiting 800 ms.
  for(int i=0;i<seqLen;++i)onPress(expected[i]);
  assert(stagePending);
  deliver(ModuleBus::SIMON,0,0,0,ModuleBus::RESET);loop();
  assert(!gameActive && !stagePending && response().state==ModuleBus::WAITING);
  puts("PASS: Simon waits, deterministic stages/all difficulties, errors, solve, reset during pause");
}
'''

compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("g++ required")
for name, checks in (("protocol", protocol), ("simon", simon)):
    cpp = out / f"{name}.cpp"
    cpp.write_text(common + checks, encoding="utf-8")
    exe = out / f"{name}.exe"
    subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-I", str(out), "-I", str(project), "-I", str(project / "include"), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)