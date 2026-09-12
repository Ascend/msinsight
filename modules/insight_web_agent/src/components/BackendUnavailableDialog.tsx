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
import { ACP_STATUS_I18N_KEY, showsNodeDownload, type AcpUnavailableReason } from '../acpStatus';
import { subscribeBackendUnavailable, type BackendConnectionFailure } from '../backendConnection';

export const BackendUnavailableDialog = (): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const [failure, setFailure] = useState<BackendConnectionFailure>();

    useEffect(() => subscribeBackendUnavailable(setFailure), []);

    const reason: AcpUnavailableReason = failure?.status ?? 'unreachable';
    const i18nKey = ACP_STATUS_I18N_KEY[reason];

    return (
        <Modal
            cancelButtonProps={{ style: { display: 'none' } }}
            maskClosable={false}
            okText={t('dismiss')}
            onCancel={() => setFailure(undefined)}
            onOk={() => setFailure(undefined)}
            open={Boolean(failure)}
            title={t(`acpUnavailable.${i18nKey}.title`)}
        >
            <p>{t(`acpUnavailable.${i18nKey}.description`, { version: failure?.nodeVersion || '' })}</p>
            <p>{t(`acpUnavailable.${i18nKey}.action`)}</p>
            {showsNodeDownload(reason) ? (
                <p>
                    <strong>{t('nodeDownloadAddress')}</strong>
                    <br />
                    <code style={{ overflowWrap: 'anywhere', userSelect: 'text' }}>{t('nodeDownloadUrl')}</code>
                </p>
            ) : null}
        </Modal>
    );
};
