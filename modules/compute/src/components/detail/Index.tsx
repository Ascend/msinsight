/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { observer } from 'mobx-react';
import { Layout } from '@insight/lib/components';
import { type Session } from '../../entity/session';
import BaseInfo from './BaseInfo';
import ComputeWorkload from './ComputeWorkload/Index';
import MemoryWorkload from './MemoryWorkload/Index';
import CoreOccupancy from './CoreOccupancy/Index';
import Roofline from './Roofline/Index';
import TopWarpStallReason from './TopWarpStallReason/Index';

const index = observer(({ session }: { session: Session }): JSX.Element => {
    return (
        <Layout>
            <BaseInfo session={session}/>
            <CoreOccupancy session={session}/>
            <Roofline session={session}/>
            <ComputeWorkload session={session}/>
            <TopWarpStallReason session={session}/>
            <MemoryWorkload session={session}/>
        </Layout>
    );
});

export default index;
