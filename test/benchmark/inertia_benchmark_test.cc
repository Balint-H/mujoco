// Copyright 2025 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A benchmark for comparing legacy and two CSR implementations of inertia
// factor and then solve.

#include <benchmark/benchmark.h>
#include <absl/base/attributes.h>
#include <mujoco/mjdata.h>
#include <mujoco/mujoco.h>
#include "src/engine/engine_core_smooth.h"
#include "test/fixture.h"

namespace mujoco {
namespace {

// number of steps to benchmark
static const int kNumBenchmarkSteps = 50;

// ----------------------------- benchmark ------------------------------------

enum class SolveType {
  kLegacy = 0,
  kCsr,
};

static void BM_solve(benchmark::State& state, SolveType type) {
  static mjModel* m;
  m = LoadModelFromPath("../test/benchmark/testdata/inertia.xml");

  mjData* d = mj_makeData(m);
  mj_forward(m, d);

  // allocate input and output vectors
  mj_markStack(d);

  // make CSR matrix
  mjtNum* Ms = mj_stackAllocNum(d, m->nC);
  mjtNum* LDs = mj_stackAllocNum(d, m->nC);
  for (int i=0; i < m->nC; i++) {
    Ms[i] = d->qM[d->mapM2C[i]];
  }

  // arbitrary input vector
  mjtNum *res = mj_stackAllocNum(d, m->nv);
  mjtNum *vec = mj_stackAllocNum(d, m->nv);
  for (int i=0; i < m->nv; i++) {
    vec[i] = 0.2 + 0.3*i;
  }

  // benchmark
  while (state.KeepRunningBatch(kNumBenchmarkSteps)) {
    for (int i=0; i < kNumBenchmarkSteps; i++) {
      switch (type) {
        case SolveType::kLegacy:
          mj_factorI(m, d, d->qM, d->qLD, d->qLDiagInv);
          mj_solveM(m, d, res, vec, 1);
          break;
        case SolveType::kCsr:
          mju_copy(LDs, Ms, m->nC);
          mj_factorIs(LDs, d->qLDiagInv, m->nv,
                      d->C_rownnz, d->C_rowadr, m->dof_simplenum, d->C_colind);
          mju_copy(res, vec, m->nv);
          mj_solveLDs(res, LDs, d->qLDiagInv, m->nv, 1,
                      d->C_rownnz, d->C_rowadr, m->dof_simplenum, d->C_colind);
      }
    }
  }

  // finalize
  mj_freeStack(d);
  mj_deleteData(d);
  mj_deleteModel(m);
  state.SetItemsProcessed(state.iterations());
}

void ABSL_ATTRIBUTE_NO_TAIL_CALL BM_solve_LEGACY(benchmark::State& state) {
  MujocoErrorTestGuard guard;
  BM_solve(state, SolveType::kLegacy);
}
BENCHMARK(BM_solve_LEGACY);

void ABSL_ATTRIBUTE_NO_TAIL_CALL BM_solve_CSR(benchmark::State& state) {
  MujocoErrorTestGuard guard;
  BM_solve(state, SolveType::kCsr);
}
BENCHMARK(BM_solve_CSR);

}  // namespace
}  // namespace mujoco