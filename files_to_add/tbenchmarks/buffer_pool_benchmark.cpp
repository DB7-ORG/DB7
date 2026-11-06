#include <benchmark/benchmark.h>
#include <random>

#include "storage/buffer_pool/buffer_pool.hpp"

using namespace db7;
using namespace db7::storage;

static constexpr const char *kDataDir = ".data_benchmark_buffer_pool";
static std::unique_ptr<DiskManagerAsync> dm_;
static std::unique_ptr<DiskScheduler> sched_;
static std::unique_ptr<PageVersionManager> vm_;
static std::unique_ptr<BufferPool> buffer_pool_;
static table_id tbl_ = 1;
static u32 DATA_PAGE_COUNT = 10'000;

static void Populate(u32 size, u32 pool_size) {
  dm_->CreateOpenFile(tbl_, size);

  for (u32 i = 1; i < size; ++i) {
    PageIdentifier id{tbl_, i};
    PinnedPage page = buffer_pool_->Pin(id, PageKind::Heap);
    std::memset(page->GetData(), 0xAB, DB7_PAGE_SIZE);
  }

  auto *pages = buffer_pool_->GetPagesDebug();
  for (u32 i = 0; i < pool_size; i++) {
    auto &page = pages[i];
    if (page.GetPageId() == 0) continue;

    auto next = Page::SetWriteInProgress(page.State());
    page.StoreState(next);
    sched_->Enqueue(IoTask(IoTask::WRITE, IoPriority::HIGH, &page, page.GetId()));
  }

  for (u32 i = 0; i < pool_size; i++) {
    auto &page = pages[i];
    if (page.GetPageId() != 0) page.WaitWrite();
  }
}

static void Setup(const benchmark::State &state) {
  const auto pool_size = static_cast<u32>(state.range(0));

  dm_ = std::make_unique<DiskManagerAsync>(kDataDir);
  sched_ = std::make_unique<DiskScheduler>(dm_.get());
  sched_->Start();
  vm_ = std::make_unique<PageVersionManager>();
  buffer_pool_ = std::make_unique<BufferPool>(sched_.get(), vm_.get(), pool_size);

  Populate(DATA_PAGE_COUNT, pool_size);
}

static void Teardown(const benchmark::State &) {
  buffer_pool_.reset();
  vm_.reset();
  sched_.reset();
  dm_.reset();
}

static void SimulateWork(Page *page, i64 passes) {
  const auto *data = reinterpret_cast<const unsigned char *>(page->GetData());
  u64 sum = 0;

  for (i64 pass = 0; pass < passes; ++pass) {
    for (size_t i = 0; i < DB7_PAGE_SIZE; i += 64) { // touch one byte per cache line
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
  }
}

static void PrintStats(benchmark::State &state) {
  if (state.thread_index() == 0) {
    const auto &monitor = buffer_pool_->GetMonitorStats(); // replace with your API
    monitor.Print();
  }
}

static void BM_RandomWorkload(benchmark::State &state) {

  std::mt19937 rng(state.thread_index());
  std::uniform_int_distribution<u32> dist(1, DATA_PAGE_COUNT - 1);

  auto numOps = 1000;

  for (auto _ : state) {
    for (int i = 0; i < numOps; i++) {
      PinnedPage page = buffer_pool_->Pin({tbl_, dist(rng)}, PageKind::Heap);
      benchmark::DoNotOptimize(page);
      SimulateWork(page.get(), 1);
    }
  }

  state.SetItemsProcessed(state.iterations());

  // PrintStats(state);
}

BENCHMARK(BM_RandomWorkload)
    ->Arg(5'000)
    ->Setup(Setup)
    ->Teardown(Teardown)
    ->ThreadRange(1, 16)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
;

BENCHMARK_MAIN();