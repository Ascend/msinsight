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
export const dark: Theme = {
    mode: 'dark',
    backgroundColor: '#1f2329', // 主界面背景色
    timeInsightIcon: '#86AD53', // time Insight 图标颜色
    allocationInsightIcon: '#9E644F', // allocation Insight 图标颜色
    contentBackgroundColor: '#1f2329', // 区域内容背景色
    buttonBackgroundColor: '#4C4C4C', // 模板、按钮背景色
    okBackgroundColor: '#5391FF', // ok按钮背景色
    selectBackgroundColor: '#4c4c4c', // session选中背景色
    cardHeadBackgroundColor: 'rgb(44, 44, 44)', // 卡片表头背景色
    fontColor: 'rgb(177, 177, 177)', // 字体颜色
    disabledFontColor: 'rgb(255, 255, 255, 0.15)',
    tableHeadFontColor: 'rgb(255, 255, 255, 0.6)', // 表格表头字体颜色
    subtitleColor: 'rgb(255, 255, 255, 0.4)', // session小字体颜色
    solidLine: 'rgb(46, 47, 49)', // border分割线颜色
    buttonFontColor: '#5291FF', // 模板、按钮字体颜色
    svgBackgroundColor: '#A3A3A3', // svg设备、homePage图片颜色
    svgPlayBackgroundColor: '#FFFFFF', // svg录制、暂停、删除等图片颜色
    scrollbarColor: '#606060', // 滚动条颜色
    tableRowSelect: '#4C4C4C', // 卡片表格tr选中色
    devicePullDown: '#EBEBEB', // 设备下拉按钮颜色
    switchIconColor: '#959595', // 模板下拉按钮颜色
    selectedChartColor: 'white', // 泳道选择区域左右两边背景色
    frameRelativeLineColor: 'rgb(255, 255, 255, 0.5)', // frame 连线颜色
    selectJankColor: '#FFDDEB', // 异常帧选中之后的边框颜色
    selectNoJankColor: '#EAFFE9', // 正常帧选中之后的边框颜色
    templateBackgroundColor: 'rgb(255, 255, 255, 0.15)', // 模板图标背景色
    templateSVGFillColor: 'rgba(255, 255, 255, 0.6)', // 模板svg图填充色
    selectedTemplateSVGFillColor: 'rgba(255, 255, 255)', // 选中模板svg图填充色
    selectedChartBackgroundColor: '#45649B', // 选中泳道颜色
    selectedChartBorderColor: '#5391ff',
    deviceProcessBackgroundColor: '#262626',
    deviceProcessContentFontColor: 'rgb(226, 226, 226, 0.9)', // 设备和进程框里字体的颜色
    deviceProcessActiveFontColor: 'rgb(237, 237, 237, 0.9)', // 设备和进程下拉框选中范围的字体颜色
    deviceProcessNotActiveFontColor: 'rgb(150, 150, 150, 0.9)', // 设备和进程下拉框未选中范围的字体颜色
    searchBackgroundColor: 'rgb(255, 255, 255, 0.1)', // 搜索框的背景颜色
    tableBorderColor: 'rgb(255, 255, 255, 0.1)', // border分割线颜色
    backIconBackgroundColor: 'rgb(255, 255, 255)', // all sessions页面返回按钮背景色
    arrowUnexpandedBgColor: 'rgb(255, 255, 255, 0.5)', // Detail表格未展开时小三角背景色
    insightHeaderButtonBackgroundColor: 'rgb(48, 48, 48)',
    timelineAxisColor: '#3e4551',
    searchInputCaretColor: '#317AF7', // 搜索输入框光标颜色
    allSessionHeadBgColor: 'rgba(255, 255, 255, 0.05)', // session列表页头部背景色
    agentWelcomeCardBackgroundColor: 'rgba(255, 255, 255, 0.05)',
    deviceProcessActiveBackgroundColor: '#313131', // 设备和进程下拉框选中范围的背景颜色
    placeholderFontColor: 'rgb(100, 100, 100)',
    unitTagInfoBackgroundColor: 'rgb(255, 255, 255, 0.15)', // 泳道tag背景色
    tooltipBGColor: '#272c34',
    chartWrongBGColor: 'rgba(217, 72, 56, 0.1)',
    systemEventColor: '#DB6B42', // 系统事件背景色
    thumbIconBackgroundColor: 'rgb(171, 171, 171)', // 推荐入口主图标背景色
    thumbEntranceBackgroundColor: 'rgb(64, 64, 64)', // 推荐入口面板背景颜色
    toolTipBackgroundColor: 'rgb(64, 64, 64)', // 推荐入口面板背景颜色
    toolTipShadowColor: 'rgb(0 0 0 / 10%)',
    shadowBackgroundColor: 'rgb(18, 18, 18)',
    timeMakerListToolTipBackgroundColor: '#1A1A1A',
    enclosureBorder: 'rgb(131, 131, 131)', // 封闭框线颜色，如：Input 组件、Radio 按钮
    controllerBarBackgroundColor: 'rgb(66, 66, 66)', // Detail区域上方控制栏背景色
    activeButtonBackgroundColor: 'rgb(255, 255, 255)', // 活动按钮背景色
    disableButtonBackgroundColor: 'rgb(127, 127, 127)', // 失效按钮背景色
    fpsPointersColor: 'rgb(255, 255, 255, 0.7)', // FPS指针
    fpsTextColor: 'rgb(255, 255, 255)', // FPS文字颜色
    fpsColor: 'rgb(255, 255, 255, 0.5)', // FPS本身颜色
    categories: {
        system: { name: 'System', color: '#5291ff', background: '#3a404d' }, // 系统分类标签颜色与背景色
        arkTS: { name: 'ArkTS', color: '#5ba854', background: '#3b423a' }, // JS分类标签颜色与背景色
        NAPI: { name: 'NAPI', color: '#db6b42', background: '#5b504c' }, // NAPI分类标签颜色与背景色
        native: { name: 'Native', color: '#db6b42', background: '#5b504c' }, // Native分类标签颜色与背景色
    },
    searchIconBackgroundColor: 'rgb(255, 255, 255)', // 全部session页搜索按钮背景色
    cancelIconBackgroundColor: 'rgb(255, 255, 255, 0.6)', // 全部session页删除按钮背景色
    dividerColor: '#000',
    closeDragContainerBG: 'rgba(255,255,255,0.11)',
    tooltipFontColor: 'rgba(255, 255, 255, .9)',
    tooltipBoxShadow: '0 4px 16px 0 rgba( 0, 0, 0, .32 )',
    colorPalette: {
        slateblue: '#6259DE',
        royalblue: '#317AF7',
        skyblue: '#4694C2',
        turquoise: '#5AADA0',
        olivedrab: '#5BA854',
        yellowgreen: '#86AD53',
        gold: '#D1A738',
        orange: '#E08C3A',
        coral: '#DB6B42',
        orangered: '#D94838',
        palevioletred: '#CC5286',
        mediumorchid: '#8C55C2',
        transparentMask: '#4E4E4E',
        pink: '#E67C92',
        otherColor: 'rgb(80, 80, 80)',
        yellow: '#b09239',
        coralRed: '#FF5432',
        deepBlue: '#0062DC',
        tealGreen: '#2DB47C',
        aquaBlue: '#3DCFD4',
        vividBlue: '#0077FF',
        vividRed: '#EC2829',
        raspberryPink: '#EC4F83',
        skyBlue: '#3DB6FC',
        royalPurple: '#6D47F5',
        sunsetOrange: '#F69226',
        amethystPurple: '#BD45E8',
        limeGreen: '#81BA06',
    },
    grayscaleColor: '#4E4E4E',
    filterColor: 'rgba(255, 255, 255, .9)', // 过滤模块字体颜色
    filterTipColor: 'rgba(255, 255, 255, .6)', // 过滤模块提示文字字体颜色
    multiSelectBgColor: 'rgba(255, 255, 255, .15)', // 多选列表item背景色
    multiSelectUnCheckedBgColor: 'rgba(0, 0, 0, .4)', // 多选列表item单选框未选中背景色
    multiSelectUnCheckedBorderColor: 'rgba(255, 255, 255, .4)', // 多选列表item单选框未选中边框色
    timeDiffBackgroundColor: 'rgba(33, 33, 33, 0.82)', // 时间差背景颜色
    timeDiffPictureColor: 'rgb(150, 150, 150, 0.9)', // 时间差箭头颜色
    filterIconColor: 'rgb(255, 255, 255)', // 过滤图标背景色
    filterSelectActiveBgColor: '#313131',
    searchContainerBorder: 'none',
    searchHeaderBgColor: 'rgb(255, 255, 255, 0.1)',
    listItemHoverColor: 'rgb(255, 255, 255, 0.7)',
    flagListSelectedColor: 'rgb(91,94,103)',
    flagListHoverColor: 'rgb(79, 79, 79)',
    colorSelectedBorder: '1px solid #FFFFFF',
    frameExpectColor: 'rgb(255, 255, 255, 0.9)',
    schedulingLineColor: 'rgb(255, 255, 255, 0.7)',
    buttonColor: {
        enableClickColor: 'rgba(255,255,255,0.6)',
        disableClickColor: 'rgba(255,255,255,0.2)',
        emphasizeColor: '#5291FF',
        suspendBGColor: 'rgba(255,255,255,0.1)',
        unSuspendBGColor: 'transparent',
    },
    otherColor: 'grey',
    switchOpen: '#5291FF',
    switchClose: '#1A1A1A',

    primaryColor: '#0077FF',
    primaryColorLight1: '#3375b9',
    primaryColorLight2: '#2a598a',
    primaryColorLight3: '#213d5b',
    primaryColorLight4: '#1d3043',
    primaryColorLight5: '#18222c',
    primaryColorLight6: '#3291FE',
    primaryColorDark: '#66b1ff',
    primaryColorHover: '#3291FE',
    primaryColorDisabled: '#052D5A',
    successColor: '#24AB36',
    successColorLight1: '#4e8e2f',
    successColorLight2: '#3e6b27',
    successColorLight3: '#2d481f',
    successColorLight4: '#25371c',
    successColorLight5: '#1c2518',
    successColorDark: '#85ce61',
    warningColor: '#EBAF00',
    warningColorLight1: '#a77730',
    warningColorLight2: '#7d5b28',
    warningColorLight3: '#533f20',
    warningColorLight4: '#3e301c',
    warningColorLight5: '#292218',
    warningColorDark: '#ebb563',
    dangerColor: '#E32020',
    dangerColorLight1: '#b25252',
    dangerColorLight2: '#854040',
    dangerColorLight3: '#582e2e',
    dangerColorLight4: '#412626',
    dangerColorLight5: '#2b1d1d',
    dangerColorDark: '#f78989',
    infoColor: '#576372',
    infoColorLight1: '#6b6d71',
    infoColorLight2: '#525457',
    infoColorLight3: '#393a3c',
    infoColorLight4: '#2d2d2f',
    infoColorLight5: '#202121',
    infoColorDark: '#a6a9ad',
    majorColor: '#F97611',
    majorColorLight1: '#FBA25D',
    bgColor: '#1f2329',
    bgColorDark: '#181b20',
    bgColorGrey: '#343a43',
    bgColorLight: '#2a2f37',
    bgColorLighter: '#343a43',
    bgColorDisabled: '#272c33',
    bgColorCommon: '#2A2F37',
    textColor: '#D2DCE9',
    textColorPrimary: '#ffffff',
    textColorSecondary: '#d3dce9',
    textColorTertiary: '#8D98AA',
    textColorFourth: '#ffffff',
    textColorPlaceholder: '#626d7c',
    textColorPlaceholderLight: '#272c33',
    textColorDisabled: '#6C6E72',
    textColorMenu: '#F4F6FA',
    borderColor: '#363c46',
    borderColorLight: '#3e4551',
    borderColorLighter: '#576372',
    borderColorExtraLight: '#fffefe',
    borderColorDisabled: '#626D7C',
    boxShadow: '0px 12px 32px 4px rgba(0, 0, 0, 0.36), 0px 8px 20px rgba(0, 0, 0, 0.72)',
    boxShadowLight: '0px 0px 12px rgba(0, 0, 0, 0.72)',
    boxShadowLighter: '0px 0px 6px rgba(0, 0, 0, 0.72)',
    boxShadowDark: '0px 16px 48px 16px rgba(0, 0, 0, 0.72), 0px 12px 32px #000000, 0px 8px 16px -8px #000000',
    boxShadowDropDown: '0 8px 16px 0 rgba(0, 0, 0, 0.10)',
    maskColor: 'rgb(0, 0, 0, 0.55)',
    maskColorExtraLight: 'rgba(0, 0, 0, 0.3)',
    borderColorHover: '#6C6E72',
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
    icon: '#D1D1D1',
    iconColor: '#D1D1D1',
    tableTextColor: '#8D98AA',
    radioSelectedColor: '#007AFF',
    rankBackgroudColor: '#2A2F37',
    contextMenuBgColor: '#2a2f37',
    summaryChartBgColor: '#576372',
    rankContainerBackgroudColor: '#181B20',
    unitInfoTextColor: '#BCBCBC',
    scrollBarWidth: 7,
    splitLineColor: '#3C3C3C',
    selectedBgColor: '#343A43',
    // 把新加的字段同时加到light.ts文件、以及emotion.d.ts文件...........
};
