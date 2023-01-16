// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "exec/pipeline/pipeline.h"

#include "exec/pipeline/pipeline_driver.h"
#include "exec/pipeline/scan/connector_scan_operator.h"
#include "exec/pipeline/stream_pipeline_driver.h"
#include "runtime/runtime_state.h"

namespace starrocks::pipeline {

size_t Pipeline::degree_of_parallelism() const {
    // DOP (degree of parallelism) of Pipeline's SourceOperator determines the Pipeline's DOP.
    return source_operator_factory()->degree_of_parallelism();
}

void Pipeline::count_down_driver(RuntimeState* state) {
    bool all_drivers_finished = ++_num_finished_drivers == _drivers.size();
    if (all_drivers_finished) {
        state->fragment_ctx()->count_down_pipeline(state);
    }
}

void Pipeline::clear_drivers() {
    _drivers.clear();
}

Drivers& Pipeline::drivers() {
    return _drivers;
}

const Drivers& Pipeline::drivers() const {
    return _drivers;
}

void Pipeline::instantiate_drivers(RuntimeState* state) {
    auto* query_ctx = state->query_ctx();
    auto* fragment_ctx = state->fragment_ctx();
    auto workgroup = fragment_ctx->workgroup();
    auto is_stream_pipeline = fragment_ctx->is_stream_pipeline();

    size_t dop = degree_of_parallelism();

    VLOG_ROW << "Pipeline " << to_readable_string() << " parallel=" << dop
             << " is_stream_pipeline=" << is_stream_pipeline
             << " fragment_instance_id=" << print_id(fragment_ctx->fragment_instance_id());

    setup_pipeline_profile(state);
    for (size_t i = 0; i < dop; ++i) {
        auto&& operators = create_operators(dop, i);
        DriverPtr driver = nullptr;
        if (is_stream_pipeline) {
            driver = std::make_shared<StreamPipelineDriver>(std::move(operators), query_ctx, fragment_ctx, this,
                                                            fragment_ctx->next_driver_id());

        } else {
            driver = std::make_shared<PipelineDriver>(std::move(operators), query_ctx, fragment_ctx, this,
                                                      fragment_ctx->next_driver_id());
        }
        setup_drivers_profile(driver);
        driver->set_workgroup(workgroup);
        _drivers.emplace_back(std::move(driver));
    }

    query_ctx->query_trace()->register_drivers(fragment_ctx->fragment_instance_id(), _drivers);

    if (!source_operator_factory()->with_morsels()) {
        return;
    }

    auto* morsel_queue_factory = source_operator_factory()->morsel_queue_factory();
    DCHECK(morsel_queue_factory != nullptr);
    DCHECK(dop == 1 || dop == morsel_queue_factory->size());
    for (size_t i = 0; i < dop; ++i) {
        auto& driver = _drivers[i];
        driver->set_morsel_queue(morsel_queue_factory->create(i));
        if (auto* scan_operator = driver->source_scan_operator()) {
            scan_operator->set_workgroup(workgroup);
            scan_operator->set_query_ctx(query_ctx->get_shared_ptr());
            if (dynamic_cast<ConnectorScanOperator*>(scan_operator) != nullptr) {
                if (workgroup != nullptr) {
                    scan_operator->set_scan_executor(state->exec_env()->connector_scan_executor_with_workgroup());
                } else {
                    scan_operator->set_scan_executor(state->exec_env()->connector_scan_executor_without_workgroup());
                }
            } else {
                if (workgroup != nullptr) {
                    scan_operator->set_scan_executor(state->exec_env()->scan_executor_with_workgroup());
                } else {
                    scan_operator->set_scan_executor(state->exec_env()->scan_executor_without_workgroup());
                }
            }
        }
    }
}

void Pipeline::setup_pipeline_profile(RuntimeState* runtime_state) {
    runtime_state->runtime_profile()->add_child(runtime_profile(), true, nullptr);
}

void Pipeline::setup_drivers_profile(const DriverPtr& driver) {
    runtime_profile()->add_child(driver->runtime_profile(), true, nullptr);
    auto* dop_counter = ADD_COUNTER_SKIP_MERGE(runtime_profile(), "DegreeOfParallelism", TUnit::UNIT);
    COUNTER_SET(dop_counter, static_cast<int64_t>(source_operator_factory()->degree_of_parallelism()));
    auto* total_dop_counter = ADD_COUNTER(runtime_profile(), "TotalDegreeOfParallelism", TUnit::UNIT);
    COUNTER_SET(total_dop_counter, dop_counter->value());
    auto& operators = driver->operators();
    for (int32_t i = operators.size() - 1; i >= 0; --i) {
        auto& curr_op = operators[i];
        driver->runtime_profile()->add_child(curr_op->get_runtime_profile(), true, nullptr);
    }
}

void Pipeline::count_down_epoch_finished_driver(RuntimeState* state) {
    bool all_drivers_finished = ++_num_epoch_finished_drivers == _drivers.size();
    if (all_drivers_finished) {
        state->fragment_ctx()->count_down_epoch_pipeline(state);
    }
}

Status Pipeline::reset_epoch(RuntimeState* state) {
    _num_epoch_finished_drivers = 0;
    for (const auto& driver : drivers()) {
        DCHECK_EQ(driver->driver_state(), pipeline::DriverState::EPOCH_FINISH);
        DCHECK(down_cast<pipeline::StreamPipelineDriver*>(driver.get()));
        auto* stream_driver = down_cast<pipeline::StreamPipelineDriver*>(driver.get());
        RETURN_IF_ERROR(stream_driver->reset_epoch(state));
    }
    return Status::OK();
}

} // namespace starrocks::pipeline