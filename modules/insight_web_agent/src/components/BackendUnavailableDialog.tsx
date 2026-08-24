/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { Modal } from 'antd';
import { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { subscribeBackendUnavailable, type BackendConnectionFailure } from '../backendConnection';

export const BackendUnavailableDialog = (): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const [failure, setFailure] = useState<BackendConnectionFailure>();

    useEffect(() => subscribeBackendUnavailable(setFailure), []);

    return (
        <Modal
            cancelButtonProps={{ style: { display: 'none' } }}
            maskClosable={false}
            okText={t('dismiss')}
            onCancel={() => setFailure(undefined)}
            onOk={() => setFailure(undefined)}
            open={Boolean(failure)}
            title={t('nodeBackendUnavailableTitle')}
        >
            <p>{t('nodeBackendUnavailableDescription')}</p>
            <p>{t('nodeBackendUnavailableAction')}</p>
            <p>
                <strong>{t('nodeDownloadAddress')}</strong>
                <br />
                <code style={{ overflowWrap: 'anywhere', userSelect: 'text' }}>{t('nodeDownloadUrl')}</code>
            </p>
        </Modal>
    );
};
