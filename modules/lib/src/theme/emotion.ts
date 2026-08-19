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
import '@emotion/react';

declare module '@emotion/react' {

    interface ColorPalette {
        slateblue: string;
        royalblue: string;
        skyblue: string;
        turquoise: string;
        olivedrab: string;
        yellowgreen: string;
        gold: string;
        orange: string;
        coral: string;
        orangered: string;
        palevioletred: string;
        mediumorchid: string;
        pink: string;
        otherColor: string;
        yellow: string;
        coralRed: string;
        deepBlue: string;
        tealGreen: string;
        aquaBlue: string;
        vividBlue: string;
        vividRed: string;
        raspberryPink: string;
        skyBlue: string;
        royalPurple: string;
        sunsetOrange: string;
        amethystPurple: string;
        limeGreen: string;
    }
    export interface Theme {
        mode: 'light' | 'dark';
        backgroundColor: string; // 主界面背景色
        timeInsightIcon: string; // time Insight 图标颜色
        allocationInsightIcon: string; // allocation Insight 图标颜色
        contentBackgroundColor: string; // 区域内容背景色
        buttonBackgroundColor: string; // 模板、按钮背景色
        okBackgroundColor: string; // ok按钮背景色
        selectBackgroundColor: string; // session选中背景色
        cardHeadBackgroundColor: string; // 卡片表头背景色
        fontColor: string; // 字体颜色
        disabledFontColor: string; // 被禁用组件字体颜色
        tableHeadFontColor: string; // 表格表头字体颜色
        subtitleColor: string; // session小字体颜色
        solidLine: string; // border分割线颜色
        buttonFontColor: string; // 模板、按钮字体颜色
        svgBackgroundColor: string; // svg设备、homePage图片颜色
        svgPlayBackgroundColor: string; // svg录制、暂停、删除等图片颜色
        scrollbarColor: string; // 滚动条颜色
        tableRowSelect: string; // 卡片表格tr选中色
        devicePullDown: string; // 设备下拉按钮颜色
        switchIconColor: string; // 模板下拉按钮颜色
        selectedChartColor: string; // 泳道选择区域左右两边背景色
        frameRelativeLineColor: string; // frame 连线颜色
        selectJankColor: string; // 异常帧选中之后的边框颜色
        selectNoJankColor: string; // 正常帧选中之后的边框颜色
        selectedChartBackgroundColor: string; // 选中泳道颜色
        selectedChartBorderColor: string; // 选中泳道边框色
        templateBackgroundColor: string; // 模板图标背景色
        templateSVGFillColor: string; // 模板svg图填充色
        selectedTemplateSVGFillColor: string; // 选中模板svg图填充色
        deviceProcessBackgroundColor: string; // 设备进程下拉框背景色
        deviceProcessContentFontColor: string; // 设备和进程框里字体的颜色
        deviceProcessActiveFontColor: string; // 设备和进程下拉框选中范围的字体颜色
        deviceProcessNotActiveFontColor: string; // 设备和进程下拉框未选中范围的字体颜色
        searchBackgroundColor: string; // 搜索框的背景颜色
        tableBorderColor: string; // 表格行分界线颜色
        backIconBackgroundColor: string; // all sessions页面返回按钮背景色
        arrowUnexpandedBgColor: string; // Detail表格未展开时小三角背景色
        insightHeaderButtonBackgroundColor: string; // insight 启动/停止按钮背景颜色
        timelineAxisColor: string; // 时间轴颜色
        searchInputCaretColor: string; // 搜索输入框光标颜色
        allSessionHeadBgColor: string; // session列表页头部背景色
        agentWelcomeCardBackgroundColor: string; // InsightBot 欢迎页卡片背景色
        deviceProcessActiveBackgroundColor: string; // 设备和进程下拉框选中范围的背景颜色
        placeholderFontColor: string;
        unitTagInfoBackgroundColor: string; // 泳道tag背景色
        tooltipBGColor: string; // tooltip背景颜色
        chartWrongBGColor: string; // 泳道失败chart背景颜色
        systemEventColor: string; // 系统事件背景色
        thumbIconBackgroundColor: string; // 推荐入口主图表背景色
        thumbEntranceBackgroundColor: string; // 推荐入口背景面板颜色
        toolTipBackgroundColor: string; // 推荐入口背景面板颜色
        toolTipShadowColor: string; // tooltip 阴影颜色
        timeMakerListToolTipBackgroundColor: string; // 时间轴标记列表tooltip背景颜色
        shadowBackgroundColor: string; // 阴影颜色
        enclosureBorder: string; // 封闭框线颜色，如：Input 组件、Radio 按钮
        controllerBarBackgroundColor: string; // Detail区域上方控制栏背景色
        activeButtonBackgroundColor: string; // 活动按钮背景色
        disableButtonBackgroundColor: string; // 失效按钮背景色
        fpsPointersColor: string; // FPS指针
        fpsTextColor: string; // FPS文字颜色
        fpsColor: string; // FPS本身颜色
        categories: {
            system: { name: 'System'; color: string; background: string }; // 系统分类标签颜色与背景色
            arkTS: { name: 'ArkTS'; color: string; background: string }; // JS分类标签颜色与背景色
            NAPI: { name: 'NAPI'; color: string; background: string }; // NAPI分类标签颜色与背景色
            native: { name: 'Native'; color: string; background: string }; // Native分类标签颜色与背景色
        };
        searchIconBackgroundColor: string; // 全部session页搜索按钮背景色
        cancelIconBackgroundColor: string; // 全部session页删除按钮背景色
        dividerColor: string; // 界面面板分割线颜色
        closeDragContainerBG: string; // 关闭按钮背景色
        tooltipFontColor: string; // ToolTip字体颜色
        tooltipBoxShadow: string; // ToolTip盒子阴影
        // 基于DevEco2.0设计规范的推荐用色
        colorPalette: ColorPalette & {
            transparentMask: string;
        };
        grayscaleColor: string; // 泳道元素的灰度颜色
        filterColor: string; // 过滤模块字体颜色
        filterTipColor: string; // 过滤模块提示文字字体颜色
        multiSelectBgColor: string; // 多选列表item背景色
        multiSelectUnCheckedBgColor: string; // 多选列表item单选框未选中背景色
        multiSelectUnCheckedBorderColor: string; // 多选列表item单选框未选中边框色
        timeDiffBackgroundColor: string; // 时间差背景颜色
        timeDiffPictureColor: string; // 时间差箭头颜色
        filterIconColor: string; // 过滤图标背景色
        filterSelectActiveBgColor: string; // 过滤选择框选中背景色
        searchContainerBorder: string; // 全部session列表页搜索框边框色
        searchHeaderBgColor: string; // 全部session列表页搜索框背景色
        listItemHoverColor: string; // 无序列表hover时的字体颜色
        flagListSelectedColor: string; // 事件轴列表选中颜色
        flagListHoverColor: string; // 事件轴列表悬浮颜色
        colorSelectedBorder: string; // 颜色块选择边框
        schedulingLineColor: string; // 时延线颜色
        frameExpectColor: string; // ExpectStart ExpectEnd颜色
        buttonColor: {
            enableClickColor: string; // 可点击颜色
            disableClickColor: string; // 禁用颜色
            emphasizeColor: string; // 强调颜色
            suspendBGColor: string; // 悬浮背景色
            unSuspendBGColor: string;
        }; // 按钮各状态颜色，及背景色
        otherColor: string;
        switchOpen: string;
        switchClose: string;
        primaryColor: string; // 主色/品牌色
        primaryColorLight1: string;
        primaryColorLight2: string;
        primaryColorLight3: string;
        primaryColorLight4: string;
        primaryColorLight5: string;
        primaryColorLight6: string;
        primaryColorDark: string;
        primaryColorHover: string;
        primaryColorDisabled: string;
        successColor: string; // 成功色
        successColorLight1: string;
        successColorLight2: string;
        successColorLight3: string;
        successColorLight4: string;
        successColorLight5: string;
        successColorDark: string;
        warningColor: string; // 报警色
        warningColorLight1: string;
        warningColorLight2: string;
        warningColorLight3: string;
        warningColorLight4: string;
        warningColorLight5: string;
        warningColorDark: string;
        dangerColor: string; // 错误色
        dangerColorLight1: string;
        dangerColorLight2: string;
        dangerColorLight3: string;
        dangerColorLight4: string;
        dangerColorLight5: string;
        dangerColorDark: string;
        infoColor: string; // 信息色
        infoColorLight1: string;
        infoColorLight2: string;
        infoColorLight3: string;
        infoColorLight4: string;
        infoColorLight5: string;
        infoColorDark: string;
        majorColor: string; // 主要色
        majorColorLight1: string;
        bgColor: string; // 背景色
        bgColorDark: string;
        bgColorGrey: string;
        bgColorLight: string;
        bgColorLighter: string;
        bgColorDisabled: string; // 组件禁用后背景色
        bgColorCommon: string; // 背景常用色
        textColor: string; // 文字常用色
        textColorPrimary: string; // 文字主色
        textColorSecondary: string; // 文字次级色
        textColorTertiary: string; // 文字三级色
        textColorFourth: string; // 文字四级色
        textColorPlaceholder: string;
        textColorPlaceholderLight: string;
        textColorDisabled: string; // 文字禁用色
        textColorMenu: string;
        borderColor: string; // 边框色
        borderColorLight: string;
        borderColorLighter: string;
        borderColorExtraLight: string;
        borderColorDisabled: string;
        boxShadow: string; // 阴影色
        boxShadowLight: string;
        boxShadowLighter: string;
        boxShadowDark: string;
        boxShadowDropDown: string;
        maskColor: string; // 遮罩层色
        maskColorExtraLight: string;
        borderColorHover: string;
        fontSizeExtraLarge: string; // 超大字号
        fontSizeLarge: string; // 大字号
        fontSizeMedium: string; // 中字号
        fontSizeBase: string; // 基础字号
        fontSizeSmall: string; // 小字号
        fontSizeExtraSmall: string; // 超小字号
        fontFamily: string;
        borderRadiusBase: string; // 基础圆角
        borderRadiusSmall: string;
        borderRadiusRound: string;
        borderRadiusCircle: string;
        pagePadding: string;
        icon: string; // 图标颜色
        iconColor: string; // 图标颜色2
        tableTextColor: string; // 表格颜色
        radioSelectedColor: string; // 单选框选中填充颜色
        rankBackgroudColor: string;
        contextMenuBgColor: string; // 右键菜单背景色
        summaryChartBgColor: string; // 泳道预览图颜色
        rankContainerBackgroudColor: string;
        unitInfoTextColor: string;
        scrollBarWidth: number; // 滚动条宽度
        splitLineColor: string; // 分割线颜色
        selectedBgColor: string; // 项目选中背景色
    }
}
