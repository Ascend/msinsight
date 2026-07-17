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
import React, { ComponentType, useRef, forwardRef } from 'react';
import styled from '@emotion/styled';
import { ReactComponent as HelpSvg } from './img/help.svg';
import { ReactComponent as PinSvg } from './img/icons_dark_normal_ascendinsight_pin.svg';
import { ReactComponent as UnPinSvg } from './img/icons_dark_normal_ascendinsight_pin_line.svg';
import { ReactComponent as StartSvg } from './img/icons_dark_normal_mindstudioinsight_start.svg';
import { ReactComponent as CaretSvg } from './img/caret.svg';
import CareRight from './img/caret_right.svg';
import { ReactComponent as SearchIconSvg } from './img/search.svg';
import { ReactComponent as PlusIconSvg } from './img/plus.svg';
import { ReactComponent as MinusIconSvg } from './img/minus.svg';
import { ReactComponent as FilterIconSvg } from './img/filter.svg';
import { ReactComponent as FlagIconSvg } from './img/flag.svg';
import { ReactComponent as LinkIconSvg } from './img/link.svg';
import { ReactComponent as ResetIconSvg } from './img/reset.svg';
import { ReactComponent as BulbSvg } from './img/bulb.svg';
import { ReactComponent as ArrowDownSvg } from './img/arrow_down.svg';
import { ReactComponent as EyeCloseOtuLineSvg } from './img/eye_close_outlined.svg';
import { ReactComponent as ArrowUpSvg } from './img/arrow_up.svg';
import { ReactComponent as ColumnFilterSvg } from './img/column_filter.svg';
import { ReactComponent as ExpandSvg } from './img/expand.svg';
import { ReactComponent as ExpandRightSvg } from './img/expand_right.svg';
import { ReactComponent as ImportDataSvg } from './img/import_data.svg';
import { ReactComponent as RefreshSvg } from './img/refresh.svg';
import { ReactComponent as FileSvg } from './img/file.svg';
import { ReactComponent as FolderSvg } from './img/folder.svg';
import { ReactComponent as AlarmSvg } from './img/alarm.svg';
import { ReactComponent as DeleteSvg } from './img/delete.svg';
import { ReactComponent as AddSvg } from './img/add.svg';
import { ReactComponent as LocalImportSvg } from './img/local_import.svg';
import { ReactComponent as LangZhSvg } from './img/lang_zh.svg';
import { ReactComponent as LangEnSvg } from './img/lang_en.svg';
import { ReactComponent as SetSvg } from './img/set.svg';
import { ReactComponent as FullTextSvg } from './img/full_text.svg';
import { ReactComponent as FullTextActiveSvg } from './img/full_text_active.svg';

import { CopyOutlined } from '@ant-design/icons';
import { compareColors } from '../utils/Color';
import { useTheme } from '@emotion/react';

interface ISVGProps extends React.SVGProps< SVGSVGElement > {
    active?: boolean;
    disabled?: boolean;
    theme?: 'dark' | 'light';
}

interface IIconProps extends ISVGProps {
    type?: string;
    iconChangeList?: any[];
    svgElement?: React.FunctionComponent<React.SVGProps< SVGSVGElement > & { title?: string }>;
}

interface IIconDivProps extends React.DetailedHTMLProps<React.HTMLAttributes<HTMLDivElement>, HTMLDivElement> {
    svgElement?: string;
    url?: string;
    float?: string;
    height?: string;
    width?: string;
}

const StyledIcon = styled.div<{ color?: string;disabled?: boolean }>`
    display: inline-flex;
    align-items: center;
    color: inherit;
    font-style: normal;
    line-height: 0;
    text-align: center;
    text-transform: none;
    vertical-align: text-top;
    text-rendering: optimizelegibility;
    -webkit-font-smoothing: antialiased;
    > svg path {
        fill: ${(p): string | null => {
        const { disabled, color, theme } = p;
        if (disabled) {
            return theme.disableButtonBackgroundColor;
        } else if (color !== null && color !== undefined) {
            return color;
        } else {
            return null;
        }
    }};
    }
`;

