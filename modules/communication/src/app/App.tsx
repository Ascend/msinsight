/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import React, { useEffect, useLayoutEffect, useState } from 'react';
import { observer } from 'mobx-react';
import { Empty, Spin } from '@insight/lib/components';
import { Alert } from 'antd';
import { ThemeProvider } from '@emotion/react';
import { SharedConfigProvider } from '@insight/lib';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import i18n from '@insight/lib/i18n';
import { useTranslation } from 'react-i18next';
import { CommunicationStoreProvider, useCommunicationStore } from '../store/context';
import { Analysis } from './Analysis';
import type { Services } from './services';
import { Selector, options } from '../components/Selector';
import { useQuery } from '../state/useQuery';
import { PageFilterRow, Page, PageContent, ParsingStatus } from './styles';

const ClusterFilter = observer(function ClusterFilter({ services }: { services: Services }): JSX.Element {
    const { t } = useTranslation('communication');
    const store = useCommunicationStore();
    return <Selector label={t('searchCriteria.Cluster')} value={store.clusterPath} width="auto"
        options={store.clusters.map(item => ({ value: item.path, label: item.name }))} onChange={value => {
            store.selectCluster(value);
            services.bridge.send('clusterPage/selectCluster', { project: store.project, clusterPath: value });
        }} />;
});

const ClusterStatus = observer(function ClusterStatus(): JSX.Element {
    const { t } = useTranslation('communication');
    const store = useCommunicationStore();
    if (store.compare) { return <Alert type="info" message={t('framework:clusterView.compareUnavailable')} />; }
    if (!store.clusterPath) { return <Empty description={t('modern.selectData')} />; }
    return <ParsingStatus role="status" aria-live="polite">
        <Spin />
        <div>{t('modern.clusterParsing')}<p>{t('modern.clusterParsingHint')}</p></div>
    </ParsingStatus>;
});

const ClusterPage = observer(function ClusterPage({ services }: { services: Services }): JSX.Element {
    const { t } = useTranslation('communication');
    const store = useCommunicationStore();
    const { clusterPath, generation, iterationId } = store;
    const ready = Boolean(clusterPath) && store.parsed && !store.compare;
    const iterations = useQuery(JSON.stringify([generation, clusterPath]), ready, signal => services.api.iterations(clusterPath, signal));
    useEffect(() => {
        if (iterations.data) { store.resolveIteration(generation, iterations.data); }
    }, [store, generation, iterationId, iterations.data]);
    const selectedIteration = iterations.data?.includes(iterationId) ? iterationId : '';
    return <>
        <PageFilterRow>
            <ClusterFilter services={services} />
            {ready && <Selector label={t('searchCriteria.Step')} value={selectedIteration} options={options(iterations.data ?? [])}
                loading={iterations.loading} onChange={store.selectIteration} />}
        </PageFilterRow>
        <PageContent>{ready
            ? <Analysis key={generation} services={services} iterations={iterations} iterationId={selectedIteration} />
            : <ClusterStatus />}</PageContent>
    </>;
});

const Content = observer(function Content({ services }: { services: Services }): JSX.Element {
    const store = useCommunicationStore();
    const { i18n: translation } = useTranslation('communication');
    const locale = store.language === 'zh' ? 'zhCN' : 'enUS';
    const [canDisplay, setCanDisplay] = useState(false);
    useEffect(() => {
        if (store.initialized && translation.language === locale) { setCanDisplay(true); }
    }, [store.initialized, translation.language, locale]);
    // Gate only the initial mount; later language changes must not remount charts or refetch data.
    return <Page data-theme={store.dark ? 'dark' : 'light'}>
        {canDisplay
            ? <ClusterPage services={services} />
            : <ParsingStatus role="status" aria-busy="true"><Spin delay={150} /></ParsingStatus>}
    </Page>;
});

const PageTheme = observer(function PageTheme({ children }: { children: React.ReactNode }): JSX.Element {
    const { dark, language } = useCommunicationStore();
    useLayoutEffect(() => { window.setTheme(dark); }, [dark]);
    const locale = language === 'zh' ? 'zhCN' : 'enUS';
    useEffect(() => { void i18n.changeLanguage(locale); }, [locale]);
    return <ThemeProvider theme={themeInstance.getTheme()[dark ? 'dark' : 'light']}>
        <GlobalStyles /><SharedConfigProvider locale={locale}>{children}</SharedConfigProvider>
    </ThemeProvider>;
});

export function App({ services }: { services: Services }): JSX.Element {
    useEffect(() => { services.bridge.start(); return () => services.bridge.dispose(); }, [services.bridge]);
    return <CommunicationStoreProvider value={services.store}><PageTheme><Content services={services} /></PageTheme></CommunicationStoreProvider>;
}
