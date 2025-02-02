// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/wasm/c-api.h"
#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

bool g_callback_called;

own<Trap> Callback(const Val args[], Val results[]) {
  g_callback_called = true;
  return nullptr;
}

}  // namespace

TEST_F(WasmCapiTest, Serialize) {
  FunctionSig sig(0, 0, nullptr);
  uint32_t callback_index =
      builder()->AddImport(base::CStrVector("callback"), &sig);
  byte code[] = {WASM_CALL_FUNCTION0(callback_index)};
  AddExportedFunction(base::CStrVector("run"), code, sizeof(code), &sig);
  Compile();

  vec<byte_t> serialized = module()->serialize();
  EXPECT_TRUE(serialized);  // Serialization succeeded.

  // We reset the module and collect it to make sure the NativeModuleCache does
  // not contain it anymore. Otherwise deserialization will not happen.
  ResetModule();
  Heap* heap =
      reinterpret_cast<::wasm::StoreImpl*>(store())->i_isolate()->heap();
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
  heap->PreciseCollectAllGarbage(Heap::kForcedGC,
                                 GarbageCollectionReason::kTesting);
  heap->PreciseCollectAllGarbage(Heap::kForcedGC,
                                 GarbageCollectionReason::kTesting);
  own<Module> deserialized = Module::deserialize(store(), serialized);

  // Try to serialize the module again. This can fail if deserialization does
  // not set up a clean state.
  deserialized->serialize();

  own<FuncType> callback_type =
      FuncType::make(ownvec<ValType>::make(), ownvec<ValType>::make());
  own<Func> callback = Func::make(store(), callback_type.get(), Callback);
  Extern* imports[] = {callback.get()};

  own<Instance> instance = Instance::make(store(), deserialized.get(), imports);
  ownvec<Extern> exports = instance->exports();
  Func* run = exports[0]->func();
  g_callback_called = false;
  run->call();
  EXPECT_TRUE(g_callback_called);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