export function Icon({ type = '', svgElement, className = '', theme, color, active, disabled, style, ...restProp }: IIconProps): JSX.Element {
    const Svg: ComponentType | undefined = svgElement;

    return <StyledIcon disabled={disabled} color={color} style={{ ...(style ?? {}) }} className={className}>
        {Svg && (
            <Svg
                width={16}
                height={16}
                {...restProp}
            />
        )}
    </StyledIcon>;
}

export function DrawIcon({ iconChangeList, svgElement, className = '', theme, color, active, disabled, style, ...restProp }: IIconProps): JSX.Element {
    const Svg: ComponentType | undefined = svgElement;
    const curTheme = useTheme().mode;
    const changeList: any[] = iconChangeList ?? [];
    const svgRef = useRef<SVGSVGElement>(null);
    setTimeout(() => {
        if (svgRef.current) {
            // 获取 SVG 元素中的所有路径（path）元素
            const paths = svgRef.current.querySelectorAll('*') as NodeListOf<SVGPathElement>;
            paths.forEach((path: SVGPathElement) => {
                changeList.forEach((changeItem: { dark: any; light: any }) => {
                    const { dark, light } = changeItem;
                    if (compareColors(getComputedStyle(path).fill, dark) || compareColors(getComputedStyle(path).fill, light)) {
                        if (curTheme === 'light') {
                            path.style.fill = light;
                        } else {
                            path.style.fill = dark;
                        }
                    }

                    if (compareColors(getComputedStyle(path).stroke, dark) || compareColors(getComputedStyle(path).stroke, light)) {
                        if (curTheme === 'light') {
                            path.style.stroke = light;
                        } else {
                            path.style.stroke = dark;
                        }
                    }
                });
            });
        }
    });

    // 定义Svg组件，接受ref
    const SvgREF = forwardRef<SVGSVGElement, React.SVGProps<SVGSVGElement>>((props: React.SVGProps<SVGSVGElement> = {}, ref: React.Ref<SVGSVGElement> = null) => {
        return Svg ? <Svg ref={ref} {...props} /> : null;
    });

    SvgREF.displayName = 'SvgREF';

    return <StyledIcon disabled={disabled} color={color} style={{ ...(style ?? {}) }} className={className}>
        {Svg && (
            <SvgREF
                width={16}
                height={16}
                {...restProp}
                ref={svgRef}
            />
        )}
    </StyledIcon>;
}

const IconDiv = styled.div<{ svgElement?: string;url?: string;height?: string;width?: string }>`
    display: inline-flex;
    align-items: center;
    color: inherit;
    font-style: normal;
    line-height: 0;
    text-align: center;
    text-transform: none;
    vertical-align: text-top;
    text-rendering: optimizelegibility;
    -webkit-font-smoothing: antialiased;
    height:16px;
    width: 16px;
    background:center no-repeat url("${(p): string => {
        return p?.svgElement?.toString() ?? '';
    }}") ;
            background-size: ${(p): string => `${p.height ?? '16px'} ${p.width ?? '16px'}`};
`;

export function HelpIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={HelpSvg} color={useTheme().iconColor} {...props }/>;
}

export function PinIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={PinSvg} active={true} {...props }/>;
}

export function UnPinIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={UnPinSvg} active={true} {...props }/>;
}

export function CaretDownIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={CaretSvg} color={useTheme().iconColor} {...props }/>;
}

export function CaretRightIcon(props: IIconDivProps): JSX.Element {
    return <IconDiv svgElement={CareRight} {...props } />;
}

