/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import React from 'react';
import { getMemoryApplicationSrc, type MemoryApplicationName } from '../../moduleConfig';

interface Props {
    application: MemoryApplicationName;
    visible: boolean;
    attributes?: React.IframeHTMLAttributes<HTMLIFrameElement>;
    onLoad: React.ReactEventHandler<HTMLIFrameElement>;
}

export const MemoryApplicationFrame = ({ application, visible, attributes, onLoad }: Props): React.ReactElement =>
    <iframe {...attributes} key={application} id={application} name={application}
        title={application} style={{ display: visible ? 'block' : 'none' }} data-memory-application={application}
        onLoad={onLoad} src={getMemoryApplicationSrc(application)} />;
