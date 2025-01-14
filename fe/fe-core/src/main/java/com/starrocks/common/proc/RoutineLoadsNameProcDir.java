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

// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/fe/fe-core/src/main/java/org/apache/doris/common/proc/RoutineLoadsNameProcDir.java

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package com.starrocks.common.proc;

import com.google.common.base.Preconditions;
import com.starrocks.common.AnalysisException;
import com.starrocks.load.routineload.RoutineLoadJob;
import com.starrocks.load.routineload.RoutineLoadManager;
import com.starrocks.server.GlobalStateMgr;
import com.starrocks.sql.ast.ShowRoutineLoadStmt;

import java.util.List;

/*
    SHOW PROC "/routine_loads/{jobName}"
    show all of routine load named job name in all of db

    RESULT
    show result is sames as show routine load {jobName}
 */
public class RoutineLoadsNameProcDir implements ProcDirInterface {

    private final String jobName;

    public RoutineLoadsNameProcDir(String jobName) {
        this.jobName = jobName;
    }

    @Override
    public boolean register(String name, ProcNodeInterface node) {
        return false;
    }

    @Override
    public ProcNodeInterface lookup(String jobIdStr) throws AnalysisException {
        Preconditions.checkNotNull(jobIdStr);

        long jobId;
        try {
            jobId = Long.valueOf(jobIdStr);
        } catch (NumberFormatException e) {
            throw new AnalysisException("Invalid job id format: " + jobIdStr);
        }

        return new RoutineLoadProcNode(jobId);
    }

    @Override
    public ProcResult fetchResult() throws AnalysisException {
        BaseProcResult baseProcResult = new BaseProcResult();
        baseProcResult.setNames(ShowRoutineLoadStmt.getTitleNames());

        // find all of job named routine load
        RoutineLoadManager routineLoadManager = GlobalStateMgr.getCurrentState().getRoutineLoadManager();
        List<RoutineLoadJob> routineLoadJobList = routineLoadManager.getJobByName(jobName);
        for (RoutineLoadJob routineLoadJob : routineLoadJobList) {
            baseProcResult.addRow(routineLoadJob.getShowInfo());
        }

        return baseProcResult;
    }
}
