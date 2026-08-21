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
import type { Theme } from '@emotion/react';
export const light: Theme = {
    mode: 'light',
    backgroundColor: '#FFFFFF',
    timeInsightIcon: '#A5D61D', // time Insight 图标颜色
    allocationInsightIcon: '#ED955F', // allocation Insight 图标颜色
    contentBackgroundColor: '#FDFEFF',
    buttonBackgroundColor: '#E5E6E8',
    okBackgroundColor: '#0A58F6',
    selectBackgroundColor: '#FFFFFF',
    cardHeadBackgroundColor: '#FFFFFF',
    fontColor: 'rgba(0, 0, 0, 0.9)',
    disabledFontColor: 'rgb(171, 171, 171)',
    tableHeadFontColor: 'rgb(0, 0, 0, 0.6)',
    subtitleColor: 'rgb(0, 0, 0, 0.5)',
    solidLine: 'rgb(0, 0, 0, 0.2)',
    buttonFontColor: '#0A59F7',
    svgBackgroundColor: 'rgb(0,0,0,0.9)',
    svgPlayBackgroundColor: '#000000',
    scrollbarColor: '#C1C2C4',
    tableRowSelect: '#D9DADC',
    devicePullDown: '#1A1819',
    switchIconColor: '#18181A',
    templateBackgroundColor: 'rgb(255, 255, 255)',
    templateSVGFillColor: 'rgba(0, 0, 0, 0.6)',
    selectedTemplateSVGFillColor: 'rgba(255, 255, 255)',
    selectedChartColor: 'black',
    frameRelativeLineColor: 'rgb(0, 0, 0, 0.5)', // frame 连线颜色
    selectJankColor: '#C0250D', // 异常帧选中之后的边框颜色
    selectNoJankColor: '#25A707', // 正常帧选中之后的边框颜色
    selectedChartBackgroundColor: '#ACC3F5',
    selectedChartBorderColor: '#0959f4',
    deviceProcessBackgroundColor: '#FFFFFF',
    deviceProcessContentFontColor: 'rgb(0, 0, 0, 0.9)', // 设备和进程框里字体的颜色
    deviceProcessActiveFontColor: 'rgb(0, 0, 0, 0.9)', // 设备和进程下拉框选中范围的字体颜色
    deviceProcessNotActiveFontColor: 'rgb(150, 150, 150, 0.9)', // 设备和进程下拉框未选中范围的字体颜色
    searchBackgroundColor: 'rgb(0, 0, 0, 0.1)',
    tableBorderColor: 'rgb(0, 0, 0, 0.1)',
    backIconBackgroundColor: 'rgb(0, 0, 0)',
    arrowUnexpandedBgColor: 'rgb(0, 0, 0, 0.5)',
    insightHeaderButtonBackgroundColor: 'rgb(217, 218, 220)',
    timelineAxisColor: '#C3CEDF',
    searchInputCaretColor: '#0A59F7',
    allSessionHeadBgColor: 'rgba(0, 0, 0, 0.05)',
    agentWelcomeCardBackgroundColor: 'rgba(0, 0, 0, 0.03)',
    deviceProcessActiveBackgroundColor: '#FFFFFF', // 设备和进程下拉框选中范围的背景颜色
    placeholderFontColor: 'rgb(150, 150, 150, 0.9)',
    unitTagInfoBackgroundColor: 'rgb(0, 0, 0, 0.15)',
    tooltipBGColor: 'rgb(255, 255, 255)',
    chartWrongBGColor: 'rgba(232, 64, 38, 0.1)',
    systemEventColor: '#ED6F21', // 系统事件背景色
    thumbIconBackgroundColor: 'rgb(51, 51, 51)',
    thumbEntranceBackgroundColor: 'rgb(64, 64, 64)',
    toolTipBackgroundColor: 'rgb(241, 243, 245)',
    toolTipShadowColor: 'rgba(200, 200, 200, 0.2)',
    timeMakerListToolTipBackgroundColor: '#404040',
    shadowBackgroundColor: 'rgb(121, 121, 121)',
    enclosureBorder: 'rgb(201, 201, 201)', // 封闭框线颜色，如：Input 组件、Radio 按钮
    controllerBarBackgroundColor: 'rgb(235, 235, 235)', // Detail区域上方控制栏背景色
    activeButtonBackgroundColor: 'rgb(0, 0, 0)',
    disableButtonBackgroundColor: 'rgb(127, 127, 127)',
    fpsPointersColor: 'rgb(0, 0 0, 0.7)', // FPS指针
    fpsTextColor: 'rgb(0, 0, 0)', // FPS文字颜色
    fpsColor: 'rgb(0, 0, 0, 0.5)', // FPS本身颜色
    categories: {
        system: { name: 'System', color: '#0A59F7', background: '#E7EFFE' }, // 系统分类标签颜色与背景色
        arkTS: { name: 'ArkTS', color: '#64BB5C', background: '#EFF6EE' }, // JS分类标签颜色与背景色
        NAPI: { name: 'NAPI', color: '#ED6F21', background: '#FBF0EC' }, // NAPI分类标签颜色与背景色
        native: { name: 'Native', color: '#ED6F21', background: '#FBF0EC' }, // Native分类标签颜色与背景色
    },
    searchIconBackgroundColor: 'rgb(0, 0, 0)',
    cancelIconBackgroundColor: 'rgb(0, 0, 0, 0.6)',
    dividerColor: '#fff',
    closeDragContainerBG: 'rgb(229, 230, 232)',
    tooltipFontColor: 'rgba(0, 0, 0, .6)',
    tooltipBoxShadow: '0 4px 16px 0 rgba( 0, 0, 0, .16 )',
    colorPalette: {
        slateblue: '#564AF7',
        royalblue: '#0A59F7',
        skyblue: '#46B1E3',
        turquoise: '#61CFBE',
        olivedrab: '#64BB5C',
        yellowgreen: '#A5D61D',
        gold: '#F7CE00',
        orange: '#F9A01E',
        coral: '#ED6F21',
        orangered: '#E84026',
        palevioletred: '#E64566',
        mediumorchid: '#AC49F5',
        transparentMask: '#CCCCCC',
        pink: '#E67C92',
        otherColor: 'rgb(220, 220, 220)',
        yellow: '#b09239',
        coralRed: '#FF5432',
        deepBlue: '#0062DC',
        tealGreen: '#279C6E',
        aquaBlue: '#34B1B9',
        vividBlue: '#0077FF',
        vividRed: '#FD2F2F',
        raspberryPink: '#D53F78',
        skyBlue: '#2F9CE0',
        royalPurple: '#6037DB',
        sunsetOrange: '#EE891D',
        amethystPurple: '#AA38CE',
        limeGreen: '#75A105',
    },
    grayscaleColor: '#CCCCCC',
    filterColor: 'rgba(0, 0, 0, .9)', // 过滤模块字体颜色
    filterTipColor: 'rgba(0, 0, 0, .6)', // 过滤模块提示文字字体颜色
    multiSelectBgColor: 'rgba(0, 0, 0, .15)', // 多选列表item背景色
    multiSelectUnCheckedBgColor: 'rgba(255, 255, 255, .4)', // 多选列表item单选框未选中背景色
    multiSelectUnCheckedBorderColor: 'rgba(0, 0, 0, .4)', // 多选列表item单选框未选中边框色
    timeDiffBackgroundColor: 'rgb(150, 150, 150, 0.9)', // 时间差背景颜色
    timeDiffPictureColor: 'rgba(0, 0, 0, 0.82)', // 时间差箭头颜色
    filterIconColor: 'rgb(0, 0, 0, .6)', // 过滤图标背景色
    filterSelectActiveBgColor: 'rgb(0, 0, 0, .1)',
    searchContainerBorder: '1px solid rgb(0, 0, 0, .3)',
    searchHeaderBgColor: 'transparent',
    listItemHoverColor: 'rgb(0, 0, 0, 0.7)',
    flagListSelectedColor: 'rgb(164,161,152)',
    flagListHoverColor: 'rgb(176, 176, 176)',
    colorSelectedBorder: '1px solid #000000',
    schedulingLineColor: 'black',
    frameExpectColor: 'rgb(0, 0, 0, 0.9)',
    buttonColor: {
        enableClickColor: 'rgba(0,0,0,0.6)',
        disableClickColor: 'rgba(0,0,0,0.2)',
        emphasizeColor: '#5291FF',
        suspendBGColor: 'rgba(0,0,0,0.1)',
        unSuspendBGColor: 'transparent',
    },
    otherColor: 'rgb(192,192,192)',
    switchOpen: '#0A59F7',
    switchClose: '#E1E3E6',

    primaryColor: '#0077FF',
    primaryColorLight1: '#79bbff',
    primaryColorLight2: '#a0cfff',
    primaryColorLight3: '#c6e2ff',
    primaryColorLight4: '#d9ecff',
    primaryColorLight5: '#ecf5ff',
    primaryColorLight6: '#3291FE',
    primaryColorDark: '#337ecc',
    primaryColorHover: '#3291FE',
    primaryColorDisabled: '#A0CFFF',
    successColor: '#24AB36',
    successColorLight1: '#95d475',
    successColorLight2: '#b3e19d',
    successColorLight3: '#d1edc4',
    successColorLight4: '#e1f3d8',
    successColorLight5: '#f0f9eb',
    successColorDark: '#529b2e',
    warningColor: '#EBAF00',
    warningColorLight1: '#eebe77',
    warningColorLight2: '#f3d19e',
    warningColorLight3: '#f8e3c5',
    warningColorLight4: '#faecd8',
    warningColorLight5: '#fdf6ec',
    warningColorDark: '#b88230',
    dangerColor: '#E32020',
    dangerColorLight1: '#f89898',
    dangerColorLight2: '#fab6b6',
    dangerColorLight3: '#fcd3d3',
    dangerColorLight4: '#fde2e2',
    dangerColorLight5: '#fef0f0',
    dangerColorDark: '#c45656',
    infoColor: '#576372',
    infoColorLight1: '#b1b3b8',
    infoColorLight2: '#c8c9cc',
    infoColorLight3: '#dedfe0',
    infoColorLight4: '#e9e9eb',
    infoColorLight5: '#f4f4f5',
    infoColorDark: '#73767a',
    majorColor: '#F97611',
    majorColorLight1: '#FBA25D',
    bgColor: '#FDFEFF',
    bgColorDark: '#EBEFF6',
    bgColorGrey: '#8D98AA',
    bgColorLight: '#F4F6FA',
    bgColorLighter: '#ffffff',
    bgColorDisabled: '#EBEDF3',
    bgColorCommon: '#EBEFF6',
    textColor: '#4E5865',
    textColorPrimary: '#000000',
    textColorSecondary: '#4E5865',
    textColorTertiary: '#8D98AA',
    textColorFourth: '#ffffff',
    textColorPlaceholder: '#BFC7D7',
    textColorPlaceholderLight: '#EBEDF3',
    textColorDisabled: '#c0c4cc',
    textColorMenu: '#000000',
    borderColor: '#E4EAF1',
    borderColorLight: '#DFE5EF',
    borderColorLighter: '#C3CEDF',
    borderColorExtraLight: '#B3BFD3',
    borderColorDisabled: '#BFC7D7',
    boxShadow: '0px 12px 32px 4px rgba(0, 0, 0, 0.04), 0px 8px 20px rgba(0, 0, 0, 0.08)',
    boxShadowLight: '0px 0px 12px rgba(0, 0, 0, 0.12)',
    boxShadowLighter: '0px 0px 6px rgba(0, 0, 0, 0.12)',
    boxShadowDark: '0px 16px 48px 16px rgba(0, 0, 0, 0.08), 0px 12px 32px rgba(0, 0, 0, 0.12), 0px 8px 16px -8px rgba(0, 0, 0, 0.16)',
    boxShadowDropDown: '0 8px 16px 0 rgba(0, 0, 0, 0.10)',
    maskColor: 'rgb(255, 255, 255, 0.55)',
    maskColorExtraLight: 'rgba(255, 255, 255, 0.3)',
    borderColorHover: '#c0c4cc',
    fontSizeExtraLarge: '20px',
    fontSizeLarge: '18px',
    fontSizeMedium: '16px',
    fontSizeBase: '14px',
    fontSizeSmall: '13px',
    fontSizeExtraSmall: '12px',
    fontFamily: 'Inter, -apple-system, BlinkMacSystemFont, \'Segoe UI\', Roboto, Oxygen, Ubuntu, Cantarell, \'Fira Sans\', \'Droid Sans\', sans-serif',
    borderRadiusBase: '4px',
    borderRadiusSmall: '2px',
    borderRadiusLarge: '8px',
    borderRadiusRound: '20px',
    borderRadiusCircle: '100%',
    pagePadding: '16px',
    icon: '#8D98AA',
    iconColor: '#4E5865',
    tableTextColor: '#000000',
    radioSelectedColor: '#007AFF',
    rankBackgroudColor: '#FDFEFF',
    contextMenuBgColor: '#EBEFF6',
    summaryChartBgColor: '#BFC7D7',
    rankContainerBackgroudColor: '#EBEFF6',
    unitInfoTextColor: '#4E5865',
    scrollBarWidth: 7,
    splitLineColor: '#DFE5EF',
    selectedBgColor: '#EBEFF6',
};
