#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "column/vectorized_fwd.h"
#include "common/object_pool.h"
#include "exec/hash_join_components.h"
#include "exec/pipeline/hashjoin/hash_join_probe_operator.h"
#include "exec/spill/partition.h"
#include "runtime/runtime_state.h"
#include "util/runtime_profile.h"

namespace starrocks::pipeline {

struct NoBlockCountDownLatch {
    void reset(int32_t total) { _count_down = total; }

    void count_down() {
        _count_down--;
        DCHECK_GE(_count_down, 0);
    }

    bool ready() const { return _count_down == 0; }

private:
    std::atomic_int32_t _count_down{};
};

struct SpillableHashJoinProbeMetrics {
    RuntimeProfile::Counter* hash_partitions = nullptr;
    RuntimeProfile::Counter* probe_shuffle_timer = nullptr;
};

class SpillableHashJoinProbeOperator final : public HashJoinProbeOperator {
public:
    template <class... Args>
    SpillableHashJoinProbeOperator(Args&&... args) : HashJoinProbeOperator(std::forward<Args>(args)...) {}
    ~SpillableHashJoinProbeOperator() override = default;

    Status prepare(RuntimeState* state) override;

    void close(RuntimeState* state) override;

    bool has_output() const override;

    bool need_input() const override;

    bool is_finished() const override;

    Status set_finishing(RuntimeState* state) override;

    Status set_finished(RuntimeState* state) override;

    bool pending_finish() const override;

    Status push_chunk(RuntimeState* state, const ChunkPtr& chunk) override;

    StatusOr<ChunkPtr> pull_chunk(RuntimeState* state) override;

    void set_probe_spiller(std::shared_ptr<spill::Spiller> spiller) { _probe_spiller = std::move(spiller); }

private:
    void set_spill_strategy(spill::SpillStrategy strategy) { _join_builder->set_spill_strategy(strategy); }
    spill::SpillStrategy spill_strategy() const { return _join_builder->spill_strategy(); }

    void _acquire_next_partitions();

    bool _all_loaded_partition_data_ready();

    bool _all_partition_finished() const;

    Status _load_all_partition_build_side(RuntimeState* state);

    Status _load_partition_build_side(RuntimeState* state, const std::shared_ptr<spill::SpillerReader>& reader,
                                      size_t idx);

    void _update_status(Status&& status);

    Status _status() const;

    Status _push_probe_chunk(RuntimeState* state, const ChunkPtr& chunk);

private:
    SpillableHashJoinProbeMetrics metrics;

    std::vector<const SpillPartitionInfo*> _build_partitions;
    std::unordered_map<int32_t, const SpillPartitionInfo*> _pid_to_build_partition;
    std::vector<const SpillPartitionInfo*> _processing_partitions;
    std::unordered_set<int32_t> _processed_partitions;

    std::vector<std::shared_ptr<spill::SpillerReader>> _current_reader;
    std::vector<bool> _eofs;
    std::shared_ptr<spill::Spiller> _probe_spiller;

    ObjectPool _component_pool;
    std::vector<HashJoinProber*> _probers;
    std::vector<HashJoinBuilder*> _builders;
    std::unordered_map<int32_t, int32_t> _pid_to_process_id;

    bool _is_finished = false;
    bool _is_finishing = false;

    NoBlockCountDownLatch _latch;
    mutable std::mutex _mutex;
    Status _operator_status;

    std::shared_ptr<spill::IOTaskExecutor> _executor;

    ChunkPtr _staging_chunk;
};

class SpillableHashJoinProbeOperatorFactory : public HashJoinProbeOperatorFactory {
public:
    template <class... Args>
    SpillableHashJoinProbeOperatorFactory(Args&&... args) : HashJoinProbeOperatorFactory(std::forward<Args>(args)...){};

    ~SpillableHashJoinProbeOperatorFactory() override = default;

    Status prepare(RuntimeState* state) override;
    OperatorPtr create(int32_t degree_of_parallelism, int32_t driver_sequence) override;

private:
    std::shared_ptr<spill::SpilledOptions> _spill_options;
    std::shared_ptr<spill::SpillerFactory> _spill_factory = std::make_shared<spill::SpillerFactory>();
};

} // namespace starrocks::pipeline