export function FlagIcon(props: ISVGProps): JSX.Element {
    const iconChangeList = [{
        dark: '#D1D1D1',
        light: '#4E5865',
    }];
    return <DrawIcon svgElement={FlagIconSvg} iconChangeList={iconChangeList} {...props } />;
}
export function FilterIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={FilterIconSvg} color={useTheme().iconColor} {...props } />;
}
export function SearchIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={SearchIconSvg} color={useTheme().iconColor} {...props } />;
}
export function LinkIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={LinkIconSvg} color={useTheme().iconColor} {...props } />;
}
export function PlusIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={PlusIconSvg} {...props } />;
}
export function MinusIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={MinusIconSvg} {...props } />;
}
export function ResetIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={ResetIconSvg} {...props } />;
}
export function StartIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={StartSvg} {...props }/>;
}
export function BulbIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={BulbSvg} {...props }/>;
}
export function ArrowUpIcon(props: ISVGProps): JSX.Element {
    const iconChangeList = [{
        dark: '#2A2F37',
        light: '#F4F6FA',
    }];
    return <DrawIcon svgElement={ArrowUpSvg} iconChangeList={iconChangeList} width={92} {...props }/>;
}
export function ArrowDownIcon(props: ISVGProps): JSX.Element {
    const iconChangeList = [{
        dark: '#2A2F37',
        light: '#F4F6FA',
    }];
    return <DrawIcon svgElement={ArrowDownSvg} iconChangeList={iconChangeList} width={92} {...props }/>;
}

export function CopyOutlinedIcon({ style }: { style?: React.CSSProperties }): JSX.Element {
    return <CopyOutlined style={style} />;
}

export function ColumnFilterIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={ColumnFilterSvg } {...props }/>;
}
export function ExpandIcon(props: ISVGProps): JSX.Element {
    const { style = {}, ...restProps } = props;
    return <Icon svgElement={ExpandSvg} style={{ verticalAlign: 'middle', margin: '0 10px 0 0', ...style }} {...restProps} />;
}
export function CollapseIcon(props: ISVGProps): JSX.Element {
    const { style = {}, ...restProps } = props;
    return <Icon svgElement={ExpandRightSvg} style={{ verticalAlign: 'middle', margin: '-5px 5px 0 -2px', ...style }} {...restProps} />;
}

export function AlarmIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={AlarmSvg} {...props } style={{ marginTop: '2px' }}/>;
}

export function ImportDataIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={ImportDataSvg} {...props }/>;
}
export function RefreshIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={RefreshSvg} {...props }/>;
}
export function FileIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={FileSvg} {...props }/>;
}
export function FolderIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={FolderSvg} {...props }/>;
}
export function DeleteIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={DeleteSvg} {...props }/>;
}
export function AddIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={AddSvg} {...props }/>;
}
export function LocalImportIcon(props: ISVGProps): JSX.Element {
    const iconChangeList = [{
        dark: '#D1D1D1',
        light: '#595959',
    }, {
        dark: 'rgba(0,0,0,0)',
        light: '#EBEBEB',
    }];
    return <DrawIcon svgElement={LocalImportSvg} iconChangeList={iconChangeList} {...props }/>;
}
export function LangZhIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={LangZhSvg } {...props }/>;
}
export function LangEnIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={LangEnSvg} {...props }/>;
}

export function SetIcon(props: ISVGProps): JSX.Element {
    return <Icon svgElement={SetSvg} {...props }/>;
}

// 大小写匹配
export function CaseIcon(props: ISVGProps): JSX.Element {
    const { active, ...restProps } = props;
    return <Icon type={`case${active ? 'Active' : ''}`} {...restProps}/>;
}

// 全文匹配
export function FullTextIcon(props: ISVGProps): JSX.Element {
    const { active, ...restProps } = props;
    if (active) {
        return <Icon svgElement={FullTextActiveSvg} {...restProps}/>;
    } else {
        return <Icon svgElement={FullTextSvg} {...restProps}/>;
    }
}

export function EyeCloseOtuLine(props: ISVGProps): JSX.Element {
    return <Icon svgElement={EyeCloseOtuLineSvg} {...props }/>;
}
